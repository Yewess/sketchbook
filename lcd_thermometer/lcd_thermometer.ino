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

    // Wait on conversion, setup starting SMA values
    DL(F("Setting first local short SMA..."));
    sma_append(data.sma_local_s,
               data.rom_local,
               data.max_local_p);
    DL(F("Setting first local long SMA..."));
    sma_append(data.sma_local_l,
               data.rom_local,
               data.max_local_p);
    DL(F("Setting first remote short SMA..."));
    sma_append(data.sma_remote_s,
               data.rom_remote,
               data.max_remote_p);
    DL(F("Setting first remote long SMA..."));
    sma_append(data.sma_remote_s,
               data.rom_remote,
               data.max_remote_p);
}

inline void Data::loop(void) {
}

/*
 *  Private Member Functions
 */

void Data::init_max(OneWire& owb,
                    MaxDS18B20::MaxRom& rom,
                    MaxDS18B20*& max_pr) const {
    while (true) {
        if (MaxDS18B20::getMaxROM(owb, rom, 0)) {
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
                                MaxDS18B20::MaxRom& rom,
                                MaxDS18B20*& max_pr) const {
    int16_t temp = get_temp(rom, max_pr);
    if (temp != FAULTTEMP)
        return sma.append(get_temp(rom, max_pr));
    else
        DL(F("Not recording faulty temperature!"));
        return FAULTTEMP;
}

// Regular functions

void lcdbl_up_ef(const TimedEvent* timed_event) {
    // prevent overflow
    if (data.lcdBlVal < 239)
        data.lcdBlVal += 16;
    else
        data.lcdBlVal = 255;
}

void lcdbl_down_ef(const TimedEvent* timed_event) {
    // prevent underflow
    if (data.lcdBlVal > 17)
        data.lcdBlVal -= 16;
    else
        data.lcdBlVal = 0;
}

void lcdbl_up(StateMachine* state_machine) {
    static TimedEvent lcdbl_up_te(data.current_time, 1000 / 16, lcdbl_up_ef);
    if (data.lcdBlVal < 255)
        lcdbl_up_te.update();
    analogWrite(Data::PIN_LCD_BL, data.lcdBlVal);
}

void lcdbl_on(StateMachine* state_machine) {
    data.lcdBlVal = 255;
    analogWrite(Data::PIN_LCD_BL, data.lcdBlVal);
}

void lcdbl_down(StateMachine* state_machine) {
    static TimedEvent lcdbl_down_te(data.current_time, 1000 / 16, lcdbl_down_ef);
    if (data.lcdBlVal > 0)
        lcdbl_down_te.update();
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
