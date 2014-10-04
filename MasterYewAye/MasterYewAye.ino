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
        DL("OWB power up");
        pinMode(Pin::owbA, INPUT); // external pull-up
        pinMode(Pin::owbB, INPUT); // external pull-up
        pinMode(Pin::owbMosfet, OUTPUT);
        digitalWrite(Pin::owbMosfet, HIGH); // low-side mosfet
        delay(100);  // powerup time
        alreadyOn = true;
    }
}

void lcd_power(bool on=true) {
    static bool alreadyOn = false;
    if (!on) {
        digitalWrite(Pin::lcdMosfet, LOW);  // mosfet off
        pinMode(Pin::lcdMosfet, INPUT); // no leaking
        pinMode(Pin::lcdBL, INPUT);
        pinMode(Pin::lcdRS, INPUT);
        pinMode(Pin::lcdEN, INPUT);
        pinMode(Pin::lcdD4, INPUT);
        pinMode(Pin::lcdD5, INPUT);
        pinMode(Pin::lcdD6, INPUT);
        pinMode(Pin::lcdD7, INPUT);
        alreadyOn = false;
    } else if (!alreadyOn) { // turn on
        DL("LCD power up");
        pinMode(Pin::lcdBL, OUTPUT);
        pinMode(Pin::lcdRS, OUTPUT);
        pinMode(Pin::lcdEN, OUTPUT);
        pinMode(Pin::lcdD4, OUTPUT);
        pinMode(Pin::lcdD5, OUTPUT);
        pinMode(Pin::lcdD6, OUTPUT);
        pinMode(Pin::lcdD7, OUTPUT);
        pinMode(Pin::lcdMosfet, OUTPUT);
        digitalWrite(Pin::lcdBL, HIGH);  // controled by mosfet
        digitalWrite(Pin::lcdMosfet, HIGH);  // low-side mosfet
        delay(100);
        lcd.begin(lcdCols, lcdRows);
        alreadyOn = true;
    }
}

void arf_power(bool on=true) {
    static bool alreadyOn = false;
    if (!on) {
        DL("AREF power down");
        digitalWrite(Pin::arfMosfet, LOW);  // mosfet off
        pinMode(Pin::arfMosfet, INPUT); // disconnect
        pinMode(Pin::battSense, INPUT); // NO SHORT!
        alreadyOn = false;
    } else if (!alreadyOn) {
        DL("AREF power up");
        pinMode(Pin::battSense, INPUT);
        pinMode(Pin::arfMosfet, OUTPUT);
        digitalWrite(Pin::arfMosfet, HIGH);  // low-side mosfet
        delay(50);  // time for cap. discharge
        analogRead(Pin::battSense);
        delay(50);
        analogRead(Pin::battSense);
        delay(100);
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
        DL("Encoder power up");
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
            delay(100); // flush needs more time
            Serial.end();
            alreadyOn = false;
        } else if (!alreadyOn) {
            DL("Serial power up");
            pinMode(Pin::serialRx, OUTPUT);
            pinMode(Pin::serialTx, OUTPUT);
            Serial.begin(Pin::serialBaud);
            digitalWrite(Pin::serialRx, HIGH);
            digitalWrite(Pin::serialTx, HIGH);  // Prevents noise on line
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
                                        ((tempSmaFour / tempSmaSample) * 4) +
                                        ((tempSmaTwelve / tempSmaOne * 4)));
                DL();
                Serial.flush();
            }
            pinsPrinted = true;
            alreadyOn = true;
        }
    }
#endif // DEBUG

void all_pins_down(void) {
    enc_power(false);
    arf_power(false);
    lcd_power(false);
    owb_power(false);
    #ifdef DEBUG
        ser_power(false);
    #endif // DEBUG
}

unsigned int readBattery(void) {
    unsigned int raw=0;

    battMvPrev = battMv;
    arf_power(true); // sssslllllooooowwww
    raw = analogRead(Pin::battSense);
    D(F("    battery raw: ")); DL(raw);
    battMv = map(raw, 0, 1023,
                 PowerSense::offMv,
                 PowerSense::arfMv + PowerSense::offMv);
    D(F("    battery offset: ")); DL(battMv);
    battMv /= PowerSense::powDivFact;  // remove voltage divider
    D(F("    battery scaled: ")); DL(battMv);
    if (battMv < (PowerSense::battEmpty - 200))
        battMv = 0;  // on external power
    arf_power(false);
    return battMv;
}

uint8_t percentBattery(void) {
    if (battMv >= PowerSense::battFull)
        return 100;
    else if (battMv <= PowerSense::battEmpty)
        return 0;
    else {
        long actual = battMv - PowerSense::battEmpty;
        long maximum = PowerSense::battFull - PowerSense::battEmpty;
        return (actual * 100) / maximum;
    }
}

void update_time(void) {
    currentTime = millis();
    // TODO: when (sleepCycleCounter * wdtSleep8) => -1UL - wdtSleep8
    //       reset sleepCycleCounter back to 1;
    // prevent lost-time accumulation from overflowing
    for (Millis c=0; c < sleepCycleCounter; c++)
        currentTime += wdtSleep8;  // okay if this overflows
}

