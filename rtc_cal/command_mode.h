#ifndef COMMAND_MODE_H
#define COMMAND_MODE_H

#include <Rtc_Pcf8563.h>
#include "eeprom.h"
#include "clock.h"
#include "line_buff.h"

/*
 * Globals
 */

extern Rtc_Pcf8563 rtc;

/*
 * Functions
 */

void countTicks(uint32_t* ticks, uint8_t pin) {
    // Critical section
    noInterrupts();
    bool startState = digitalRead(pin);
    while (*ticks) {
        while (digitalRead(pin) == startState)
            {};
        while (digitalRead(pin) != startState)
            {};
        (*ticks)--;
    }
    interrupts();
    // End critical section
}

void printTimeNextSecond(void) {
    rtc.getDateTime();
    int8_t oldSeconds = rtc.getSecond();
    while (oldSeconds == rtc.getSecond())
        rtc.getDateTime();
    Serial.print(rtc.formatDate());
    Serial.print(" ");
    Serial.print(rtc.formatTime());
    if (rtc.getVoltLow()) {
        Serial.println("?");
    } else
        Serial.println();
}

void serialTimeSeconds(uint32_t seconds) {
    printTimeNextSecond();
    Serial.print(F("Timing "));
    Eep_type eep(defaults, &eemem);
    EepromData* eepromData = eep.data();
    seconds *= eepromData->clkoutHz;
    Serial.print(seconds, DEC);
    Serial.println(F(" RTC ticks"));
    // Timing starts now
    Serial.println("START");
    Serial.flush();
    countTicks(&seconds, eepromData->clkoutPin);
    Serial.println("STOP!");
    Serial.flush();
    // Timing ends now
    rtc.getDateTime();
    Serial.print(rtc.formatDate());
    Serial.print(" ");
    Serial.print(rtc.formatTime());
}

uint8_t readByte(void) {
    while (!Serial.available())
        delay(10);
    return Serial.read();
}

void setOffset(void) {
    LineBuff<true, true, 16> line;
    Serial.print(F("Base frequency is: "));
    Eep_type eep(defaults, &eemem);
    EepromData* eepromData = eep.data();
    Serial.print(eepromData->clkoutHz);
    Serial.print(F(" + MiliHertz Offset: "));
    Serial.print(eepromData->hzOffset);
    Serial.println();

    Serial.print(F("Enter expected ticks: "));
    char number[16] = {0};  // uint32_t needs 10-chars
    strncpy(number, line.gotLineBlock(), 15);
    line.flush();
    Serial.println();
    char *endptr;
    clock::epoch_type expected_ticks = strtol(number, &endptr, 10);
    if (endptr == number) {
        Serial.println(F("Error parsing number"));
        return;
    }

    Serial.print(F("Enter actual ticks: "));
    strncpy(number, line.gotLineBlock(), 15);
    line.flush();
    Serial.println();
    clock::epoch_type actual_ticks = strtol(number, &endptr, 10);
    if (endptr == number) {
        Serial.println(F("Error parsing number"));
        return;
    }

    clock::freq_type actual_hertz = 0.0;
    clock::freq_type expected_hertz = eepromData->clkoutHz;  // SLOW!
    clock::Clock::calcHertz(expected_ticks, expected_hertz,
                            actual_ticks, actual_hertz);

    expected_hertz = actual_hertz - expected_hertz;
    eepromData->hzOffset = expected_hertz * 1000.0;

    eep.save();
    // FIXME: print details
    Serial.print(F(" MiliHertz Offset is now: "));
    Serial.print(eepromData->hzOffset);
}

int16_t valiDateTime(char *input, int16_t min, int16_t max) {
    // make range check easier
    min--;
    max++;
    char number[4] = {0};
    char* endptr = number;
    int16_t result = -1;
    memcpy(number, input, 2);
    result = constrain(strtol(number, &endptr, 10), min, max);
    if ((*endptr != '\0') || (result == min) || (result == max))
        return -1;  // invalid input
    return result;  // valid input
}

void setDateTime(void) {
    Serial.print(F("Enter date/time in 'yymmddhhmmss' format: "));
    char datetime[14] = {0};
    LineBuff<true, true, 14> line;
    strncpy(datetime, line.gotLineBlock(), 13);
    Serial.println();
    if (strnlen(datetime, 14) != 12) {
        Serial.print(F("Input too long ("));
        Serial.print(strnlen(datetime, 14), DEC);
        Serial.println(F("/12)"));
        return;  // invalid input
    }
    line.flush();

    Eep_type eep(defaults, &eemem);
    EepromData* eepromData = eep.data();
    clock::TM now = eepromData->setTime;
    // year
    now.tm_year = valiDateTime(datetime, 0, 99);
    if (now.tm_year == -1) {
        Serial.print(F("Inv. year: "));
        Serial.println(now.tm_year);
        return;
    }

    // month
    now.tm_mon = valiDateTime(datetime+2, 1, 12);
    if (now.tm_mon == -1) {
        Serial.print(F("Inv. month:"));
        Serial.print(now.tm_mon);
        return;
    }

    // day
    now.tm_mday = valiDateTime(datetime+4, 1, now.daysInMonth());
    if (now.tm_mday == -1) {
        Serial.print(F("Month: "));
        Serial.println(now.tm_mon);
        Serial.print(F("Inv. day: "));
        Serial.println(now.tm_mday);
        Serial.print(F("Days in month: "));
        Serial.println(now.daysInMonth());
        return;
    }

    // hour
    now.tm_hour = valiDateTime(datetime+6, 0, 23);
    if (now.tm_hour == -1) {
        Serial.print(F("Inv. hour: "));
        Serial.println(now.tm_hour);
        return;
    }

    // minute
    now.tm_min = valiDateTime(datetime+8, 0, 59);
    if (now.tm_min == -1) {
        Serial.print(F("Inv. min"));
        Serial.println(now.tm_min);
        return;
    }

    // second
    now.tm_sec = valiDateTime(datetime+10, 0, 60);
    if (now.tm_sec == -1) {
        Serial.print(F("Inv. sec"));
        Serial.println(now.tm_sec);
        return;
    }

    now.tm_wday = now.whatWeekday();
    now.tm_yday = now.daysInYear();
    now.tm_ticks = 0;
    now.tm_hertz = eepromData->clkoutHz;

    Serial.println(F("---Enter to synchronize---"));
    now.toSerial();
    while (!Serial.available()) delay(10);
    uint8_t nextByte = Serial.read();
    if (nextByte == '\r' || nextByte == '\n') {
        rtc.setDateTime(now.tm_mday, now.tm_wday, now.tm_mon, 0,
                        now.tm_year, now.tm_hour,
                        now.tm_min, now.tm_sec);
        memcpy(&eepromData->setTime, &now, sizeof(eepromData->setTime));
        eep.save();
    } else
        Serial.println(F("Aborted"));
}

