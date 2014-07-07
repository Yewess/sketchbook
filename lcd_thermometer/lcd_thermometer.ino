#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <stdlib.h>
#include <LiquidCrystal.h>
#include <OneWire.h>
#include "MaxDS18B20.h"
#include "MovingAvg.h"
#include "TimedEvent.h"
#include "StateMachine.h"

/*
Theory of operation:

Every ~8-seconds, WDT wakes up device:
    * Power up OWB A & B
    * Setup max18b20's
        * If none found:
            * Turn on LCD
            * LCD write "connect max18b20"
            * Repeat for each OWB
    * Begin temperature conversion
    * If button is pressed:
        * Switch on status LED, wait for button release
        * Turn on LCD
        * LCD write "measuring temperature"
        * Set UI flag
    * else:
        * Switch on status LED
    * Wait remainder of time for temperature conversion
    * Read temperature
    * Update SMA's if necessary
    * Switch off status LED
    * If UI flag not set:
        * Go back to sleep
    * else:
        * Show display indicated by encoder value
        * If 1-min goes by w/o encoder change
            * Go back to sleep
        * If button pressed:
            * Go back to sleep
        * Repeat above
*/

//#define NODEBUG
#ifndef NODEBUG
    #define DEBUG
    #define D(WHAT) Serial.print(WHAT)
    #define DL(WHAT) Serial.println(WHAT)
#else
    #define D(WHAT) if (false)
    #define DL(WHAT) if (false)
#endif // NODEBUG

static const struct DispState {
    static const uint8_t now = 0;
    static const uint8_t one = 1;
    static const uint8_t four = 2;
    static const uint8_t snooze = 3;
} DispState;

static const struct TransState {
    static const uint8_t enter = 0;
    static const uint8_t hold = 1;
    static const uint8_t exit = 2;
} TransState;

static const struct LcdBlState {
    static const uint8_t up = 0;
    static const uint8_t down = 2;
    static const uint8_t equal = 3;
    static const uint8_t on = 4;
    static const uint8_t off = 5;
} LcdBlState;

#ifdef DEBUG
static const uint8_t PIN_SERIAL_RX = 0;
static const uint8_t PIN_SERIAL_TX = 1;
static const uint32_t SERIAL_BAUD = 115200;
#endif // DEBUG

// Peripheral Power
static const uint8_t PIN_LCD_MOSFET = 12;
static const uint8_t PIN_ARF_MOSFET = A3;

// One Wire Bus
static const uint8_t PIN_OWB_A = 11;
static const uint8_t PIN_OWB_B = 10;

// IIC
static const uint8_t PIN_IIC_SCL = A5;
static const uint8_t PIN_IIC_SDA = A4;

// Battery Sense.
static const uint8_t PIN_BAT_SENSE = A0;

// Status indicator
static const uint8_t PIN_STATUS_LED = 13;

// LCD Interface
static const uint8_t PIN_LCD_RS = A1;
static const uint8_t PIN_LCD_EN = A2;
static const uint8_t PIN_LCD_DA4 = 5;
static const uint8_t PIN_LCD_DA5 = 6;
static const uint8_t PIN_LCD_DA6 = 7;
static const uint8_t PIN_LCD_DA7 = 8;
static const uint8_t PIN_LCD_BL = 9; // pwm

// LCD Details
static const uint8_t LCD_COLS = 16;
static const uint8_t LCD_ROWS = 2;

// One Wire Bus
static const uint8_t MAXRES = 10;
static const int16_t FAULTTEMP = 12345;

// Timing data
static const uint16_t WDT8SEC           = 7806; // 8 sec avg WDT osc time
static const uint32_t SLEEPTIME         = 600000; // 10 minutes
static const uint8_t SLEEP_CYCLES       = SLEEPTIME / WDT8SEC;
static const Millis LCD_UPDATE_INTERVAL = 1000; // < 1000ms is unreadable
static const Millis TEMP_SAMPLE_INTVL   = 600000; // 10 minutes (in ms)
static const Millis TEMP_SMA_WINDOW_S   = 3600000; // 1 hour SMA (short)
static const Millis TEMP_SMA_WINDOW_L   = 14400000; // 4 hour SMA (long)
                                  // (TEMP_SMA_WINDOW_S / TEMP_SAMPLE_INTVL);