void updateTemps(void) {
    owb_power(true);
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
    updateTemps();  // power on, start conversion
    DL(F("Waiting on A"));
    while (sma.owbA.status == OwbStatus::converting)
        if (conversion_done(sma.owbA.bus))
            break;
    DL(F("Waiting on B"));
    while (sma.owbB.status == OwbStatus::converting)
        if (conversion_done(sma.owbB.bus))
            break;
    updateTemps(); // Grab actual readings
    owb_power(false); // no need to stay on
}

uint16_t sma_append(SimpleMovingAvg& sma, int16_t x10degrees) {
    if (x10degrees != Owb::checkStatus) {
        D(F("SMA Appended: ")); DL(x10degrees);
        return sma.append(x10degrees);
    } else {
        DL(F("Not recording faulty temperature!"));
        return Owb::checkStatus;
    }
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
        sleep_disable();
        pinMode(Pin::button, INPUT_PULLUP);  // logic is inverted
        if (digitalRead(Pin::button) == LOW) { // button is pressed
            sleepCyclesRemaining = 0; // wake up
        } else
            pinMode(Pin::button, INPUT);
    }
    sei();  // enable interrupts
    power_all_enable(); // switch on all internal devices
    #ifdef DEBUG
        ser_power(true);
    #endif // DEBUG
    enc_power(true);
}

void updateWake(TimedEvent* timed_event) {
    // wakeCounter allows UI activity to force stay awake longer
    if (wakeCounter <= 0) {
        lcd.clear();
        sleep();  // ZZZzzzzzzz
        lcdDisplay = false;
        lcdRefresh = false;
        wakeCounter = wakeMinMultiplier;
        enc.write(encValue * 4);  // looses brains after wake up
        DL("");
        update_time();
        timed_event->reset();  // count from now, not when slept
        // Zap old temperature readings
        sma.owbA.status = OwbStatus::none;
        sma.owbB.status = OwbStatus::none;
        sma.owbA.x10degrees = Owb::checkStatus;
        sma.owbB.x10degrees = Owb::checkStatus;
        D(F("Wake up: ")); D(millis());
        D(F("ms sleep #: ")); D(sleepCycleCounter);
        D(F(" adjusted Time: ")); D(currentTime); DL(F("ms"));
    } else
        wakeCounter--;
}

void updateBattery(TimedEvent* timed_event) {
    readBattery(); // updates battMv and battMvPrev
    if (battMv == 0)
        DL(F("Battery: On External Power"));
    else {
        D(F("Battery percent: ")); D(percentBattery());
        DL(F("% remaining"));
        D(F("Battery  mV: ")); DL(battMv);
        D(F("mV draw: "));
        D((abs((double)battMvPrev - (double)battMv) /
            (batterySample / 1000.0))); // ms -> s
        DL(" per second");
    }
}

void updateOneHour(TimedEvent* timed_event) {
    wait_conversion();
    D(currentTime); D(F(" 1H SMA append: ")); D(sma.owbA.x10degrees);
    D(F(" / ")); DL(sma.owbB.x10degrees);
    sma_append(sma.owbA.oneHour, sma.owbA.x10degrees);
    sma_append(sma.owbB.oneHour, sma.owbB.x10degrees);
}

void updateFourHour(TimedEvent* timed_event) {
    wait_conversion();
    D(currentTime); D(F(" 4H SMA append: ")); D(sma.owbA.x10degrees);
    D(F(" / ")); DL(sma.owbB.x10degrees);
    sma_append(sma.owbA.fourHour, sma.owbA.x10degrees);
    sma_append(sma.owbB.fourHour, sma.owbB.x10degrees);
}

void updateTwelveHour(TimedEvent* timed_event) {
    wait_conversion();
    D(currentTime); D(F(" 12H SMA append: ")); D(sma.owbA.x10degrees);
    D(F(" / ")); DL(sma.owbB.x10degrees);
    sma_append(sma.owbA.twelveHour, sma.owbA.x10degrees);
    sma_append(sma.owbB.twelveHour, sma.owbB.x10degrees);
}

void lcdPrintTemp(int16_t centi_temp) {
    if (centi_temp >= 0)
        lcd.print(" ");
    if (centi_temp == -850)
        lcd.print("???");
    else {
        lcd.print(centi_temp / 10);
        lcd.print(".");
        lcd.print(centi_temp - centi_temp / 10 * 10);
    }
    lcd.print("\xDF"); // degree's symbol
}

void lcdPrintBattMv(void) {
    if (battMv < (PowerSense::battEmpty - 200))
        lcd.print("  Ext  ");
    else {
        lcd.print(battMv / 1000);
        lcd.print(".");
        lcd.print(battMv - ((battMv / 1000) * 1000));
    }
}