void dumpEeprom() {
    Eep_type eep(defaults, &eemem);
    EepromData* eepromData = eep.data();
    Serial.print(F("Eeprom address: ")); Serial.println(reinterpret_cast<uint16_t>(&eemem));
    Serial.print(F("\tclkoutPin: ")); Serial.println(eepromData->clkoutPin);
    Serial.print(F("\tclkoutMode: ")); Serial.println(eepromData->clkoutMode);
    Serial.print(F("\tclkoutIntNo: ")); Serial.println(eepromData->clkoutIntNo);
    Serial.print(F("\tclkoutIntMode: ")); Serial.println(eepromData->clkoutIntMode);
    Serial.print(F("\tintPin: ")); Serial.println(eepromData->intPin);
    Serial.print(F("\tintMode: ")); Serial.println(eepromData->intMode);
    Serial.print(F("\tintIntNo: ")); Serial.println(eepromData->intIntNo);
    Serial.print(F("\tintIntMode: ")); Serial.println(eepromData->intIntMode);
    Serial.print(F("\tclkoutCfg: ")); Serial.println(eepromData->clkoutCfg);
    Serial.print(F("\tclkoutHz: ")); Serial.println(eepromData->clkoutHz);
    Serial.print(F("\thzOffset/1000: ")); Serial.print(eepromData->hzOffset);
        Serial.print(F("  ("));
        Serial.print(eepromData->clkoutHz + (eepromData->hzOffset / 1000.0));
        Serial.println(F("Hz)"));
    Serial.print(F("\tcmdModeWindow: ")); Serial.println(eepromData->cmdModeWindow);
    Serial.print(F("\tcmdModeCode: ")); Serial.println(eepromData->cmdModeCode);
    clock::TM set_time = eepromData->setTime;
    Serial.print(F("\tsetTime: ")); set_time.toSerial();
}

void commandMode(void) {
    while (true) {
        char command[16] = {0};
        LineBuff<true, true, 16> line;
        Serial.println();
        Serial.print(F(">>> "));
        // blocking
        strncpy(command, line.gotLineBlock(), 15);
        Serial.println();
        if (memcmp(command, "help", 4) == 0)
            Serial.println(F("Commands:  exit, help, dump, echo, now, time1H, time1M, "
                             "time 6H, setOffset, setDT, clearVL"));
        else if (memcmp(command, "dump", 4) == 0)
            dumpEeprom();
        else if (memcmp(command, "now", 3) == 0)
            printTimeNextSecond();
        else if (memcmp(command, "clearVL", 7) == 0)
            rtc.clearVoltLow();
        else if (memcmp(command, "setOffset", 9) == 0)
            setOffset();
        else if (memcmp(command, "time1H", 6) == 0)
            serialTimeSeconds(60UL * 60UL);
        else if (memcmp(command, "time6H", 6) == 0)
            serialTimeSeconds(60UL * 60UL * 6);
        else if (memcmp(command, "time1M", 6) == 0)
            serialTimeSeconds(60UL);
        else if (memcmp(command, "setDT", 5) == 0)
            setDateTime();
        else if (memcmp(command, "echo", 4) == 0) {
            Serial.print(F("Send one byte: 0x"));
            Serial.println(readByte(), HEX);
        } else if (memcmp(command, "exit", 4) == 0)
            break;
        else
            Serial.println(F("Unknown/invalid command, type 'help' for help."));
        line.flush();
    }
}

bool commandModeEnter(void) {
    Serial.setTimeout(500);
    char buffer[16] = {0};
    Eep_type eep(defaults, &eemem);
    EepromData* eepromData = eep.data();
    if (Serial.available() && (Serial.peek() == eepromData->cmdModeCode[0]))
        Serial.readBytes(buffer, 15);
    Serial.setTimeout(1000);  // back to default
    if (strncmp(buffer,
                eepromData->cmdModeCode,
                strlen(eepromData->cmdModeCode)) == 0)
        return true;
    else
        return false;
}

#endif // COMMAND_MODE_H