static const uint16_t SMA_POINTS_S = 6;
                                  // (TEMP_SMA_WINDOW_L / TEMP_SAMPLE_INTVL);
static const uint16_t SMA_POINTS_L = 24;
// Allow re-use of lcdBlStateTimer for transState
static const uint8_t LCD_BL_RAMP_DIVISIONS = 20;  // updates per second
                                 // 1000 / LCD_BL_RAMP_DIVISIONS;
const Millis LCD_BL_RAMP_INTERVAL = 50;  // ms intvl btwn ticks
// Durations, in ramp-div updates (ticks) for each stage
static const uint16_t LCD_BL_RAMP_TICKS = 20;  // 1-second in ticks
static const uint16_t LCD_BL_EQ_TICKS = 10; //  .5-seconds in ticks
static const uint16_t LCD_BL_ON_TICKS = 160;  // 8-seconds in ticks
static const uint16_t LCD_BL_OFF_TICKS = 0;  // zero-time

// max value for dispCycles
static const uint8_t MAX_DISPLAY_CYCLES = 3;  // now, one, four...before snooze
// Temperature unit
static const bool CELSIUS = false;
static const char DEGREE_CHAR = 223;

// OneWire bus and sensors
OneWire owb_local(PIN_OWB_A); // local
OneWire owb_remote(PIN_OWB_B); // remote
MaxDS18B20::MaxRom rom_local;
MaxDS18B20::MaxRom rom_remote;
MaxDS18B20 max_local(owb_local, rom_local);
MaxDS18B20 max_remote(owb_remote, rom_remote);

uint8_t lcdBlVal = 0;  // PWM value to set (0-255)
LiquidCrystal lcd(PIN_LCD_RS, PIN_LCD_EN,
                  PIN_LCD_DA4, PIN_LCD_DA5,
                  PIN_LCD_DA6, PIN_LCD_DA7);

// master clock
Millis current_time = 0;
uint32_t sleepAdvance = 0; // total sleepCycleCounter
volatile uint8_t sleepCycleCounter = 0;

// Temperature readings
int16_t now_local = FAULTTEMP;
int16_t now_remote = FAULTTEMP;
SimpleMovingAvg sma_local_s(SMA_POINTS_S);
SimpleMovingAvg sma_local_l(SMA_POINTS_L);
SimpleMovingAvg sma_remote_s(SMA_POINTS_S);
SimpleMovingAvg sma_remote_l(SMA_POINTS_L);

// serviced by main loop, transitions in transState
StateMachine dispState(sizeof(DispState) / sizeof(uint8_t),
                       DispState.now);  // data display state (now, 1h, 4h,, snooze)
uint8_t dispCycles = 0;  // number of times through dispState

// serviced by dispState, transitions in lcdBlState
StateMachine transState(sizeof(TransState) / sizeof(uint8_t),
                        TransState.enter);  // inner-display state (enter, hold, exit, next)
uint8_t lcdblStateCache(LcdBlState.equal);  // Cache during dispState cycle

// serviced by transState, transitions in lcdBLStateTimer
StateMachine lcdBlState(sizeof(LcdBlState) / sizeof(uint8_t),
                        LcdBlState.equal);    // backlight state (ramp up, ramp-down, hold-on, hold-off)

// inc dispTics every LCD_BL_RAMP_INTERVAL miliseconds
TimedEvent lcdBlStateTimer(current_time, LCD_BL_RAMP_INTERVAL, lcdbl_event_equal_f); // transition timer for lcdBlState
uint16_t lcdblTicks = 0;  // nr lcdBlStateTimer fires

//////////////////////// WTD sleep wakeup ISR
ISR(WDT_vect)
{
    sleepCycleCounter++;
}

void init_max(OneWire& owb,
              MaxDS18B20::MaxRom& rom,
              MaxDS18B20& max) {
    while (true) {
        if (MaxDS18B20::getMaxROM(owb, rom, 0)) {
            DL("Found device");
            max.setResolution(MAXRES);
            DL("Set resolution");
            if (!max.writeMem())
                DL(F("Error: Device setup failed!"));
            else {
                D("Setup device: "); rom.serial_print();
                break;
            }
        }
        DL(F("Waiting for device presence..."));
        delay(1000);
    }
}

