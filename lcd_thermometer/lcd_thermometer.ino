/**********************************************************************
 Notes:

LCD:

    12345678901234567890
    Now  Loc   Rem   Dif
       000.0 000.0 000.0

    1hA  Loc   Rem   Dif
       000.0 000.0 000.0

    4hA  Loc   Rem   Dif
       000.0 000.0 000.0

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

#include <stdlib.h>
#include <LiquidCrystal.h>
#include <OneWire.h>
#include "MaxDS18B20.h"
#include "MovingAvg.h"
#include "TimedEvent.h"
#include "StateMachine.h"
#include "lcd.h"
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
           //short_smas(current_time, Data::TEMP_SAMPLE_INTVL, short_sma_update_f),
           short_smas(current_time, 600000, short_sma_update_f),
           //long_smas(current_time, Data::TEMP_SAMPLE_INTVL, long_sma_update_f),
           long_smas(current_time, 600000, long_sma_update_f),
           sma_local_s(SMA_POINTS_S),
           sma_local_l(SMA_POINTS_L),
           sma_remote_s(SMA_POINTS_S),
           sma_remote_l(SMA_POINTS_L),
           lcd(PIN_LCD_RS, PIN_LCD_EN,
               PIN_LCD_DA0, PIN_LCD_DA1,
               PIN_LCD_DA2, PIN_LCD_DA3,
               PIN_LCD_DA4, PIN_LCD_DA5,
               PIN_LCD_DA6, PIN_LCD_DA7),
           dispState(sizeof(DispState) / sizeof(uint8_t),
                     DispState.now),
           transState(sizeof(TransState) / sizeof(uint8_t),
                      TransState.enter),
           lcdBlState(sizeof(LcdBlState) / sizeof(uint8_t),
                      LcdBlState.equal),
           lcdBlStateTimer(current_time, LCD_BL_RAMP_INTERVAL, lcdbl_event_equal_f),
           last_updown(LcdBlState.equal),
           lcdblTicks(0),
           dispCycles(0),
           lcdBlVal(255) { // no flicker @ start
    #ifdef DEBUG
    Serial.begin(115200);
    #endif // DEBUG
    DL(F("Initializing..."));

    D(F("Short SMA points: ")); D(SMA_POINTS_S);
    D(F(" Short SMA update interval (ms): ")); DL(TEMP_SAMPLE_INTVL);

    D(F("Long SMA points: ")); D(SMA_POINTS_L);
    D(F(" Long SMA update interval (ms): ")); DL(TEMP_SAMPLE_INTVL);

    D(F("LCD up/dn Ramp intvl (ms): ")); D(LCD_BL_RAMP_INTERVAL);
    D(F("LCD ramp ticks (1-second divisions): ")); DL(LCD_BL_RAMP_TICKS);

    D(F("LCD equal ticks: ")); D(LCD_BL_EQ_TICKS);
    D(F(" LCD on ticks: ")); DL(LCD_BL_ON_TICKS);

    D(F("Setup LCD pins:"));
    D(F(" RS- ")); D(PIN_LCD_RS);
    pinMode(PIN_LCD_RS, OUTPUT);
    D(F(" EN- ")); D(PIN_LCD_EN);
    pinMode(PIN_LCD_EN, OUTPUT);
    D(F(" D0- ")); D(PIN_LCD_DA0);
    pinMode(PIN_LCD_DA0, OUTPUT);
    D(F(" D1- ")); D(PIN_LCD_DA1);
    pinMode(PIN_LCD_DA1, OUTPUT);
    D(F(" D2- ")); D(PIN_LCD_DA2);
    pinMode(PIN_LCD_DA2, OUTPUT);
    D(F(" D3- ")); D(PIN_LCD_DA3);
    pinMode(PIN_LCD_DA3, OUTPUT);
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

    D(F("Setting status LED pin to output: ")); DL(PIN_STATUS_LED);
    pinMode(PIN_STATUS_LED, OUTPUT);

    D(F("Setting local OneWire bus pin to output: ")); DL(PIN_OWB_LOCAL);
    pinMode(PIN_OWB_LOCAL, OUTPUT);

    D(F("Setting remote OneWire bus pin to output: ")); DL(PIN_OWB_REMOTE);
    pinMode(PIN_OWB_REMOTE, OUTPUT);

    DL(F("Setting up sensor devices..."));
    init_max(owb_local, rom_local, max_local_p);
    init_max(owb_remote, rom_remote, max_remote_p);

    // setup time
    DL(F("Setting timer"));
    current_time = millis();
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
    while (!max_pr->conversionDone()) {
        digitalWrite(PIN_STATUS_LED,
                     !digitalRead(PIN_STATUS_LED));
        delay(100);
    }
}

inline int16_t Data::get_temp(MaxDS18B20::MaxRom& rom,
                              MaxDS18B20*& max_pr) const {
    if (!max_pr->conversionDone())
        wait_conversion(rom, max_pr);
    digitalWrite(PIN_STATUS_LED, HIGH);
    D(F("Reading memory from device: ")); rom.serial_print();
    if (max_pr->readMem()) {
        D(F("Temperature: ")); D(max_pr->getTempC()); DL(F("°C"));
        digitalWrite(PIN_STATUS_LED, LOW);
        return max_pr->getTempC();
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


inline void Data::setup(void) {
    DL(F("Computing initial temperatures..."));
    start_conversion(data.rom_local, data.max_local_p);
    start_conversion(data.rom_remote, data.max_remote_p);

    // Conversion takes a while, do other stuff while it runs
    DL(F("Initializing LCD..."));
    lcd.begin(LCD_COLS, LCD_ROWS);

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

    DL(F("Setting up local temp..."));
    now_local = get_temp(rom_local, max_local_p);
    sma_append(data.sma_local_s, now_local);
    sma_append(data.sma_local_l, now_local);

    DL(F("Setting up remote temp..."));
    now_remote = get_temp(rom_remote, max_remote_p);
    sma_append(data.sma_remote_s, now_remote);
    sma_append(data.sma_remote_l, now_remote);
}

inline void Data::loop(void) {
    current_time = millis();
    temp_update();  // now_local & now_remote
    short_smas.update();
    long_smas.update();
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

void short_sma_update_f(const TimedEvent* timed_event) {
    DL(F("Updating local short SMA..."));
    data.sma_append(data.sma_local_s, data.now_local);
    DL(F("Updating remote short SMA..."));
    data.sma_append(data.sma_remote_s, data.now_remote);
}

void long_sma_update_f(const TimedEvent* timed_event) {
    DL(F("Updating local long SMA..."));
    data.sma_append(data.sma_local_l, data.now_local);
    DL(F("Updating remote long SMA..."));
    data.sma_append(data.sma_remote_l, data.now_remote);
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
    data.last_updown = data.lcdBlState.getCurrentId();
}

void now_f(StateMachine* state_machine) {
    // update LCD content
    data.transState.service();
}

void one_f(StateMachine* state_machine) {
    // update LCD content
    data.transState.service();
}

void four_f(StateMachine* state_machine) {
    // update LCD content
    data.transState.service();
}

void snooze_f(StateMachine* state_machine) {
    D(F("Snoozing..."));

    // STUB
    delay(10000); // STUB
    // STUB

    D(F("Waking up"));
    data.dispCycles = 0;
    data.dispState.setCurrentId(Data::DispState::now);
    DL(F("Gathering post-snooze temp. reading"));
    data.start_conversion(data.rom_local, data.max_local_p);
    data.start_conversion(data.rom_remote, data.max_remote_p);
    temp_update();  // and start next conversion
    choose_updown(); // set up data.lcdBlState
    data.transState.setCurrentId(Data::TransState::enter);
    // enter always only handles up or equal
    if ((data.last_updown != Data::LcdBlState::up) ||
        (data.last_updown != Data::LcdBlState::equal))
        lcdbl_helper_init(Data::LcdBlState::off);
    else
        lcdbl_helper_init(data.last_updown); // up or equal
}

/////////////////////// transState servicer

void enter_f(StateMachine* state_machine) {
    data.lcdBlState.service();
    // going to hold?
    if (data.transState.getCurrentId() != Data::TransState::enter)
        D(F("Transition to hold"));
        // choose_updown() preserved in data.last_updown
        lcdbl_helper_init(Data::LcdBlState::on);
}

void hold_f(StateMachine* state_machine) {
    data.lcdBlState.service();
    // going to exit?
    if (data.transState.getCurrentId() != Data::TransState::hold)
        D(F("Transition to exit"));
        // this cycle's lcdBlState
        if ((data.last_updown != Data::LcdBlState::down) ||
            (data.last_updown != Data::LcdBlState::equal))
            lcdbl_helper_init(Data::LcdBlState::off);
        else
            lcdbl_helper_init(data.last_updown); // down or equal
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
        if ((data.last_updown != Data::LcdBlState::up) ||
            (data.last_updown != Data::LcdBlState::equal))
            lcdbl_helper_init(Data::LcdBlState::off);
        else
            lcdbl_helper_init(data.last_updown); // up or equal
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

void setup(void) {
    DL(F("Setting up..."));
    data.setup();
}

void loop(void) {
    DL(F("Looping..."));
    data.loop();
}