/**********************************************************************
 Notes:

LCD:

    1234567890123456
    Now L R *C  Diff
    000.0 000.0 000

    1hA L R *C  Diff
    000.0 000.0 000

    4hA L R *C  Diff
    000.0 000.0 000

    * backlight:
        * "Up" blink if current temp > 1-hour temp
              ****     ****     ****
             *****    *****    *****
            ******   ******   ******
           *******  *******  *******
           |--|--|  repeat once per categoty
            1s 8s

        * "Down" blink if current temp < 1-hour temp
          ****     ****     ****
          *****    *****    *****
          ******   ******   ******
          *******  *******  *******
           |--|--|  repeat once per categoty
            1s 8s

Non-display:
    * Sleep for 8s (8000ms)
    * measure temp for 1s (1000ms)
    * misc. calc for 1s   (1000ms)

Data:
0) measure temp every 10 minutes -> (1 A + 1 B = 2)
1) every 10 minutes, save temp -> 1-hour SMA (6 A + 6 B readings = 12)
2) every hour, also save temp -> 24-hour SMA (24A + 24B = 48)

Memory requirements for data:
* class SimpleMovingAvg: 8 bytes + 2 * max_points bytes
* "live" A & B readings @ 2-bytes == 4 bytes total
* 60mSMA A & B @ 20-bytes        == 40 bytes total
* 24hSMA A & B @ 56-bytes       == 112 bytes total
                               --------------------
                                   156 bytes

Power:

35-45 mA ~= 40 hours w/ AA source
   10 mA ~= 175 hours w/ AA source

----------------------------------------------------

Measuring 4xAA battery:

z1 = 400k ohm
z2 = 1m ohm

>>> def vdiv(vin, z1, z2):
...     return vin * (z2 / (z1 + z2))

Max ~= 7.0 volts

>>> print vdiv(7.0, 400000.0, 1000000.0)
5.0

>>> print vdiv(4.5, 400000.0, 1000000.0)
3.21428571429

setup() {
    connect vout to analog pin
    connect z2 through OUTPUT gpio pin }
    set gpio HIGH (source)

check_battery() {
    set gpio LOW (sink)
    read analog pin
    set gpio HIGH (source)
    # map read value (0 and 1023)
    volts = map(red, 0, 1023, 5000, 0)
    battery full = 5000
    battery empty = 3214
    battert percent = map(volts, 3214, 5000, 0, 100)
    return battert percent
}
**********************************************************************/
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
#include "lcd_thermometer.h"

/*
 *  Public Member Functions
 */

Data::Data(void)
           :
           owb_local(PIN_OWB_LOCAL),
           owb_remote(PIN_OWB_REMOTE),
           now_local(FAULTTEMP),
           now_remote(FAULTTEMP),
           sma_local_s(SMA_POINTS_S),
           sma_local_l(SMA_POINTS_L),
           sma_remote_s(SMA_POINTS_S),
           sma_remote_l(SMA_POINTS_L),
           lcd(PIN_LCD_RS, PIN_LCD_EN,
               PIN_LCD_DA4, PIN_LCD_DA5,
               PIN_LCD_DA6, PIN_LCD_DA7),
           dispState(sizeof(DispState) / sizeof(uint8_t),
                     DispState.now),
           transState(sizeof(TransState) / sizeof(uint8_t),
                      TransState.enter),
           lcdBlState(sizeof(LcdBlState) / sizeof(uint8_t),
                      LcdBlState.equal),
           lcdBlStateTimer(current_time, LCD_BL_RAMP_INTERVAL, lcdbl_event_equal_f),
           lcdblStateCache(LcdBlState.equal),
           lcdblTicks(0),
           dispCycles(0),
           sleepCycleCounter(0),
           sleepAdvance(0),
           lcdBlVal(255) { // no flicker @ start
    // timing critical
    noInterrupts();
    // Clear the reset flag.
    MCUSR &= ~(1<<WDRF);
    // In order to change WDE or the prescaler, we need to
    // set WDCE (This will allow updates for 4 clock cycles).
    WDTCSR |= (1<<WDCE) | (1<<WDE);
    // set new watchdog timeout prescaler value
    WDTCSR = (1<<WDP0) | (1<<WDP3); // 8.0 seconds
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
}