inline void start_conversion(MaxDS18B20::MaxRom& rom,
                             MaxDS18B20& max) {
    D(F("Starting conversion on: ")); rom.serial_print();
    if (!max.startConversion())
        DL(F("Error: Device disappeared!"));
}

inline void wait_conversion(MaxDS18B20::MaxRom& rom,
                            MaxDS18B20& max) {
    D(F("Waiting for conversion on: ")); rom.serial_print();
    D(F("."));
    while (!max.conversionDone()) {
        digitalWrite(PIN_STATUS_LED,
                     !digitalRead(PIN_STATUS_LED));
        D(F("."));
        delay(100);
    }
    DL(F("done"));
}

inline int16_t get_temp(MaxDS18B20::MaxRom& rom,
                        MaxDS18B20& max) {
    if (!max.conversionDone())
        wait_conversion(rom, max);
    digitalWrite(PIN_STATUS_LED, HIGH);
    D(F("Reading memory from device: ")); rom.serial_print();
    if (max.readMem()) {
        if (CELSIUS) {
            D(F("Temperature: ")); D(max.getTempC()); DL(F("°C"));
            digitalWrite(PIN_STATUS_LED, LOW);
            return max.getTempC();
        } else {
            D(F("Temperature: ")); D(max.getTempF()); DL(F("°F"));
            digitalWrite(PIN_STATUS_LED, LOW);
            return max.getTempF();
        }
    }
    DL(F("Error reading memory!"));
    return FAULTTEMP;
}

inline int16_t sma_append(SimpleMovingAvg& sma,
                          uint16_t temp) {
    if (temp != FAULTTEMP) {
        return sma.append(temp);
    } else {
        DL(F("Not recording faulty temperature!"));
        return FAULTTEMP;
    }
}


void all_pins_up(void){
    power_all_enable();

    #ifdef DEBUG
        pinMode(PIN_SERIAL_RX, INPUT);
        pinMode(PIN_SERIAL_TX, OUTPUT);
        delay(100);
        Serial.begin(SERIAL_BAUD);
        DL(">>>DEBUG ENABLED<<<");
    #endif // DEBUG

    D(F("Setting status LED pin to output: ")); DL(PIN_STATUS_LED);
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, HIGH);

    D(F("Setup LCD pins:"));
    D(F(" RS- ")); D(PIN_LCD_RS);
    pinMode(PIN_LCD_RS, OUTPUT);
    D(F(" EN- ")); D(PIN_LCD_EN);
    pinMode(PIN_LCD_EN, OUTPUT);
    D(F(" D4- ")); D(PIN_LCD_DA4);
    pinMode(PIN_LCD_DA4, OUTPUT);
    D(F(" D5- ")); D(PIN_LCD_DA5);
    pinMode(PIN_LCD_DA5, OUTPUT);
    D(F(" D6- ")); D(PIN_LCD_DA6);
    pinMode(PIN_LCD_DA6, OUTPUT);
    D(F(" D7- ")); DL(PIN_LCD_DA7);
    pinMode(PIN_LCD_DA7, OUTPUT);

    D(F("Setting LCD Backlight pin (PWM) to output: ")); DL(PIN_LCD_BL);
    pinMode(PIN_LCD_BL, OUTPUT);
    analogWrite(PIN_LCD_BL, 0);  // PWM off

    D(F("Setting OneWire bus A pin to input: ")); DL(PIN_OWB_A);
    pinMode(PIN_OWB_A, INPUT);

    D(F("Setting OneWire bus B pin to input: ")); DL(PIN_OWB_B);
    pinMode(PIN_OWB_B, INPUT);

    D(F("Powering up peripherals on pin: ")); DL(PIN_LCD_MOSFET);
    D(F("Powering up peripherals on pin: ")); DL(PIN_ARF_MOSFET);
    pinMode(PIN_LCD_MOSFET, OUTPUT);
    pinMode(PIN_ARF_MOSFET, OUTPUT);
    pinMode(PIN_LCD_MOSFET, HIGH);
    pinMode(PIN_ARF_MOSFET, HIGH);
    // Devices need power-on time to settle
    delay(100);

    digitalWrite(PIN_STATUS_LED, LOW);

    DL(F("Initializing LCD"));
    lcd.begin(LCD_COLS, LCD_ROWS);
    lcd.noDisplay();
    lcd.clear();

    DL(F("Setting up A sensor"));
    // These will do inf. loop until sensor is present, give feedback
    init_max(owb_local, rom_local, max_local);
    init_max(owb_remote, rom_remote, max_remote);

    current_time = sleepAdvance * WDT8SEC + millis();
    D(F("Setting timer (+advance): "));
    D(current_time);
    D(F(" ("));
    D(sleepAdvance * WDT8SEC);
    DL(F(")"));
}