void updateLcdData(void) {  // Only update values
    lcd.setCursor(2, 1);
    switch (encValue) {
        case EncState::current: lcdPrintTemp(sma.owbA.x10degrees); break;
        case EncState::one:     lcdPrintTemp(sma.owbA.oneHour.value()); break;
        case EncState::four:    lcdPrintTemp(sma.owbA.fourHour.value()); break;
        case EncState::twelve:  lcdPrintTemp(sma.owbA.twelveHour.value()); break;
        case EncState::battery: lcdPrintBattMv(); break;
    }
    lcd.setCursor(10, 1);
    switch (encValue) {
        case EncState::current: lcdPrintTemp(sma.owbB.x10degrees); break;
        case EncState::one:     lcdPrintTemp(sma.owbB.oneHour.value()); break;
        case EncState::four:    lcdPrintTemp(sma.owbB.fourHour.value()); break;
        case EncState::twelve:  lcdPrintTemp(sma.owbB.twelveHour.value()); break;
        case EncState::battery: lcd.print(percentBattery()); break;
    }
}

void refreshLcd(void) {  // Paint entire screen
    #ifdef DEBUG
        D(F("wakeCounter: ")); DL(wakeCounter);
        D(F("\nOWB A"));
        D(F(" status: ")); D(sma.owbA.status);
        D(F(" x10 Temp: ")); D(sma.owbA.x10degrees);
        D(F(" 1hSMA: ")); D(sma.owbA.oneHour.value());
        D(F(" 4hSMA: ")); D(sma.owbA.fourHour.value());
        D(F(" 12hSMA: ")); DL(sma.owbA.twelveHour.value());
        D(F("OWB B"));
        D(F(" status: ")); D(sma.owbB.status);
        D(F(" x10 Temp: ")); D(sma.owbB.x10degrees);
        D(F(" 1hSMA: ")); D(sma.owbB.oneHour.value());
        D(F(" 4hSMA: ")); D(sma.owbB.fourHour.value());
        D(F(" 12hSMA: ")); DL(sma.owbB.twelveHour.value());
    #endif // DEBUG
    lcd.setCursor(0, 0);
    switch (encValue) {
        case EncState::current: lcd.print("Current Temp.   "); break;
        case EncState::one:     lcd.print("1 Hour SMA Temp."); break;
        case EncState::four:    lcd.print("4 Hour SMA Temp."); break;
        case EncState::twelve:  lcd.print("12Hour SMA Temp."); break;
        case EncState::battery: lcd.print("Battery Power   "); break;
    }
    lcd.setCursor(0, 1);
    if (encValue != EncState::battery)
        lcd.print("A:      B:      ");
    else
        lcd.print("        V    %  ");
    updateLcdData();
    lcdRefresh = false;
}

void updateLcd(TimedEvent* timed_event) {
    if (!lcdDisplay)
        return;
    lcd_power(true);
    updateTemps(); update_time();
    if (lcdRefresh)
        refreshLcd();  // Re-paint entire screen
    else
        updateLcdData();  // Pain only new values
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
        lcdRefresh = true;
        lcdDisplay = true;
        wakeCounter = wakeMaxMultiplier;
    }
}

void pressHandler(Button& source) {
    D(F("Button press: ")); DL(source.holdTime());
    lcdRefresh = true;
    lcdDisplay = true;
    wakeCounter = wakeMaxMultiplier;
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

    analogReference(EXTERNAL);
    delay(5);  // how long it takes

    #ifdef DEBUG
        ser_power(true);
    #endif // DEBUG
    DL(F("Setup..."));
    enc_power(true);

    button.pressHandler(pressHandler);
    wakeCounter = wakeMaxMultiplier;
    lcdRefresh = true;
    lcdDisplay = true;

    enc.write(0);
    update_time();

    delay(1000);
    DL(F("Reading battery voltage"));
    readBattery();
    delay(1000);
    readBattery();
    D(F("Battery range: ")); D(battMvPrev);
    D(F(" - ")); DL(battMv);

    D(F("Start: ")); DL(millis());
    DL(F("Looping..."));
}

void loop(void) {
    static TimedEvent oneHourUpdate(currentTime,
                                    tempSmaSample,  // 10 minutes
                                    updateOneHour);
    static TimedEvent fourHourUpdate(currentTime,
                                     tempSmaSample,
                                     updateFourHour);
    static TimedEvent twelveHourUpdate(currentTime,
                                       tempSmaOne,  // one hour
                                       updateTwelveHour);
    static TimedEvent batteryUpdate(currentTime,
                                    batterySample,  // 45 minutes
                                    updateBattery);
    static TimedEvent wakeUpdate(currentTime,
                                 wakeTime,
                                 updateWake);
    static TimedEvent encUpdate(currentTime,
                                encTime,
                                updateEnc);
    static TimedEvent lcdUpdate(currentTime,
                                lcdTime,
                                updateLcd);
    batteryUpdate.update(); update_time();
    oneHourUpdate.update(); update_time();
    fourHourUpdate.update(); update_time();
    twelveHourUpdate.update(); update_time();
    button.process(); update_time();
    encUpdate.update(); update_time();
    lcdUpdate.update(); update_time();
    wakeUpdate.update();
}