void Data::init_max(OneWire& owb,
                    MaxDS18B20::MaxRom& rom,
                    MaxDS18B20*& max_pr) const {
    while (true) {
        if (MaxDS18B20::getMaxROM(owb, rom, 0)) {
            if (max_pr == NULL)
                max_pr = new MaxDS18B20(owb, rom);
            max_pr->setResolution(MAXRES);
            if (!max_pr->writeMem())
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

inline void Data::start_conversion(MaxDS18B20::MaxRom& rom,
                                   MaxDS18B20*& max_pr) const {
    D(F("Starting conversion on: ")); rom.serial_print();
    if (!max_pr->startConversion())
        DL(F("Error: Device disappeared!"));
}

inline void Data::wait_conversion(MaxDS18B20::MaxRom& rom,
                                  MaxDS18B20*& max_pr) const {
    D(F("Waiting for conversion on: ")); rom.serial_print();
    D(F("."));
    while (!max_pr->conversionDone()) {
        digitalWrite(PIN_STATUS_LED,
                     !digitalRead(PIN_STATUS_LED));
        D(F("."));
        delay(100);
    }
    DL(F("done"));
}

inline int16_t Data::get_temp(MaxDS18B20::MaxRom& rom,
                              MaxDS18B20*& max_pr) const {
    if (!max_pr->conversionDone())
        wait_conversion(rom, max_pr);
    digitalWrite(PIN_STATUS_LED, HIGH);
    D(F("Reading memory from device: ")); rom.serial_print();
    if (max_pr->readMem()) {
        if (Data::CELSIUS) {
            D(F("Temperature: ")); D(max_pr->getTempC()); DL(F("°C"));
            digitalWrite(PIN_STATUS_LED, LOW);
            return max_pr->getTempC();
        } else {
            D(F("Temperature: ")); D(max_pr->getTempF()); DL(F("°F"));
            digitalWrite(PIN_STATUS_LED, LOW);
            return max_pr->getTempF();
        }
    }
    DL(F("Error reading memory!"));
    return FAULTTEMP;
}

inline int16_t Data::sma_append(SimpleMovingAvg& sma,
                                uint16_t temp) {
    if (temp != FAULTTEMP) {
        return sma.append(temp);
    } else {
        DL(F("Not recording faulty temperature!"));
        return FAULTTEMP;
    }
}


void Data::all_pins_up(void){
    power_all_enable();

    #ifdef DEBUG
        pinMode(PIN_SERIAL_RX, INPUT);
        pinMode(PIN_SERIAL_TX, OUTPUT);
        delay(100);
        Serial.begin(SERIAL_BAUD);
        DL(">>>DEBUG ENABLED<<<");
    #endif // DEBUG

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

    D(F("Setting status LED pin to output: ")); DL(PIN_STATUS_LED);
    pinMode(PIN_STATUS_LED, OUTPUT);
    analogWrite(PIN_STATUS_LED, 0);  // PWM off

    D(F("Setting local OneWire bus pin to output: ")); DL(PIN_OWB_LOCAL);
    pinMode(PIN_OWB_LOCAL, OUTPUT);

    D(F("Setting remote OneWire bus pin to output: ")); DL(PIN_OWB_REMOTE);
    pinMode(PIN_OWB_REMOTE, OUTPUT);

    D(F("Powering up peripherals on pin: ")); DL(PIN_PERIPH_POWER);
    pinMode(PIN_PERIPH_POWER, OUTPUT);
    pinMode(PIN_PERIPH_POWER, HIGH);
    // Devices need power-on time to settle
    delay(100);

    DL(F("Initializing LCD"));
    lcd.begin(LCD_COLS, LCD_ROWS);
    lcd.noDisplay();
    lcd.clear();

    DL(F("Setting up local sensor"));
    // These will do inf. loop until sensor is present, give feedback
    lcd.setCursor(0,0);
    lcd.print(F("Connect local   "));
    lcd.setCursor(0,1);
    lcd.print(F("temp. temp sens."));
    lcd.display();
    init_max(owb_local, rom_local, max_local_p);
    lcd.noDisplay();
    lcd.setCursor(9,1);
    lcd.print(F("remote  "));
    lcd.display();
    init_max(owb_remote, rom_remote, max_remote_p);
    // feedback that remote sensor connected
    lcd.noDisplay();
    lcd.clear();

    current_time = sleepAdvance * WDT8SEC + millis();
    D(F("Setting timer (+advance): "));
    D(current_time);
    D(F(" ("));
    D(sleepAdvance * WDT8SEC);
    DL(F(")"));
}

void Data::all_pins_down(void) {
    DL(F("Going to sleep"));
    lcd.noDisplay();
    lcd.clear();
    lcd.print(F("Going to sleep"));
    lcd.setCursor(0,1);
    lcd.print(SLEEPTIME / 1000);
    lcd.print(F(" seconds"));
    lcd.display();
    #ifdef DEBUG
    Serial.flush();
    Serial.end();
    #endif // DEBUG
    delay(500);  // time to read message

    // Make sure everything is turned off first!
    digitalWrite(PIN_PERIPH_POWER, LOW);
    for (uint8_t pin=0; pin < 13; pin++) {
        digitalWrite(pin, LOW);
    }
    for (uint8_t pin=A1; pin < A5; pin++) {
        digitalWrite(pin, LOW);
    }
    power_all_disable();
}

inline void Data::setup(void) {
    DL(F("Computing initial temperatures..."));
    start_conversion(rom_local, max_local_p);
    start_conversion(rom_remote, max_remote_p);
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
    wait_conversion(rom_local, max_local_p);
    wait_conversion(rom_remote, max_remote_p);

    DL(F("Setting up local temp..."));
    now_local = get_temp(rom_local, max_local_p);
    sma_append(sma_local_s, now_local);
    sma_append(sma_local_l, now_local);

    DL(F("Setting up remote temp..."));
    now_remote = get_temp(rom_remote, max_remote_p);
    sma_append(sma_remote_s, now_remote);
    sma_append(sma_remote_l, now_remote);
}

inline void Data::loop(void) {
    current_time = sleepAdvance * WDT8SEC + millis();
    temp_update();  // now_local & now_remote
    current_time = sleepAdvance * WDT8SEC + millis();
    dispState.service(); // and all lower nested-states
}

/////////////////////// temp measurement

void temp_update(void) {
    if (data.max_local_p->conversionDone()) {
        D(F("Updating local reading..."));
        data.now_local = data.get_temp(data.rom_local, data.max_local_p);
        data.start_conversion(data.rom_local, data.max_local_p);
    }
    if (data.max_remote_p->conversionDone()) {
        D(F("Updating remote reading..."));
        data.now_remote = data.get_temp(data.rom_remote, data.max_remote_p);
        data.start_conversion(data.rom_remote, data.max_remote_p);
    }
}

/////////////////////// DispState servicers

void lcdbl_helper_init(uint8_t lcd_bl_state) {
    data.lcdblTicks = 0;
    switch (lcd_bl_state) {
        case Data::LcdBlState::up:
            DL(F("Initializing backlight up transition"));
            data.lcdBlStateTimer.reInitIfNot(lcdbl_event_up_f);
            data.lcdBlVal = 0;
            break;
        case Data::LcdBlState::equal:
            DL(F("Initializing backlight equal transition"));
            data.lcdBlStateTimer.reInitIfNot(lcdbl_event_equal_f);
            data.lcdBlVal = 255;
            break;
        case Data::LcdBlState::on:
            DL(F("Initializing backlight on transition"));
            data.lcdBlStateTimer.reInitIfNot(lcdbl_event_on_f);
            data.lcdBlVal = 255;
            break;
        case Data::LcdBlState::down:
            D(F("Backlight down ramp "));
            data.lcdBlStateTimer.reInitIfNot(lcdbl_event_down_f);
            data.lcdBlVal = 255;
        case Data::LcdBlState::off:
            DL(F("Initializing backlight off transition"));
            data.lcdBlStateTimer.reInitIfNot(lcdbl_event_off_f);
            data.lcdBlVal = 0;
    }
}

void choose_updown(void) {  // ONCE per display change
    uint16_t sma_value = data.sma_remote_s.value();

    if (data.now_remote > sma_value) {
        data.lcdBlState.setCurrentId(Data::LcdBlState::up);
    } else if (data.now_remote < sma_value) {
        data.lcdBlState.setCurrentId(Data::LcdBlState::down);
    } else {
        data.lcdBlState.setCurrentId(Data::LcdBlState::equal);
    }
    data.lcdblStateCache = data.lcdBlState.getCurrentId();
}

void disp_header(PGM_P header_p) {
    char c = pgm_read_byte(header_p);
    for (; c != 0; c = pgm_read_byte(header_p++))
         data.lcd.print(c);
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
        data.lcd.print(ibuff);
    // only room for 3 digits + 1 decimal
    data.lcd.print('.');
    data.lcd.print(ibuff);
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
        data.lcd.print('+');
    else
        data.lcd.print('-');
    for (uint8_t i=0; i < len; i++)
        data.lcd.print(ibuff);
}

void disp_unit() {
    data.lcd.print(Data::DEGREE_CHAR);
    if (data.CELSIUS)
        data.lcd.print('C');
    else
        data.lcd.print('F');
}

void now_f(StateMachine* state_machine) {
    /*
      Now L R *C  Diff
      000.0 000.0 +000
    */
    // copy header
    data.lcd.noDisplay();
    data.lcd.clear();
    disp_header(PSTR("Now L R CF  Diff"));
    data.lcd.setCursor(8,0);
    disp_unit();
    data.lcd.setCursor(0,1);
    disp_temp(data.now_local);
    data.lcd.print(' ');
    disp_temp(data.now_remote);
    data.lcd.print(' ');
    disp_diff(data.now_local, data.now_remote);
    data.lcd.display();
    // service state
    data.transState.service();
}

void one_f(StateMachine* state_machine) {
    /*
      1hA L R *C  Diff
      000.0 000.0 +000
    */
    const int16_t sma_local = data.sma_remote_s.value();
    const int16_t sma_remote = data.sma_remote_s.value();
    data.lcd.noDisplay();
    data.lcd.clear();
    disp_header(PSTR("1hA L R CF  Diff"));
    data.lcd.setCursor(8,0);
    disp_unit();
    data.lcd.setCursor(0,1);
    disp_temp(sma_local);
    data.lcd.print(' ');
    disp_temp(sma_remote);
    data.lcd.print(' ');
    disp_diff(data.now_local, data.now_remote);
    data.lcd.display();
    // service state
    data.transState.service();
}

void four_f(StateMachine* state_machine) {
   /*
     4hA L R *C  Diff
     000.0 000.0 +000
    */
    const int16_t sma_local = data.sma_remote_l.value();
    const int16_t sma_remote = data.sma_remote_l.value();
    data.lcd.noDisplay();
    data.lcd.clear();
    disp_header(PSTR("4hA L R CF  Diff"));
    data.lcd.setCursor(8,0);
    disp_unit();
    data.lcd.setCursor(0,1);
    disp_temp(sma_local);
    data.lcd.print(' ');
    disp_temp(sma_remote);
    data.lcd.print(' ');
    disp_diff(data.now_local, data.now_remote);
    data.lcd.display();
    // service state
    data.transState.service();
}

void snooze_f(StateMachine* state_machine) {
    D(F("Snoozing..."));
    data.all_pins_down();
    // ISR increments data.sleepCycleCounter on wakeup
    for (data.sleepCycleCounter=0; data.sleepCycleCounter < Data::SLEEP_CYCLES;) {
        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
        sleep_enable();
        sleep_mode(); // ZZzzzzzzzz
        sleep_disable(); // Woke up, disable ISR
        data.sleepAdvance += data.WDT8SEC;
    }
    data.all_pins_up();
    D(F("...Woke up"));
    // Gather initial data
    // Take a while, use time initializing LCD
    data.start_conversion(data.rom_local, data.max_local_p);
    data.start_conversion(data.rom_remote, data.max_remote_p);
    data.lcd.begin(Data::LCD_COLS, Data::LCD_ROWS);
    data.lcd.clear();
    data.lcd.print("Waking up...");
    DL(F("Gathering post-snooze temp. readings"));
    data.wait_conversion(data.rom_local, data.max_local_p);
    data.wait_conversion(data.rom_remote, data.max_remote_p);
    temp_update();  // and start next conversion
    choose_updown(); // set up data.lcdBlState
    data.transState.setCurrentId(Data::TransState::enter);
    data.dispState.setCurrentId(Data::DispState::now);
    data.dispCycles = 0;
    // enter always only handles up or equal
    if ((data.lcdblStateCache != Data::LcdBlState::up) ||
        (data.lcdblStateCache != Data::LcdBlState::equal))
        lcdbl_helper_init(Data::LcdBlState::off);
    else
        lcdbl_helper_init(data.lcdblStateCache); // up or equal
    DL(F("Updating local short SMA..."));
    data.sma_append(data.sma_local_s, data.now_local);
    DL(F("Updating remote short SMA..."));
    data.sma_append(data.sma_remote_s, data.now_remote);
    DL(F("Updating local long SMA..."));
    data.sma_append(data.sma_local_l, data.now_local);
    DL(F("Updating remote long SMA..."));
    data.sma_append(data.sma_remote_l, data.now_remote);
    delay(500);  // time to print / read;
}

/////////////////////// transState servicer

void enter_f(StateMachine* state_machine) {
    // data.setup() & exit_f() prepare initial lcdBlState state
    data.lcdBlState.service();
    // going to hold?
    if (data.transState.getCurrentId() != Data::TransState::enter)
        D(F("Transition to hold"));
        // choose_updown() preserved in data.lcdblStateCache
        lcdbl_helper_init(Data::LcdBlState::on);
}

void hold_f(StateMachine* state_machine) {
    data.lcdBlState.service();
    // going to exit?
    if (data.transState.getCurrentId() != Data::TransState::hold)
        D(F("Transition to exit"));
        // this cycle's lcdBlState
        if ((data.lcdblStateCache != Data::LcdBlState::down) ||
            (data.lcdblStateCache != Data::LcdBlState::equal))
            lcdbl_helper_init(Data::LcdBlState::off);
        else
            lcdbl_helper_init(data.lcdblStateCache); // down or equal
}

void exit_f(StateMachine* state_machine) {
    data.lcdBlState.service();
    // going to enter?
    if (data.transState.getCurrentId() != Data::TransState::exit) {
        // Go to snooze or back to now?
        if (data.dispState.getCurrentId() == Data::DispState::four) {
            // back to now
            data.dispCycles++;
            if (data.dispCycles < Data::MAX_DISPLAY_CYCLES) { // back to now
                D(F("Setting up for display cycle: ")); DL(data.dispCycles);
                // new choice for next cycle
                choose_updown();  // sets lcdBlState ID
                data.dispState.setCurrentId(Data::DispState::now);
            } else {
                 D(F("Setting up for snooze"));
                 // to snooze
                 data.dispCycles = 0;
                 data.dispState.setCurrentId(Data::DispState::snooze);
            }
        }
        D(F("Transition to enter"));
        data.transState.setCurrentId(Data::TransState::enter);
        // enter always only handles up or equal
        if ((data.lcdblStateCache != Data::LcdBlState::up) ||
            (data.lcdblStateCache != Data::LcdBlState::equal))
            lcdbl_helper_init(Data::LcdBlState::off);
        else
            lcdbl_helper_init(data.lcdblStateCache); // up or equal
    }
}

//////////////////////// lcdBLState servicers

void lcdbl_state_helper(uint16_t lcdblTicks_end,
                  uint8_t lcdBlVal_end) {
    if (data.lcdblTicks < lcdblTicks_end) {
        if (data.lcdBlStateTimer.update())  // one tick passed
            data.lcdblTicks++;
        D(data.lcdblTicks); D(F("/")); DL(data.LCD_BL_RAMP_TICKS);
    } else { // done with this trans state
            data.lcdblTicks = 0;
            data.lcdBlVal = lcdBlVal_end;  // guarantee ending value
            data.transState.next();  // wraps to beginning if end
    }
    analogWrite(data.PIN_LCD_BL, data.lcdBlVal);
    analogWrite(data.PIN_STATUS_LED, data.lcdBlVal);
}

void lcdbl_state_up_f(StateMachine* state_machine) {
    if (data.transState.getCurrentId() == Data::TransState::enter) {
        D(F("Backlight up ramp "));
        lcdbl_state_helper(data.LCD_BL_RAMP_TICKS, 255);
    } else {
        // only enter handles up ramp
        data.transState.next();
    }
}

void lcdbl_state_equal_f(StateMachine* state_machine) {
    if ((data.transState.getCurrentId() == Data::TransState::enter) ||
        (data.transState.getCurrentId() == Data::TransState::exit)) {
        D(F("Backlight equal hold "));
        lcdbl_state_helper(data.LCD_BL_EQ_TICKS, 255);
    } else {
        // only enter/exit handles equal delay
        data.transState.next();
    }
}

void lcdbl_state_on_f(StateMachine* state_machine) {
    if (data.transState.getCurrentId() == Data::TransState::hold) {
        D(F("Backlight hold on "));
        lcdbl_state_helper(data.LCD_BL_ON_TICKS, 255);
    } else {
        // only hold does on
        data.transState.next();
    }
}

void lcdbl_state_down_f(StateMachine* state_machine) {
    if (data.transState.getCurrentId() == Data::TransState::exit) {
        D(F("Backlight down ramp "));
        lcdbl_state_helper(data.LCD_BL_RAMP_TICKS, 0);
    } else {
        // only exit does down ramp
        data.transState.next();
    }
}

void lcdbl_state_off_f(StateMachine* state_machine) {
    D(F("Backlight hold off "));
    lcdbl_state_helper(data.LCD_BL_OFF_TICKS, 0);
    // to be safe, always go back to enter
    data.transState.setCurrentId(Data::TransState::enter);
}

//////////////////////// lcdBlStateTimer servicers

void lcdbl_event_up_f(const TimedEvent* timed_event) {
    // prevent overflow
    if (data.lcdBlVal < 239)
        data.lcdBlVal += 16;
    else
        data.lcdBlVal = 255;
}

void lcdbl_event_down_f(const TimedEvent* timed_event) {
    // prevent underflow
    if (data.lcdBlVal > 17)
        data.lcdBlVal -= 16;
    else
        data.lcdBlVal = 0;
}

void lcdbl_event_equal_f(const TimedEvent* timed_event) {
    data.lcdBlVal = 255; // signal not done
}

void lcdbl_event_on_f(const TimedEvent* timed_event) {
    data.lcdBlVal = 255; // signal not done
}

void lcdbl_event_off_f(const TimedEvent* timed_event) {
    data.lcdBlVal = 0; // signal not done
}

//////////////////////// WTD sleep wakeup ISR
ISR(WDT_vect)
{
    data.sleepCycleCounter++;
}

void setup(void) {
    Data data;
    data.setup();
    DL(F("Looping..."));
}

void loop(void) {
    data.loop();
}