void all_pins_down(void) {
    DL(F("Going to sleep"));
    lcd.noDisplay();
    lcd.clear();
    #ifdef DEBUG
    lcd.print(F("Going to sleep"));
    lcd.setCursor(0,1);
    lcd.print(SLEEPTIME / 1000);
    lcd.print(F(" seconds"));
    lcd.display();
    Serial.flush();
    Serial.end();
    delay(500);  // time to read message
    #endif // DEBUG

    // Make sure these turn off first!
    digitalWrite(PIN_LCD_MOSFET, LOW);
    digitalWrite(PIN_ARF_MOSFET, LOW);
    for (uint8_t pin=0; pin < 18; pin++) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }
    power_all_disable();
}


/////////////////////// temp measurement

void temp_update(void) {
    if (max_local.conversionDone()) {
        D(F("Updating local reading..."));
        now_local = get_temp(rom_local, max_local);
        start_conversion(rom_local, max_local);
    }
    if (max_remote.conversionDone()) {
        D(F("Updating remote reading..."));
        now_remote = get_temp(rom_remote, max_remote);
        start_conversion(rom_remote, max_remote);
    }
}

/////////////////////// DispState servicers

void lcdbl_helper_init(uint8_t lcd_bl_state) {
    lcdblTicks = 0;
    switch (lcd_bl_state) {
        case LcdBlState::up:
            DL(F("Initializing backlight up transition"));
            lcdBlStateTimer.reInitIfNot(lcdbl_event_up_f);
            lcdBlVal = 0;
            break;
        case LcdBlState::equal:
            DL(F("Initializing backlight equal transition"));
            lcdBlStateTimer.reInitIfNot(lcdbl_event_equal_f);
            lcdBlVal = 255;
            break;
        case LcdBlState::on:
            DL(F("Initializing backlight on transition"));
            lcdBlStateTimer.reInitIfNot(lcdbl_event_on_f);
            lcdBlVal = 255;
            break;
        case LcdBlState::down:
            D(F("Backlight down ramp "));
            lcdBlStateTimer.reInitIfNot(lcdbl_event_down_f);
            lcdBlVal = 255;
        case LcdBlState::off:
            DL(F("Initializing backlight off transition"));
            lcdBlStateTimer.reInitIfNot(lcdbl_event_off_f);
            lcdBlVal = 0;
    }
}

void choose_updown(void) {  // ONCE per display change
    uint16_t sma_value = sma_remote_s.value();

    if (now_remote > sma_value) {
        lcdBlState.setCurrentId(LcdBlState::up);
    } else if (now_remote < sma_value) {
        lcdBlState.setCurrentId(LcdBlState::down);
    } else {
        lcdBlState.setCurrentId(LcdBlState::equal);
    }
    lcdblStateCache = lcdBlState.getCurrentId();
}

void disp_header(PGM_P header_p) {
    char c = pgm_read_byte(header_p);
    for (; c != 0; c = pgm_read_byte(header_p++))
         lcd.print(c);
}

void disp_temp(uint16_t temp) {
    const uint8_t ibuff_len = 6;
    char ibuff[ibuff_len] = {0};
    int16_t dig = temp / 100;
    uint8_t tenths = (temp % 100) / 10;
    uint8_t hundredths = (temp % 100) - (tenths * 10);
    // round up or down to nearest tenth
    if (hundredths > 5)
        tenths++;
    else
        tenths--;

    itoa(dig, ibuff, 10);
    uint8_t len = strnlen(ibuff, ibuff_len);
    for (uint8_t i=0; i < len; i++)
        lcd.print(ibuff);
    // only room for 3 digits + 1 decimal
    lcd.print('.');
    lcd.print(ibuff);
}

