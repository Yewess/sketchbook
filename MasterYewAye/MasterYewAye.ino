#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <stdlib.h>
#include <Arduino.h>
#include <Button.h>
#include <Encoder.h>
#include <LiquidCrystal.h>
#include <OneWire.h>
#include <MovingAvg.h>
#include <TimedEvent.h>
#include "MasterYewAye.h"

ISR(WDT_vect)
{
    sleepCycleCounter++;
}

// Start conversion on first device found
uint8_t start_conversion(OneWire& owb) {
    //D(F("Start conversion..."));
    owb.reset_search();
    uint8_t buff[8];
    uint8_t present = 0;
    present = owb.search(buff);
    if (!present)
        return OwbStatus::notFound;
    if (OneWire::crc8(buff, 7) != buff[7])
        return OwbStatus::romBadCrc;
    present = owb.reset();  // prepare for next command
    if (present == 0)
        return OwbStatus::busError;
    if ((buff[0] != 0x10) && (buff[0] != 0x28) && (buff[0] != 0x22))
        return OwbStatus::notDs18x20;
    owb.select(buff);
    owb.write(0x44, 1); // Start conversion
    //DL(F("...started"));
    return OwbStatus::converting;
}

inline bool conversion_done(OneWire& owb) {
    if (owb.read_bit() == 1)
        return true;
    else
        return false;
}

// retrieve temperature on first device found
uint8_t get_temp(OneWire& owb, int16_t& x10degrees, bool celsius) {
    x10degrees = Owb::checkStatus;
    //DL(F("Reading temperature"));
    owb.reset_search();
    uint8_t buff[9];
    uint8_t present = owb.search(buff);
    //D(1);
    if (!present)
        return OwbStatus::notFound;
    //D(2);
    if (OneWire::crc8(buff, 7) != buff[7])
        return OwbStatus::romBadCrc;
    present = owb.reset();  // prepare for next command
    //D(3);
    if (present == 0)
        return OwbStatus::busError;
    //D(4);
    bool type_s = false;
    if (buff[0] == 0x10)
        type_s = true;
    else if ((buff[0] == 0x28) || (buff[0] == 0x22))
        type_s = false;
    else
        return OwbStatus::notDs18x20;
    //D(5);
    owb.select(buff);
    owb.write(0xBE, 1);    // Read Scratchpad
    owb.read_bytes(buff, 9);
    if (OneWire::crc8(buff, 8) != buff[8])
        return OwbStatus::ramBadCrc;
    //D(6);
    x10degrees = x10degrees = (buff[1] << 8) | buff[0];
    if (type_s) {
        x10degrees = x10degrees << 3; // 9 bit resolution default
        if (buff[7] == 0x10)
            // "count remain" gives full 12 bit resolution
            x10degrees = (x10degrees & 0xFFF0) + 12 - buff[6];
    } else {
        // default is 12 bit resolution, 750 ms conversion time
        uint8_t precision = (buff[4] & 0x60);
        // at lower res, the low bits are undefined, zero them
        if (precision == 0x00)
            x10degrees = x10degrees & ~7;  // 9 bit resolution, 93.75 ms
        else if (precision == 0x20)
            x10degrees = x10degrees & ~3; // 10 bit res, 187.5 ms
        else if (precision == 0x40)
            x10degrees = x10degrees & ~1; // 11 bit res, 375 ms
    }
    //D(7);
    if (celsius)
        x10degrees = x10degrees * 10 / 16;  // preserve one decimal place
    else
        x10degrees = x10degrees / 16 * 18 + 320;  // preserve one decimal place
    return OwbStatus::complete;
}

uint16_t sma_append(SimpleMovingAvg& sma, uint16_t x10degrees) {
    if (x10degrees != Owb::checkStatus) {
        D(F("SMA Appended: ")); DL(x10degrees);
        return sma.append(x10degrees);
    } else {
        DL(F("Not recording faulty temperature!"));
        return Owb::checkStatus;
    }
}


