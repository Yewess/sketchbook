/**********************************************************************
 Notes:

Interface:
    At startup and every 10 minutes, actvate display

    * Current Remote sense
    * Current Local sense
    * Current Difference

    * Remote 60mSMA
    * Local 60mSMA
    * Difference 60mSMA

    * Remote 24hSMA
    * Local 24hSMA
    * Difference 24hSMA

    * backlight:
        * "Up" blink if current temp > 1-hour temp
              ****     ****     ****
             *****    *****    *****
            ******   ******   ******
           *******  *******  *******
           |--|--|--|  repeat once per categoty
            1s 8s 1s
        * "Down" blink if current temp < 1-hour temp
          ****     ****     ****
          *****    *****    *****
          ******   ******   ******
          *******  *******  *******
           |--|--|--|  repeat once per categoty
            1s 8s 1s

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
           sma_local_s(SMA_POINTS_S),
           sma_local_l(SMA_POINTS_L),
           sma_remote_s(SMA_POINTS_S),
           sma_remote_l(SMA_POINTS_L),
           lcd(PIN_LCD_RS, PIN_LCD_EN,
               PIN_LCD_DA0, PIN_LCD_DA1,
               PIN_LCD_DA2, PIN_LCD_DA3,
               PIN_LCD_DA4, PIN_LCD_DA5,
               PIN_LCD_DA6, PIN_LCD_DA7),
           lcdBlState(sizeof(LcdBlState) / sizeof(LcdBlState::on),
                      LcdBlState::on),
           lcdDispState(sizeof(LcdDispState) / sizeof(LcdDispState::local_now),
                        LcdDispState::local_now),
           lcdBlStateTimer(current_time, LCD_BL_RAMP_INTERVAL, lcdbl_up_f),
           dispEHEState(DispEHEState::enter),
           lcdBlVal(255) {
    #ifdef DEBUG
    Serial.begin(115200);
    DL(F("Initializing..."));
    #endif // DEBUG
    D(F("Setting PIN_STATUS_LED ")); D(PIN_STATUS_LED); DL(F(" to OUTPUT"));
    pinMode(PIN_STATUS_LED, OUTPUT);

    DL(F("Setting up sensor devices..."));
    init_max(owb_local, rom_local, max_local_p);
    init_max(owb_remote, rom_remote, max_remote_p);

    // setup time
    DL(F("Setting timer"));
    current_time = millis();
}

inline void Data::setup(void) {
    DL(F("Reading initial temperatures..."));
    start_conversion(data.rom_local, data.max_local_p);
    start_conversion(data.rom_remote, data.max_remote_p);

    // Conversion takes a while, do other stuff while it runs
    DL(F("Initializing LCD..."));
    lcd.begin(LCD_COLS, LCD_ROWS);

    DL(F("Setting up backlight state machine..."));
    lcdBlState.define(LcdBlState::up, lcdbl_up);
    lcdBlState.define(LcdBlState::on, lcdbl_on);
    lcdBlState.define(LcdBlState::down, lcdbl_down);
    lcdBlState.define(LcdBlState::off, lcdbl_off);

    DL(F("Setting up LCD display state machine..."));
    lcdDispState.define(LcdDispState::local_now, disp_local_now);
    lcdDispState.define(LcdDispState::diff_now, disp_diff_now);
    lcdDispState.define(LcdDispState::local_60mSMA, disp_local_60mSMA);
    lcdDispState.define(LcdDispState::remote_60mSMA, disp_remote_60mSMA);
    lcdDispState.define(LcdDispState::diff_60mSMA, disp_diff_60mSMA);
    lcdDispState.define(LcdDispState::local_24hSMA, disp_local_24hSMA);
    lcdDispState.define(LcdDispState::remote_24hSMA, disp_remote_24hSMA);
    lcdDispState.define(LcdDispState::diff_24hSMA, disp_diff_24hSMA);
    lcdDispState.define(LcdDispState::snooze, disp_snooze);

    // Wait on conversion, setup starting SMA values
    DL(F("Setting up local temp..."));
    now_local = get_temp(rom_local, max_local_p);
    DL(F("Setting up remote temp..."));
    now_remote = get_temp(rom_remote, max_remote_p);
    DL(F("Setting first local short SMA..."));
    sma_append(data.sma_local_s, now_local);
    DL(F("Setting first local long SMA..."));
    sma_append(data.sma_local_l, now_local);
    DL(F("Setting first remote short SMA..."));
    sma_append(data.sma_remote_s, now_remote);
    DL(F("Setting first remote long SMA..."));
    sma_append(data.sma_remote_s, now_remote);
}

inline void Data::loop(void) {
    current_time = millis();
    lcdDispState.service();
}

/*
 *  Private Member Functions
 */

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
    D(F("Reading memory from device: ")); rom.serial_print();
    if (max_pr->readMem())
        D(F("Temperature: ")); D(max_pr->getTempC()); DL(F("°C"));
        return max_pr->getTempC();
    DL(F("Error reading memory!"));
    return FAULTTEMP;
}