void disp_diff(uint16_t local, uint16_t remote) {
    const uint8_t ibuff_len = 6;
    char ibuff[ibuff_len] = {0};
    int16_t diff = remote - local;
    int16_t dig = diff / 100;
    uint8_t tenths = (abs(diff) % 100) / 10;
    // round up or down to nearest tenth
    if (tenths > 5)
        if (dig > 0)
            dig++;
        else
            dig--;

    itoa(abs(dig), ibuff, 10);
    uint8_t len = strnlen(ibuff, ibuff_len);
    if (dig > 0)
        lcd.print('+');
    else
        lcd.print('-');
    for (uint8_t i=0; i < len; i++)
        lcd.print(ibuff);
}

void disp_unit() {
    lcd.print(DEGREE_CHAR);
    if (CELSIUS)
        lcd.print('C');
    else
        lcd.print('F');
}

void now_f(StateMachine* state_machine) {
    /*
      Now L R *C  Diff
      000.0 000.0 +000
    */
    // copy header
    lcd.noDisplay();
    lcd.clear();
    disp_header(PSTR("Now L R CF  Diff"));
    lcd.setCursor(8,0);
    disp_unit();
    lcd.setCursor(0,1);
    disp_temp(now_local);
    lcd.print(' ');
    disp_temp(now_remote);
    lcd.print(' ');
    disp_diff(now_local, now_remote);
    lcd.display();
    // service state
    transState.service();
}

void one_f(StateMachine* state_machine) {
    /*
      1hA L R *C  Diff
      000.0 000.0 +000
    */
    const int16_t sma_local = sma_remote_s.value();
    const int16_t sma_remote = sma_remote_s.value();
    lcd.noDisplay();
    lcd.clear();
    disp_header(PSTR("1hA L R CF  Diff"));
    lcd.setCursor(8,0);
    disp_unit();
    lcd.setCursor(0,1);
    disp_temp(sma_local);
    lcd.print(' ');
    disp_temp(sma_remote);
    lcd.print(' ');
    disp_diff(now_local, now_remote);
    lcd.display();
    // service state
    transState.service();
}

void four_f(StateMachine* state_machine) {
   /*
     4hA L R *C  Diff
     000.0 000.0 +000
    */
    const int16_t sma_local = sma_remote_l.value();
    const int16_t sma_remote = sma_remote_l.value();
    lcd.noDisplay();
    lcd.clear();
    disp_header(PSTR("4hA L R CF  Diff"));
    lcd.setCursor(8,0);
    disp_unit();
    lcd.setCursor(0,1);
    disp_temp(sma_local);
    lcd.print(' ');
    disp_temp(sma_remote);
    lcd.print(' ');
    disp_diff(now_local, now_remote);
    lcd.display();
    // service state
    transState.service();
}

void snooze_f(StateMachine* state_machine) {
    D(F("Snoozing..."));
    all_pins_down();
    // ISR increments sleepCycleCounter on wakeup
    for (sleepCycleCounter=0; sleepCycleCounter < SLEEP_CYCLES;) {
        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
        sleep_enable();
        sleep_mode(); // ZZzzzzzzzz
        sleep_disable(); // Woke up, disable ISR
        sleepAdvance += WDT8SEC;
    }
    all_pins_up();
    D(F("...Woke up"));
    // Gather initial data
    // Take a while, use time initializing LCD
    start_conversion(rom_local, max_local);
    start_conversion(rom_remote, max_remote);
    lcd.begin(LCD_COLS, LCD_ROWS);
    lcd.clear();
    lcd.print("Waking up...");
    DL(F("Gathering post-snooze temp. readings"));
    wait_conversion(rom_local, max_local);
    wait_conversion(rom_remote, max_remote);
    temp_update();  // and start next conversion
    choose_updown(); // set up lcdBlState
    transState.setCurrentId(TransState::enter);
    dispState.setCurrentId(DispState::now);
    dispCycles = 0;
    // enter always only handles up or equal
    if ((lcdblStateCache != LcdBlState::up) ||
        (lcdblStateCache != LcdBlState::equal))
        lcdbl_helper_init(LcdBlState::off);
    else
        lcdbl_helper_init(lcdblStateCache); // up or equal
    DL(F("Updating local short SMA..."));
    sma_append(sma_local_s, now_local);
    DL(F("Updating remote short SMA..."));
    sma_append(sma_remote_s, now_remote);
    DL(F("Updating local long SMA..."));
    sma_append(sma_local_l, now_local);
    DL(F("Updating remote long SMA..."));
    sma_append(sma_remote_l, now_remote);
    delay(500);  // time to print / read;
}