void all_pins_up(void){

    #ifdef DEBUG
        pinMode(Pin::serialRx, OUTPUT);
        pinMode(Pin::serialTx, OUTPUT);
        Serial.begin(Pin::serialBaud);
        digitalWrite(Pin::serialRx, HIGH);
        digitalWrite(Pin::serialRx, HIGH);  // Prevents noise on line
        delay(10);  // time to settle
        DL();
        if (!pinsPrinted) {
            DL(">>>DEBUG ENABLED<<<");
            D(F("    OWB's power: ")); DL(Pin::owbMosfet);
            D(F("    OWB A data: ")); DL(Pin::owbA);
            D(F("    OWB B data: ")); DL(Pin::owbB);
            D(F("    LCD pins:"));
            D(F("        RS- ")); D(Pin::lcdRS);
            D(F("        EN- ")); D(Pin::lcdEN);
            D(F("        D4- ")); D(Pin::lcdD4);
            D(F("        D5- ")); D(Pin::lcdD5);
            D(F("        D6- ")); D(Pin::lcdD6);
            D(F("        D7- ")); D(Pin::lcdD7);
            D(F("        BL- ")); DL(Pin::lcdBL);
            D(F("    LCD power: ")); DL(Pin::lcdMosfet);
            D(F("    Aref power: ")); DL(Pin::arfMosfet);
            D(F("    Encoder A/C: ")); D(Pin::encA);
            D(F(" / ")); DL(Pin::encC);
            D(F("    Button: ")); DL(Pin::button);
            D(F("Data size: ")); DL(sizeof(sma) +
                                    ((tempSmaOne / tempSmaSample) * 4) +
                                    ((tempSmaFour / tempSmaSample) * 4));
            DL();
        }
        pinsPrinted = true;
    #endif // DEBUG

    // owb
    pinMode(Pin::owbA, INPUT); // external pull-up
    pinMode(Pin::owbB, INPUT); // external pull-up
    pinMode(Pin::owbMosfet, OUTPUT);
    digitalWrite(Pin::owbMosfet, HIGH); // low-side mosfet
    // lcd
    pinMode(Pin::lcdBL, OUTPUT);
    pinMode(Pin::lcdRS, OUTPUT);
    pinMode(Pin::lcdEN, OUTPUT);
    pinMode(Pin::lcdD4, OUTPUT);
    pinMode(Pin::lcdD5, OUTPUT);
    pinMode(Pin::lcdD6, OUTPUT);
    pinMode(Pin::lcdD7, OUTPUT);
    digitalWrite(Pin::lcdBL, LOW); // Backlight OFF!
    pinMode(Pin::lcdMosfet, OUTPUT);
    digitalWrite(Pin::lcdMosfet, HIGH);  // low-side mosfet
    // Battery sense
    pinMode(Pin::battSense, INPUT);
    pinMode(Pin::arfMosfet, OUTPUT);
    digitalWrite(Pin::arfMosfet, HIGH);  // low-side mosfet
    // Rotary encoder
    pinMode(Pin::encA, INPUT_PULLUP);
    pinMode(Pin::encC, INPUT_PULLUP);
    pinMode(Pin::button, INPUT_PULLUP);
    // Devices need power-on time to settle
    delay(10);
    lcd.begin(lcdCols, lcdRows);
    lcd.clear();
}

void all_pins_down(void) {
    #ifdef DEBUG
        DL(F("Going to sleep..."));
        Serial.flush();
        delay(10); // flush needs more time
        Serial.end();
    #endif // DEBUG
    for (uint8_t pin=0; pin < A5; pin++) {
        if (pin != Pin::battSense) {  // DO NOT SHORT BATTERY TO GROUND
            pinMode(pin, OUTPUT);
            digitalWrite(pin, LOW); // discharge to ground
            delay(1);  // time to discharge
        }
    }
    // One-wire bus
    pinMode(Pin::owbA, INPUT);
    pinMode(Pin::owbB, INPUT);
    // LCD Connections
    pinMode(Pin::lcdBL, INPUT);
    pinMode(Pin::lcdRS, INPUT);
    pinMode(Pin::lcdEN, INPUT);
    pinMode(Pin::lcdD4, INPUT);
    pinMode(Pin::lcdD5, INPUT);
    pinMode(Pin::lcdD6, INPUT);
    pinMode(Pin::lcdD7, INPUT);
}

