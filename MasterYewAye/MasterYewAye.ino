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

ISR(WDT_vect) {
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
    uint8_t romBuff[8] = {0};
    uint8_t ramBuff[9] = {0};
    uint8_t present = owb.search(romBuff);
    //D(1);
    if (!present)
        return OwbStatus::notFound;
    //D(2);
    if (OneWire::crc8(romBuff, 7) != romBuff[7])
        return OwbStatus::romBadCrc;
    present = owb.reset();  // prepare for next command
    //D(3);
    if (present == 0)
        return OwbStatus::busError;
    //D(4);
    bool type_s = false;
    if (romBuff[0] == 0x10)
        type_s = true;
    else if ((romBuff[0] == 0x28) || (romBuff[0] == 0x22))
        type_s = false;
    else
        return OwbStatus::notDs18x20;
    //D(5);
    owb.select(romBuff);
    owb.write(0xBE, 1);    // Read Scratchpad
    owb.read_bytes(ramBuff, 9);
    if (OneWire::crc8(ramBuff, 8) != ramBuff[8])
        return OwbStatus::ramBadCrc;
    // If resolution < 12, set it and go back to converting while EEProm updates
    if (!type_s and (((ramBuff[4] & 0x60) >> 5) + 9) != 12) {
        DL("Setting up 12-bit resolution");
        ramBuff[2] = 0; ramBuff[3] = 0; ramBuff[4] = (12 - 9) << 5;
        owb.reset();
        owb.select(romBuff);
        owb.write(0x4E, 1);  // write scratchpad
        owb.write_bytes(ramBuff, 3);
        owb.reset();
        owb.select(romBuff);
        owb.write(0x48);  // save to EEProm
        return OwbStatus::converting;
    }
    //D(6);
    x10degrees = (ramBuff[1] << 8) | ramBuff[0];
    if (type_s) {
        x10degrees = x10degrees << 3; // 9 bit resolution default
        if (ramBuff[7] == 0x10)
            // "count remain" gives full 12 bit resolution
            x10degrees = (x10degrees & 0xFFF0) + 12 - ramBuff[6];
    } else {
        // default is 12 bit resolution, 750 ms conversion time
        uint8_t precision = (ramBuff[4] & 0x60);
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

void owb_power(bool on=true) {
    static bool alreadyOn = false;
    if (!on) {  // no harm in turning off twice
        digitalWrite(Pin::owbMosfet, LOW);  // mosfet
        pinMode(Pin::owbMosfet, INPUT);
        pinMode(Pin::owbA, INPUT);
        pinMode(Pin::owbB, INPUT);
        alreadyOn = false;
    } else if (!alreadyOn) {  // turning on twice can cause problms
        pinMode(Pin::owbA, INPUT); // external pull-up
        pinMode(Pin::owbB, INPUT); // external pull-up
        pinMode(Pin::owbMosfet, OUTPUT);
        digitalWrite(Pin::owbMosfet, HIGH); // low-side mosfet
        alreadyOn = true;
    }
}

void lcd_power(bool on=true) {
    static bool alreadyOn = false;
    if (!on) {
        lcd.clear();
        delay(1);
        digitalWrite(Pin::lcdMosfet, LOW);  // mosfet off
        pinMode(Pin::lcdMosfet, INPUT);  // disconnect
        pinMode(Pin::lcdBL, INPUT);
        pinMode(Pin::lcdRS, INPUT);
        pinMode(Pin::lcdEN, INPUT);
        pinMode(Pin::lcdD4, INPUT);
        pinMode(Pin::lcdD5, INPUT);
        pinMode(Pin::lcdD6, INPUT);
        pinMode(Pin::lcdD7, INPUT);
        alreadyOn = false;
    } else if (!alreadyOn) {
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
        delay(1);
        lcd.begin(lcdCols, lcdRows);
        lcd.clear();
        alreadyOn = true;
    }
}

void arf_power(bool on=true) {
    static bool alreadyOn = false;
    if (!on) {
        digitalWrite(Pin::arfMosfet, LOW);  // mosfet off
        pinMode(Pin::arfMosfet, INPUT); // disconnect
        pinMode(Pin::battSense, INPUT); // NO SHORT!
        alreadyOn = false;
    } else if (!alreadyOn) {
        pinMode(Pin::battSense, INPUT);
        pinMode(Pin::arfMosfet, OUTPUT);
        digitalWrite(Pin::arfMosfet, HIGH);  // low-side mosfet
        alreadyOn = true;
    }
}

void enc_power(bool on=true) {
    static bool alreadyOn = false;
    if (!on) {
        pinMode(Pin::encA, INPUT); // disable required pullups
        pinMode(Pin::encC, INPUT);
        pinMode(Pin::button, INPUT);
        EIMSK &= ~((1<<INT0) | (1<<INT1));  // switch INT1/INT0 off
        alreadyOn = false;
    } else if (!alreadyOn) {
        EIMSK |= ((1<<INT0) | (1<<INT1));  //  switch INT1/INT0 on
        pinMode(Pin::encA, INPUT_PULLUP);
        pinMode(Pin::encC, INPUT_PULLUP);
        pinMode(Pin::button, INPUT_PULLUP);
        alreadyOn = true;
    }
}

#ifdef DEBUG
    void ser_power(bool on=true) {
        static bool alreadyOn = false;
        if (!on) {
            DL(F("Going to sleep..."));
            Serial.flush();
            delay(10); // flush needs more time
            Serial.end();
            alreadyOn = false;
        } else if (!alreadyOn) {
            pinMode(Pin::serialRx, OUTPUT);
            pinMode(Pin::serialTx, OUTPUT);
            Serial.begin(Pin::serialBaud);
            digitalWrite(Pin::serialRx, HIGH);
            digitalWrite(Pin::serialRx, HIGH);  // Prevents noise on line
            delay(1);  // time to settle
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
                Serial.flush();
            }
            pinsPrinted = true;
            alreadyOn = true;
        }
    }
#endif // DEBUG

void all_pins_up(void){
    #ifdef DEBUG
        ser_power(true);
    #endif // DEBUG
    cli(); // disable interrupts
    owb_power(true);
    enc_power(true);
    sei(); // enable interrupts
    // Devices need power-on time to settle
}

void all_pins_down(void) {
    #ifdef DEBUG
        ser_power(false);
    #endif // DEBUG
    cli(); // disable interrupts
    enc_power(false);
    arf_power(false);
    lcd_power(false);
    owb_power(false);
    sei(); // enable interrupts
}

void sleep(void) {
    uint8_t sleepCyclesRemaining = sleepCycleMultiplier;
    all_pins_down();
    power_all_disable();  // switch off all internal devices (PWM, ADC, etc.)
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    cli(); // disable interrupts
    while (sleepCyclesRemaining) {
        sleepCyclesRemaining--;
        sleep_enable(); // Enable ISR
        sleep_bod_disable(); // Must happen _right_ before sleep_mode();
        sei(); // enable interrupts
        sleep_cpu(); // ZZzzzzzzzz
        sleep_disable(); // Woke up, disable ISR
        cli(); // disable interrupts
    }
    sei();  // enable interrupts
    power_all_enable(); // switch on all internal devices
    all_pins_up();
}

void update_time(void) {
    currentTime = millis();
    for (Millis c=0; c < sleepCycleCounter; c++)
        currentTime += wdtSleep8;
}

void updateTemps(void) {
    if (sma.owbA.status == OwbStatus::converting &&
        conversion_done(sma.owbA.bus)) {
        sma.owbA.status = OwbStatus::complete;
        sma.owbA.status = get_temp(sma.owbA.bus,
                                   sma.owbA.x10degrees, celsius);
        sma.owbA.status = start_conversion(sma.owbA.bus);
    } else
        sma.owbA.status = start_conversion(sma.owbA.bus);
    if (sma.owbB.status == OwbStatus::converting &&
        conversion_done(sma.owbB.bus)) {
        sma.owbB.status = OwbStatus::complete;
        sma.owbB.status = get_temp(sma.owbB.bus,
                                   sma.owbB.x10degrees, celsius);
        sma.owbB.status = start_conversion(sma.owbB.bus);
    } else
        sma.owbB.status = start_conversion(sma.owbB.bus);
}

void wait_conversion(void) {
    DL(F("Waiting on A"));
    while (sma.owbA.status == OwbStatus::converting)
        if (conversion_done(sma.owbA.bus))
            break;
    DL(F("Waiting on B"));
    while (sma.owbB.status == OwbStatus::converting)
        if (conversion_done(sma.owbB.bus))
            break;
    updateTemps();
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

void updateOneHour(TimedEvent* timed_event) {
    wait_conversion();
    D(F("1H SMA append: ")); D(sma.owbA.x10degrees);
    D(F(" / ")); DL(sma.owbB.x10degrees);
    sma_append(sma.owbA.oneHour, sma.owbA.x10degrees);
    sma_append(sma.owbB.oneHour, sma.owbB.x10degrees);
}

void updateFourHour(TimedEvent* timed_event) {
    wait_conversion();
    D(F("4H SMA append: ")); D(sma.owbA.x10degrees);
    D(F(" / ")); DL(sma.owbB.x10degrees);
    sma_append(sma.owbA.fourHour, sma.owbA.x10degrees);
    sma_append(sma.owbB.fourHour, sma.owbB.x10degrees);
}

void updateWake(TimedEvent* timed_event) {
    if (wakeCounter <= 0) {
        lcd.clear();
        digitalWrite(Pin::lcdBL, LOW);
        uiActivity = false;
        lcdDisplay = false;
        wakeCounter = wakeMinMultiplier;
        sleep();
        enc.write(encValue * 4);  // looses brains after wake up
        DL("");
        update_time();
        timed_event->reset();
        D(F("Wake up: ")); D(millis());
        D(F("ms sleep #: ")); D(sleepCycleCounter);
        D(F(" adjusted Time: ")); D(currentTime); DL(F("ms"));
        // Zap old temperature readings
        sma.owbA.status = OwbStatus::changeDev;
        sma.owbB.status = OwbStatus::changeDev;
        sma.owbA.x10degrees = Owb::checkStatus;
        sma.owbB.x10degrees = Owb::checkStatus;
    } else
        wakeCounter--;
}

void lcdPrintTemp(int16_t centi_temp) {
    if (centi_temp >= 0)
        lcd.print(" ");
    lcd.print(centi_temp / 10);
    lcd.print(".");
    lcd.print(centi_temp - centi_temp / 10 * 10);
    lcd.print("\xDF"); // degree's symbol
}

void updateLcd(TimedEvent* timed_event) {
    #ifdef DEBUG
    D("\nOWB A");
    D(F(" status: ")); D(sma.owbA.status);
    D(F(" x10 Temp: ")); D(sma.owbA.x10degrees);
    D(F(" 1hSMA: ")); D(sma.owbA.oneHour.value());
    D(F(" 4hSMA: ")); DL(sma.owbA.fourHour.value());
    D("OWB B");
    D(F(" status: ")); D(sma.owbB.status);
    D(F(" x10 Temp: ")); D(sma.owbB.x10degrees);
    D(F(" 1hSMA: ")); D(sma.owbB.oneHour.value());
    D(F(" 4hSMA: ")); DL(sma.owbB.fourHour.value());
    #endif // DEBUG
    if (!lcdDisplay)
        return;
    lcd_power(true);
    lcd.setCursor(0, 0);
    switch (encValue) {
        case EncState::current: lcd.print("Current Temp.   "); break;
        case EncState::one:     lcd.print("1 Hour SMA Temp."); break;
        case EncState::four:    lcd.print("4 Hour SMA Temp."); break;
    }
    lcd.setCursor(0, 1);
    lcd.print("A:      B:      ");
    lcd.setCursor(2, 1);
    switch (encValue) {
        case EncState::current: lcdPrintTemp(sma.owbA.x10degrees); break;
        case EncState::one: lcdPrintTemp(sma.owbA.oneHour.value()); break;
        case EncState::four: lcdPrintTemp(sma.owbA.fourHour.value()); break;
    }
    lcd.setCursor(10, 1);
    switch (encValue) {
        case EncState::current: lcdPrintTemp(sma.owbB.x10degrees); break;
        case EncState::one: lcdPrintTemp(sma.owbB.oneHour.value()); break;
        case EncState::four: lcdPrintTemp(sma.owbB.fourHour.value()); break;
    }
    digitalWrite(Pin::lcdBL, HIGH);
}

void updateEnc(TimedEvent* timed_event) {
    int32_t newPosition = enc.read() / 4;

    if (newPosition > lowByte(encMinMax)) {
      newPosition = highByte(encMinMax);
      enc.write(newPosition * 4);
    } else if (newPosition < highByte(encMinMax)) {
       newPosition = lowByte(encMinMax);
       enc.write(newPosition * 4);
    }

    if (newPosition != encValue) {
      encValue = newPosition;
      D(F("Encoder select: ")); DL(encValue);
      uiActivity = true;
    }
}

void clickHandler(Button& source) {
    if (buttonState == ButtonState::held) {
        D(F("Button click: "));
        buttonState = ButtonState::click;
        uiActivity = true;
        delay(100);
    }
}

void holdHandler(Button& source) {
    D(F("Button Held for: ")); DL(source.holdTime());
    buttonState = ButtonState::held;
    wakeCounter = -1; // back to sleep
}

void setup(void) {
    // timing critical
    noInterrupts();

    /* Set up watchdog timer to wake up chip from power-down sleep
       after (temperature & voltage dependant) "8 seconds"  */
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

/*
    button.pressHandler(pressHandler);
    button.releaseHandler(releaseHandler);
*/
    button.clickHandler(clickHandler);
    button.holdHandler(holdHandler, buttonHoldTime);

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
    enc.write(0);
    update_time();
    lcd.clear();
    digitalWrite(Pin::lcdBL, LOW);
    D(F("Start: ")); D(millis());
    D(F("ms sleep #: ")); D(sleepCycleCounter);
    D(F(" adjusted Time: ")); D(currentTime); DL(F("ms"));
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
    static TimedEvent encUpdate(currentTime,
                                encTime,
                                updateEnc);
    static TimedEvent lcdUpdate(currentTime,
                                lcdTime,
                                updateLcd);
    updateTemps(); update_time();
    oneHourUpdate.update(); update_time();
    fourHourUpdate.update(); update_time();
    button.process(); update_time();
    encUpdate.update(); update_time();
    lcdUpdate.update(); update_time();
    if (uiActivity) {
        // don't check again, unless button/enc activity
        uiActivity = false;
        // Do start displaying stuff now
        lcdDisplay = true;
        // clear last round button's status
        buttonState = ButtonState::none;
        // delay going to sleep
        wakeCounter = wakeMaxMultiplier;
    }
    wakeUpdate.update();
}