/////////////////////// transState servicer

void enter_f(StateMachine* state_machine) {
    // setup() & exit_f() prepare initial lcdBlState state
    lcdBlState.service();
    // going to hold?
    if (transState.getCurrentId() != TransState::enter)
        D(F("Transition to hold"));
        // choose_updown() preserved in lcdblStateCache
        lcdbl_helper_init(LcdBlState::on);
}

void hold_f(StateMachine* state_machine) {
    lcdBlState.service();
    // going to exit?
    if (transState.getCurrentId() != TransState::hold)
        D(F("Transition to exit"));
        // this cycle's lcdBlState
        if ((lcdblStateCache != LcdBlState::down) ||
            (lcdblStateCache != LcdBlState::equal))
            lcdbl_helper_init(LcdBlState::off);
        else
            lcdbl_helper_init(lcdblStateCache); // down or equal
}

void exit_f(StateMachine* state_machine) {
    lcdBlState.service();
    // going to enter?
    if (transState.getCurrentId() != TransState::exit) {
        // Go to snooze or back to now?
        if (dispState.getCurrentId() == DispState::four) {
            // back to now
            dispCycles++;
            if (dispCycles < MAX_DISPLAY_CYCLES) { // back to now
                D(F("Setting up for display cycle: ")); DL(dispCycles);
                // new choice for next cycle
                choose_updown();  // sets lcdBlState ID
                dispState.setCurrentId(DispState::now);
            } else {
                 D(F("Setting up for snooze"));
                 // to snooze
                 dispCycles = 0;
                 dispState.setCurrentId(DispState::snooze);
            }
        }
        D(F("Transition to enter"));
        transState.setCurrentId(TransState::enter);
        // enter always only handles up or equal
        if ((lcdblStateCache != LcdBlState::up) ||
            (lcdblStateCache != LcdBlState::equal))
            lcdbl_helper_init(LcdBlState::off);
        else
            lcdbl_helper_init(lcdblStateCache); // up or equal
    }
}

//////////////////////// lcdBLState servicers

void lcdbl_state_helper(uint16_t lcdblTicks_end,
                  uint8_t lcdBlVal_end) {
    if (lcdblTicks < lcdblTicks_end) {
        if (lcdBlStateTimer.update())  // one tick passed
            lcdblTicks++;
        D(lcdblTicks); D(F("/")); DL(LCD_BL_RAMP_TICKS);
    } else { // done with this trans state
            lcdblTicks = 0;
            lcdBlVal = lcdBlVal_end;  // guarantee ending value
            transState.next();  // wraps to beginning if end
    }
    analogWrite(PIN_LCD_BL, lcdBlVal);
    analogWrite(PIN_STATUS_LED, lcdBlVal);
}

void lcdbl_state_up_f(StateMachine* state_machine) {
    if (transState.getCurrentId() == TransState::enter) {
        D(F("Backlight up ramp "));
        lcdbl_state_helper(LCD_BL_RAMP_TICKS, 255);
    } else {
        // only enter handles up ramp
        transState.next();
    }
}

void lcdbl_state_equal_f(StateMachine* state_machine) {
    if ((transState.getCurrentId() == TransState::enter) ||
        (transState.getCurrentId() == TransState::exit)) {
        D(F("Backlight equal hold "));
        lcdbl_state_helper(LCD_BL_EQ_TICKS, 255);
    } else {
        // only enter/exit handles equal delay
        transState.next();
    }
}

void lcdbl_state_on_f(StateMachine* state_machine) {
    if (transState.getCurrentId() == TransState::hold) {
        D(F("Backlight hold on "));
        lcdbl_state_helper(LCD_BL_ON_TICKS, 255);
    } else {
        // only hold does on
        transState.next();
    }
}

void lcdbl_state_down_f(StateMachine* state_machine) {
    if (transState.getCurrentId() == TransState::exit) {
        D(F("Backlight down ramp "));
        lcdbl_state_helper(LCD_BL_RAMP_TICKS, 0);
    } else {
        // only exit does down ramp
        transState.next();
    }
}

void lcdbl_state_off_f(StateMachine* state_machine) {
    D(F("Backlight hold off "));
    lcdbl_state_helper(LCD_BL_OFF_TICKS, 0);
    // to be safe, always go back to enter
    transState.setCurrentId(TransState::enter);
}