void sleep(void) {
    D("OWB A");
    D(F(" status: ")); D(sma.owbA.status);
    D(F(" x10 Temp: ")); D(sma.owbA.x10degrees);
    D(F(" 1hSMA: ")); D(sma.owbA.oneHour.value());
    D(F(" 4hSMA: ")); DL(sma.owbA.fourHour.value());
    D("OWB B");
    D(F(" status: ")); D(sma.owbB.status);
    D(F(" x10 Temp: ")); D(sma.owbB.x10degrees);
    D(F(" 1hSMA: ")); D(sma.owbB.oneHour.value());
    D(F(" 4hSMA: ")); DL(sma.owbB.fourHour.value());
    all_pins_down();
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    power_all_disable();  // switch off all internal devices (PWM, ADC, etc.)
    sleep_enable(); // Enable ISR
    sleep_mode(); // ZZzzzzzzzz
    sleep_disable(); // Woke up, disable ISR
    power_all_enable(); // switch on all internal devices
    all_pins_up();
}

void update_time(void) {
    currentTime = millis();
    for (Millis c=0; c < sleepCycleCounter; c++)
        currentTime += wdtSleep8;
}

void updateOneHour(TimedEvent* timed_event) {
    D(F("1H SMA append: ")); D(sma.owbA.x10degrees);
    D(F(" ")); DL(sma.owbB.x10degrees);
    if (sma.owbA.x10degrees != Owb::checkStatus)
        sma.owbA.oneHour.append(sma.owbA.x10degrees);
    if (sma.owbB.x10degrees != Owb::checkStatus)
        sma.owbB.oneHour.append(sma.owbB.x10degrees);
}

void updateFourHour(TimedEvent* timed_event) {
    D(F("4H SMA append: ")); D(sma.owbA.x10degrees);
    D(F(" ")); DL(sma.owbB.x10degrees);
    if (sma.owbA.x10degrees != Owb::checkStatus)
        sma.owbA.fourHour.append(sma.owbA.x10degrees);
    if (sma.owbB.x10degrees != Owb::checkStatus)
        sma.owbB.fourHour.append(sma.owbB.x10degrees);
}

void updateWake(TimedEvent* timed_event) {
    sleep();
    DL("");
    DL("");
    update_time();
    timed_event->reset();
    D(F("Wake up: ")); D(millis());
    D(F("ms sleep #: ")); D(sleepCycleCounter);
    D(F(" adjusted Time: ")); D(currentTime); DL(F("ms"));
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
    WDTCSR = (1<<WDP0) | (1<<WDP3); // 8.0 seconds
    // Enable the WD interrupt (note no reset).
    WDTCSR |= _BV(WDIE);
    interrupts();

    all_pins_up();
    DL(F("Setup..."));

    while (sma.owbA.status != OwbStatus::converting) {
        //D(F("OWB A status: ")); DL(sma.owbA.status);
        // Timers expect conversion already started
        sma.owbA.status = start_conversion(sma.owbA.bus);
        //D(F("OWB A status: ")); DL(sma.owbA.status);
    }
    while (sma.owbB.status != OwbStatus::converting) {
        //D(F("OWB B status: ")); DL(sma.owbB.status);
        sma.owbB.status = start_conversion(sma.owbB.bus);
        //D(F("OWB B status: ")); DL(sma.owbB.status);
    }
    update_time();
    DL(F("Looping..."));
}

void loop(void) {
    static TimedEvent oneHourUpdate(currentTime,
                                    tempSmaSample,
                                    updateOneHour);
    static TimedEvent fourHourUpdate(currentTime,
                                     tempSmaSample,
                                     updateFourHour);
    static TimedEvent wakeUpdate(currentTime,
                                 wakeTime,
                                 updateWake);
    update_time();
    if (sma.owbA.status == OwbStatus::converting &&
        conversion_done(sma.owbA.bus)) {
        sma.owbA.status = OwbStatus::complete;
        sma.owbA.status = get_temp(sma.owbA.bus,
                                   sma.owbA.x10degrees, celsius);
        sma.owbA.status = start_conversion(sma.owbA.bus);
    }
    if (sma.owbB.status == OwbStatus::converting &&
        conversion_done(sma.owbB.bus)) {
        sma.owbB.status = OwbStatus::complete;
        sma.owbB.status = get_temp(sma.owbB.bus,
                                   sma.owbB.x10degrees, celsius);
        sma.owbB.status = start_conversion(sma.owbB.bus);
    }
    update_time();
    oneHourUpdate.update();
    update_time();
    fourHourUpdate.update();
    update_time();
    wakeUpdate.update();
}