inline int16_t Data::sma_append(SimpleMovingAvg& sma,
                                uint16_t temp) {
    if (temp != FAULTTEMP)
        return sma.append(temp);
    else
        DL(F("Not recording faulty temperature!"));
        return FAULTTEMP;
}

// Regular functions

// LCD content stuff

void disp_local_now(StateMachine* state_machine) {
    // write data on display
    switch (data.dispEHEState) {
        case Data::DispEHEState::enter:
        // if ramp up
            // if done ramp up
                // dispEHEState = DispEHEState::hold
            // else
                // setup or do ramp up
            break;
        case Data::DispEHEState::hold:
            // if done holding
                // dispEHEState = DispEHEState::exit
            // else
                // setup or do hold
            break;
        case Data::DispEHEState::exit:
            // if ramp down
                // if done ramp down
                    // dispEHEState = DispEHEState::enter
                    // lcdDispState.next()
                // else
                    // setup or do ramp down
            break;
    }
}

void disp_diff_now(StateMachine* state_machine) {
}

void disp_local_60mSMA(StateMachine* state_machine) {
}

void disp_remote_60mSMA(StateMachine* state_machine) {
}

void disp_diff_60mSMA(StateMachine* state_machine) {
}

void disp_local_24hSMA(StateMachine* state_machine) {
}

void disp_remote_24hSMA(StateMachine* state_machine) {
}

void disp_diff_24hSMA(StateMachine* state_machine) {
}

void disp_snooze(StateMachine* state_machine) {
    switch (data.dispEHEState) {
        case Data::DispEHEState::enter:
            // dispEHEState = DispEHEState::hold
            // go to sleep
            break;
        case Data::DispEHEState::hold:
            // if done converting
                // read temperature
                // handle sma's
                // dispEHEState = DispEHEState::exit
            break;
        case Data::DispEHEState::exit:
            // dispEHEState = DispEHEState::enter
            // lcdDispState.setCurrentId(LcdDispState::uint8_t);
            break;
    }
}

// backlight stuff

void lcdbl_up_f(const TimedEvent* timed_event) {
    // prevent overflow
    if (data.lcdBlVal < 239)
        data.lcdBlVal += 16;
    else
        data.lcdBlVal = 255;
}

void lcdbl_down_f(const TimedEvent* timed_event) {
    // prevent underflow
    if (data.lcdBlVal > 17)
        data.lcdBlVal -= 16;
    else
        data.lcdBlVal = 0;
}

void lcdbl_up(StateMachine* state_machine) {
    data.lcdBlStateTimer.reInitIfNot(lcdbl_up_f);
    if (data.lcdBlVal < 255)
        data.lcdBlStateTimer.update();
    analogWrite(Data::PIN_LCD_BL, data.lcdBlVal);
}

void lcdbl_on(StateMachine* state_machine) {
    data.lcdBlVal = 255;
    analogWrite(Data::PIN_LCD_BL, data.lcdBlVal);
}

void lcdbl_down(StateMachine* state_machine) {
    data.lcdBlStateTimer.reInitIfNot(lcdbl_down_f);
    if (data.lcdBlVal > 0)
        data.lcdBlStateTimer.update();
    analogWrite(Data::PIN_LCD_BL, data.lcdBlVal);
}

void lcdbl_off(StateMachine* state_machine) {
    data.lcdBlVal = 0;
    analogWrite(Data::PIN_LCD_BL, data.lcdBlVal);
}

void setup(void) {
    DL(F("Setting up..."));
    data.setup();
}

void loop(void) {
    DL(F("Looping..."));
    data.loop();
}