//////////////////////// lcdBlStateTimer servicers

void lcdbl_event_up_f(const TimedEvent* timed_event) {
    // prevent overflow
    if (lcdBlVal < 239)
        lcdBlVal += 16;
    else
        lcdBlVal = 255;
}

void lcdbl_event_down_f(const TimedEvent* timed_event) {
    // prevent underflow
    if (lcdBlVal > 17)
        lcdBlVal -= 16;
    else
        lcdBlVal = 0;
}

void lcdbl_event_equal_f(const TimedEvent* timed_event) {
    lcdBlVal = 255; // signal not done
}

void lcdbl_event_on_f(const TimedEvent* timed_event) {
    lcdBlVal = 255; // signal not done
}

void lcdbl_event_off_f(const TimedEvent* timed_event) {
    lcdBlVal = 0; // signal not done
}

void setup(void) {
    // timing critical
    noInterrupts();
    // Clear the reset flag.
    MCUSR &= ~(1<<WDRF);
    // In order to change WDE or the prescaler, we need to
    // set WDCE (This will allow updates for 4 clock cycles).
    WDTCSR |= (1<<WDCE) | (1<<WDE);
    // set new watchdog timeout prescaler value
    WDTCSR = WDTO_8S;
    // Enable the WD interrupt (note no reset).
    WDTCSR |= _BV(WDIE);
    interrupts();

    all_pins_up();
    DL(F("Initializing..."));

    D(F("Short SMA points: ")); D(SMA_POINTS_S);
    D(F(" Short SMA update interval (ms): ")); DL(TEMP_SAMPLE_INTVL);

    D(F("Long SMA points: ")); D(SMA_POINTS_L);
    D(F(" Long SMA update interval (ms): ")); DL(TEMP_SAMPLE_INTVL);

    D(F("LCD up/dn Ramp intvl (ms): ")); D(LCD_BL_RAMP_INTERVAL);
    D(F("LCD ramp ticks (1-second divisions): ")); DL(LCD_BL_RAMP_TICKS);

    D(F("LCD equal ticks: ")); D(LCD_BL_EQ_TICKS);
    D(F(" LCD on ticks: ")); DL(LCD_BL_ON_TICKS);
    DL(F("Computing initial temperatures..."));
    start_conversion(rom_local, max_local);
    start_conversion(rom_remote, max_remote);
    lcd.setCursor(0,0);
    lcd.print("Collecting initl");
    lcd.setCursor(0,1);
    lcd.print("temp. data");
    lcd.display();

    // Conversion takes a while, do other stuff while it runs
    DL(F("Setting up Data display state machine..."));
    dispState.define(DispState.now, now_f);
    dispState.define(DispState.one, one_f);
    dispState.define(DispState.four, four_f);
    dispState.define(DispState.snooze, snooze_f);

    D(F("Setting up transition state machine..."));
    transState.define(TransState.enter, enter_f);
    transState.define(TransState.hold, hold_f);
    transState.define(TransState.exit, exit_f);

    DL(F("Setting up backlight state machine..."));
    lcdBlState.define(LcdBlState.up, lcdbl_state_up_f);
    lcdBlState.define(LcdBlState.on, lcdbl_state_on_f);
    lcdBlState.define(LcdBlState.down, lcdbl_state_down_f);
    lcdBlState.define(LcdBlState.equal, lcdbl_state_equal_f);
    lcdBlState.define(LcdBlState.off, lcdbl_state_off_f);

    // Time to print / read
    delay(500);

    DL(F("Waiting for conversion to finish..."));
    wait_conversion(rom_local, max_local);
    wait_conversion(rom_remote, max_remote);

    DL(F("Setting up local temp..."));
    now_local = get_temp(rom_local, max_local);
    sma_append(sma_local_s, now_local);
    sma_append(sma_local_l, now_local);

    DL(F("Setting up remote temp..."));
    now_remote = get_temp(rom_remote, max_remote);
    sma_append(sma_remote_s, now_remote);
    sma_append(sma_remote_l, now_remote);
    DL(F("Looping..."));
}

void loop(void) {
    current_time = sleepAdvance * WDT8SEC + millis();
    temp_update();  // now_local & now_remote
    current_time = sleepAdvance * WDT8SEC + millis();
    dispState.service(); // and all lower nested-states
}
