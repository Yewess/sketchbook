/*
 *
 * rtc_test.ino -- Serial interface for testing Rtc_Pcf8563
 * Copyright (C) 2014 Chris Evich.  All rights reserved.
 *
 * This program library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later
 * version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
*/

#include <Arduino.h>
#include <Eep.h>
#include <Rtc_Pcf8563.h>
#include <SerialUI.h>

extern TwoWire Wire;

class EepromData {
    public:
    uint32_t serialBaudRate = 115200U;
    uint16_t suiTimeout = 65535; // ms mid-input timeout
    uint16_t suiMaxIdle = 10000; // ms waiting at menu prompt
    uint16_t suiUserTimeout = 999;  // ms between menu-enter check
    char suiInputTerm = '\n';
    uint8_t suiReadIntTries = 3;
    uint16_t rtcClkoutFreq = 32767;  // Hz
    uint8_t rtcClkoutPin = 3;
    uint8_t rtcClkoutPinMode = INPUT_PULLUP;
    uint8_t rtcClkoutVect= 1;
    uint8_t rtcClkoutMode = FALLING;
    uint8_t rtcIntPin = 2;
    uint8_t rtcIntPinMode = INPUT_PULLUP;
    uint8_t rtcIntVect = 0;
    uint8_t rtcIntMode = FALLING;
    bool echoCommands = false;
};

/*
 * Types
 */

const EepromData defaults;
typedef Eep::Eep<EepromData, 3> Eep_type;

/*
 * Menu string constants
 */

SUI_DeclareString(SETUP, "Initializing...");
SUI_DeclareString(LOOP, "Running...");

SUI_DeclareString(GREETING, "Rtc_Pcf8563 Human Interface");
SUI_DeclareString(MAIN_MENU, "Main Menu");

SUI_DeclareString(ZEROCLOCK_KEY, "zeroClock");
SUI_DeclareString(ZEROCLOCK_HELP, "Zero out all fields, disable all alarms");

SUI_DeclareString(CLEARSTATUS_KEY, "clearStatus");
SUI_DeclareString(CLEARSTATUS_HELP, "zero out all timer and alarm settings/state");

SUI_DeclareString(GETDATETIME_KEY, "getDateTime");
SUI_DeclareString(GETDATETIME_HELP, "Retrieve date/time + status");

SUI_DeclareString(SETDATETIME_KEY, "setDateTime");
SUI_DeclareString(SETDATETIME_HELP, "Set the date and time");

SUI_DeclareString(GETALARM_KEY, "getAlarm");
SUI_DeclareString(GETALARM_HELP, "Get the alarm weekday, day, hour and minute");

SUI_DeclareString(ALARMENABLED_KEY, "alarmEnabled");
SUI_DeclareString(ALARMENABLED_HELP, "Show if alarm interrupt is enabled");

SUI_DeclareString(ALARMACTIVE_KEY, "alarmActive");
SUI_DeclareString(ALARMACTIVE_HELP, "Show if alarm is active (alarming)");

SUI_DeclareString(ENABLEALARM_KEY, "enableAlarm");
SUI_DeclareString(ENABLEALARM_HELP, "Enable alarm and interrupt");

SUI_DeclareString(SETALARM_KEY, "setAlarm");
SUI_DeclareString(SETALARM_HELP, "Set alarm weekday, day, hour and minute values");

SUI_DeclareString(CLEARALARM_KEY, "clearAlarm");
SUI_DeclareString(CLEARALARM_HELP, "Clear alarm status and disable interrupt");

SUI_DeclareString(RESETALARM_KEY, "resetAlarm");
SUI_DeclareString(RESETALARM_HELP, "Clear alarm status only");

//SUI_DeclareString(GETTIMER_KEY, "getTimer");
//SUI_DeclareString(GETTIMER_HELP, "Get the Timer current countdown value");

SUI_DeclareString(TIMERENABLED_KEY, "TimerEnabled");
SUI_DeclareString(TIMERENABLED_HELP, "Show if Timer interrupt is enabled");

SUI_DeclareString(TIMERACTIVE_KEY, "TimerActive");
SUI_DeclareString(TIMERACTIVE_HELP, "Show if Timer is active (Timering)");

SUI_DeclareString(ENABLETIMER_KEY, "enableTimer");
SUI_DeclareString(ENABLETIMER_HELP, "Enable Timer and interrupt");

SUI_DeclareString(SETTIMER_KEY, "setTimer");
SUI_DeclareString(SETTIMER_HELP, "Set Timer weekday, day, hour and minute values");

SUI_DeclareString(CLEARTIMER_KEY, "clearTimer");
SUI_DeclareString(CLEARTIMER_HELP, "Clear Timer status and disable interrupt");

SUI_DeclareString(RESETTIMER_KEY, "resetTimer");
SUI_DeclareString(RESETTIMER_HELP, "Clear Timer status only");

SUI_DeclareString(SETSQUAREWAVE_KEY, "setSquareWave");
SUI_DeclareString(SETSQUAREWAVE_HELP, "Set clkout frequency");

SUI_DeclareString(CLEARSQUAREWAVE_KEY, "clearSquareWave");
SUI_DeclareString(CLEARSQUAREWAVE_HELP, "Disable clkout");

SUI_DeclareString(CLEARVOLTLOW_KEY, "clearVoltLow");
SUI_DeclareString(CLEARVOLTLOW_HELP, "clear Low Voltage bit");

SUI_DeclareString(ALARM_ENABLED, "\nAlarm is enabled");
SUI_DeclareString(ALARM_DISABLED, "\nAlarm is disabled");
SUI_DeclareString(ALARM_ACTIVE, "\nAlarm is active (alarming)");
SUI_DeclareString(ALARM_INACTIVE, "\nAlarm is inactive (not alarming)");

SUI_DeclareString(TIMER_ENABLED, "\nTimer is enabled");
SUI_DeclareString(TIMER_DISABLED, "\nTimer is disabled");
SUI_DeclareString(TIMER_ACTIVE, "\nTimer is active (alarming)");
SUI_DeclareString(TIMER_INACTIVE, "\nTimer is inactive (not alarming)");

SUI_DeclareString(SUNDAY, "Sunday ");
SUI_DeclareString(MONDAY, "Monday ");
SUI_DeclareString(TUESDAY, "Tuesday ");
SUI_DeclareString(WEDNESDAY, "Wednesday ");
SUI_DeclareString(THURSDAY, "Thursday ");
SUI_DeclareString(FRIDAY, "Friday ");
SUI_DeclareString(SATURDAY, "Saturday ");
SUI_DeclareString(ERRORDAY, "Errorday ");

SUI_DeclareString(ENTER_VALUE_FOR, "Enter value for ");
SUI_DeclareString(VOLT_LOW, "Volt Low (0-1)");
SUI_DeclareString(SECONDS, "Second (0-59)");
SUI_DeclareString(MINUTES, "Minute (0-59)");
SUI_DeclareString(HOURS, "Hour (0-23)");
SUI_DeclareString(WEEKDAY, "Weekday (0: Sunday - 6: Saturday)");
SUI_DeclareString(MONTHDAY, "Day of month (1-31)");
SUI_DeclareString(MONTH, "Month (1-12)");
SUI_DeclareString(DECADE, "Decade (0-99)");
SUI_DeclareString(CENTURY, "Century (0:2000; 1: 1900)");

SUI_DeclareString(CLKOUTFQ, "Clock output frequency "
                            "(0: 32768Hz, 1: 1024Hz, 2: 32Hz, 3: 1Hz)");

SUI_DeclareString(SETALARM, "Alarm match values (99: mask)")
SUI_DeclareString(MHDW, "Alarm Minute, Hour, Day, Weekday");
SUI_DeclareString(ENTER, "Enter a character, to commit");
SUI_DeclareString(LOWVOLTS, " VL! ");
SUI_DeclareString(ALARMING, " ALARM! ");

SUI_DeclareString(TMSRCFQ, "Timer source frequency "
                           "(0: 4096Hz, 1: 64Hz, 2: 1Hz, 3: 1/60 Hz)");
SUI_DeclareString(TMPLAC, "Timer mode (0: Active, 1: Pulsed)");
SUI_DeclareString(TMVAL, "Timer countdown value (0-255)");
SUI_DeclareString(TIMERING, " TIMER! ");


/*
 * Globals
 */

Eep_type::Block eemem EEMEM;
SUI::SerialUI sui(GREETING);
Rtc_Pcf8563 rtc;
Eep_type eep(defaults, &eemem);
EepromData eepromData;
volatile bool rtc_int = false;
volatile uint32_t clk_overflow = 0;
volatile uint32_t rtc_clkticks = 0;

/*
 * functions
 */

void rtc_int_isr(void) {
    rtc_int = true;
}

void rtc_clkout_isr(void) {
    volatile uint32_t last = rtc_clkticks;
    rtc_clkticks++;
    if (rtc_clkticks < last)  // overflow
        clk_overflow++;
}

bool is_leap_year(const uint8_t century, const uint8_t decade) {
    uint16_t year = 1900 + (century * 100) + decade;
    if ((year % 4) != 0)
        return false;
    else if ((year % 100) != 0)
        return true;
    else if ((year % 400) != 0)
        return false;
    else
        return true;
}

uint8_t days_in_month(const uint8_t month,
                      const uint8_t decade,
                      const uint8_t century) {
    enum { JAN, FEB, MAR, APR, MAY, JUN, JUL, AUG, SEP, OCTOBER, NOV, DECEMBER };
    switch (month) {
        case JAN: return 31;
        case FEB: if (is_leap_year(century, decade))
                      return 29;
                  else
                      return 28;
        case MAR: return 31;
        case APR: return 30;
        case MAY: return 31;
        case JUN: return 30;
        case JUL: return 31;
        case AUG: return 31;
        case SEP: return 30;
        case OCTOBER: return 31;
        case NOV: return 30;
        case DECEMBER: return 31;
    }
}

uint8_t what_weekday(uint8_t monthday, uint8_t month,
                     uint8_t century, uint8_t decade) {
    uint16_t year = 1900 + (century * 100) + decade;
    year -= month < 3;
    static int trans[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    return (year + year / 4 - year / 100 + year / 400 +
            trans[month - 1] + monthday) % 7;
}

int8_t sui_rtc_input(const char* sui_string, uint8_t min, uint8_t max) {
    uint8_t value;
    uint8_t tries = eepromData.suiReadIntTries;
    do {
        sui.print_P(ENTER_VALUE_FOR);
        sui.println_P(sui_string);
        sui.showEnterNumericDataPrompt();
        value = sui.parseInt();
        if (--tries == 0)
            return -1;
    } while ((value != 99) && ((value < min) || (value > max)));
    if (sui.userPresent())
        return value;
    else
        return -1;
}

/*
 * Callbacks
 */

namespace Callbacks {
    void printTmAl_cb(void) { // TODO: Disable interrupts
                              bool AF = rtc.alarmActive();
                              bool TF = rtc.timerActive();
                              if (AF) // alarm enabled and alarming
                                  sui.print_P(ALARMING);
                              if (TF)
                                  sui.print_P(TIMERING);
                              rtc.resetAlarm();
                              rtc.resetTimer();
                              rtc_int = false;
                            }
    void printdatetime_cb(void) { rtc.getDateTime();
                                  switch(rtc.getWeekday()) {
                                      case 0: sui.print_P(SUNDAY); break;
                                      case 1: sui.print_P(MONDAY); break;
                                      case 2: sui.print_P(TUESDAY); break;
                                      case 3: sui.print_P(WEDNESDAY); break;
                                      case 4: sui.print_P(THURSDAY); break;
                                      case 5: sui.print_P(FRIDAY); break;
                                      case 6: sui.print_P(SATURDAY); break;
                                      default: sui.print_P(ERRORDAY); break;
                                  }
                                  sui.print(rtc.formatDate());
                                  sui.print(" ");
                                  sui.print(rtc.formatTime());
                                  sui.print(" int: ");
                                  sui.print((byte)rtc_int);
                                  sui.print(" s2: ");
                                  sui.print(rtc.getStatus2() & 0x1F, BIN);
                                  sui.print(" ");
                                  sui.print(" tc: ");
                                  sui.print(rtc.getTimerValue(), DEC);
                                  sui.print(" ");
                                  sui.print(" tv: ");
                                  sui.print(rtc.getTimerControl() & 0x83, BIN);
                                  sui.print(" ");
                                  sui.print(" clk: ");
                                  sui.print(rtc_clkticks);
                                  sui.print(" ");
                                  if (rtc_int)  // set from ISR
                                      printTmAl_cb();
                                  if (rtc.getVoltLow())
                                      sui.println_P(LOWVOLTS);
                                  sui.println();
                                }
    void zeroClock_cb(void) { rtc.zeroClock(); sui.returnOK(); }
    void clearStatus_cb(void) { rtc.clearStatus(); sui.returnOK(); }
    void getDateTime_cb(void) { printdatetime_cb();
                                sui.returnOK(); }
    void setDateTime_cb(void) { uint8_t hour;
                                uint8_t minute;
                                uint8_t sec;
                                bool century;
                                uint8_t decade;
                                uint8_t month;
                                uint8_t day;
                                uint8_t weekday;
                                int16_t value;
                                char junk[3] = {0, 0, 0};

                                value = sui_rtc_input(CENTURY, 0, 1);
                                if ((value < 0) || (value == 99))
                                    goto FAIL;
                                century = value;

                                value = sui_rtc_input(DECADE, 0, 99);
                                if ((value < 0) || (value == 99))
                                    goto FAIL;
                                decade = value;

                                value = sui_rtc_input(MONTH, 1, 12);
                                if ((value < 0) || (value == 99))
                                    goto FAIL;
                                month = value;

                                value = sui_rtc_input(MONTHDAY, 1,
                                                      days_in_month(month,
                                                                    !century,
                                                                    decade));
                                if ((value < 0) || (value == 99))
                                    goto FAIL;
                                day = value;

                                weekday = what_weekday(day, month,
                                                       !century, decade);

                                value = sui_rtc_input(HOURS, 0, 23);
                                if ((value < 0) || (value == 99))
                                    goto FAIL;
                                hour = value;

                                value = sui_rtc_input(MINUTES, 0, 59);
                                if ((value < 0) || (value == 99))
                                    goto FAIL;
                                minute = value;

                                value = sui_rtc_input(SECONDS, 0, 59);
                                if ((value < 0) || (value == 99))
                                    goto FAIL;
                                sec = value;

                                sui.println_P(ENTER);
                                sui.showEnterDataPrompt();
                                value = 0;
                                do
                                    value = sui.readBytesToEOL(junk, 3);
                                while ((value == 0) && (sui.userPresent()));

                                if (!sui.userPresent())
                                    goto FAIL;

                                rtc.setDateTime(day, weekday, month,
                                                century, decade,
                                                hour, minute, sec);
                                sync_interrupts();
                                sui.returnOK();
                                return;

                                FAIL:
                                sui.returnError("Input Error or Timeout");
                                return; }
    void getAlarm_cb(void) { rtc.getDateTime();
                             sui.println_P(MHDW);
                             sui.print(rtc.getAlarmMinute());
                             sui.print(", ");
                             sui.print(rtc.getAlarmHour());
                             sui.print(", ");
                             sui.print(rtc.getAlarmDay());
                             sui.print(", ");
                             sui.println(rtc.getAlarmWeekday());
                             sui.returnOK(); }
    void alarmEnabled_cb(void) { rtc.getDateTime();
                                 if (rtc.alarmEnabled())
                                     sui.println_P(ALARM_ENABLED);
                                 else
                                     sui.println_P(ALARM_DISABLED);
                                 sui.returnOK(); }
    void alarmActive_cb(void) { rtc.getDateTime();
                                if (rtc.alarmActive())
                                    sui.println_P(ALARM_ACTIVE);
                                else
                                    sui.println_P(ALARM_INACTIVE);
                                sui.returnOK(); }
    void enableAlarm_cb(void) { rtc.enableAlarm(); sui.returnOK(); }
    void setAlarm_cb(void) { uint8_t minute = 0;
                             uint8_t hour = 0;
                             uint8_t day = 0;
                             uint8_t weekday = 0;
                             int16_t value;
                             sui.println_P(SETALARM);
                             value = sui_rtc_input(MINUTES, 0, 59);
                             if (value < 0)
                                 goto FAIL;
                             minute = value;

                             value = sui_rtc_input(HOURS, 0, 23);
                             if (value < 0)
                                 goto FAIL;
                             hour = value;

                             value = sui_rtc_input(MONTHDAY, 1, 31);
                             if (value < 0)
                                 goto FAIL;
                             day = value;

                             value = sui_rtc_input(WEEKDAY, 0, 6);
                             if (value < 0)
                                 goto FAIL;
                             weekday = value;

                             rtc.setAlarm(minute, hour, day, weekday);
                             sui.returnOK();
                             return;

                             FAIL:
                             sui.returnError("Input Error or Timeout");
                             return; }
    void clearAlarm_cb(void) { rtc.clearAlarm(); sui.returnOK(); }
    void resetAlarm_cb(void) { rtc.resetAlarm(); sui.returnOK(); }
    void timerEnabled_cb(void) { rtc.getDateTime();
                                 if (rtc.timerEnabled())
                                     sui.println_P(TIMER_ENABLED);
                                 else
                                     sui.println_P(TIMER_DISABLED);
                                 sui.returnOK(); }
    void timerActive_cb(void) { rtc.getDateTime();
                                if (rtc.timerActive())
                                    sui.println_P(TIMER_ACTIVE);
                                else
                                    sui.println_P(TIMER_INACTIVE);
                                sui.returnOK(); }
    void enableTimer_cb(void) { rtc.enableTimer(); sui.returnOK(); }
    void setTimer_cb(void) { uint8_t tmvalue = 0;
                             uint8_t tmfreq = 0;
                             bool is_pulsed = false;
                             int16_t value = 0;

                             value = sui_rtc_input(TMPLAC, 0, 1);
                             if ((value < 0) || (value == 99))
                                 goto FAIL;
                             is_pulsed = value;

                             value = sui_rtc_input(TMSRCFQ, 0, 3);
                             if ((value < 0) || (value == 99))
                                 goto FAIL;
                             tmfreq = value;

                             value = sui_rtc_input(TMVAL, 0, 255);
                             if ((value < 0) || (value == 99))
                                 goto FAIL;
                             tmvalue = value;

                             rtc.setTimer(tmvalue, tmfreq, is_pulsed);
                             sui.returnOK();
                             return;

                             FAIL:
                             sui.returnError("Input Error or Timeout");
                             return; }
    void clearTimer_cb(void) { rtc.clearTimer(); sui.returnOK(); }
    void resetTimer_cb(void) { rtc.resetTimer(); sui.returnOK(); }
    void setSquareWave_cb(void) { int8_t clkout = 0;

                                  clkout = sui_rtc_input(CLKOUTFQ, 0, 3);
                                  if ((clkout < 0) || (clkout == 99))
                                      goto FAIL;

                                  rtc.setSquareWave(clkout);
                                  sui.returnOK();
                                  return;

                                  FAIL:
                                  sui.returnError("Input Error or Timeout");
                                  return; };
    void clearSquareWave_cb(void) { rtc.clearSquareWave(); sui.returnOK(); }
    void clearVoltLow_cb(void) { rtc.clearVoltLow(); sui.returnOK(); }
}

void sync_interrupts(void) {
    attachInterrupt(eepromData.rtcClkoutVect,
                    rtc_clkout_isr,
                    eepromData.rtcClkoutMode);
    attachInterrupt(eepromData.rtcIntVect,
                    rtc_int_isr,
                    eepromData.rtcIntMode);
    rtc.getDateTime();
    uint8_t sec = rtc.getSecond();
    do
        rtc.getDateTime();
    while (sec == rtc.getSecond());
    clk_overflow = 0;
    rtc_clkticks = sec * eepromData.rtcClkoutFreq;
    rtc_int = false;
}

void setup(void) {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    Eep_type eep(defaults, &eemem);
    EepromData* value = eep.data();

    if (!value)
        while (true) {
            digitalWrite(LED_BUILTIN, ~digitalRead(LED_BUILTIN));
            delay(100);
        }
    memcpy(&eepromData, value, sizeof(EepromData));
    digitalWrite(LED_BUILTIN, LOW);

    pinMode(eepromData.rtcClkoutPin, eepromData.rtcClkoutPinMode);
    pinMode(eepromData.rtcIntPin, eepromData.rtcIntPinMode);

    sync_interrupts();

    sui.begin(eepromData.serialBaudRate);

    sui.println_P(SETUP);

    sui.setTimeout(eepromData.suiTimeout);
    sui.setMaxIdleMs(eepromData.suiMaxIdle);
    sui.setReadTerminator(eepromData.suiInputTerm);
    sui.setEchoCommands(eepromData.echoCommands);

    SUI::Menu* mainMenu = sui.topLevelMenu();
    mainMenu->setName(MAIN_MENU);

    // mainMenu->addCommand(key, func, help);
    mainMenu->addCommand(ZEROCLOCK_KEY,
                         Callbacks::zeroClock_cb,
                         ZEROCLOCK_HELP);

    mainMenu->addCommand(CLEARSTATUS_KEY,
                         Callbacks::clearStatus_cb,
                         CLEARSTATUS_HELP);

    mainMenu->addCommand(GETDATETIME_KEY,
                         Callbacks::getDateTime_cb,
                         GETDATETIME_HELP);

    mainMenu->addCommand(SETDATETIME_KEY,
                         Callbacks::setDateTime_cb,
                         SETDATETIME_HELP);

    mainMenu->addCommand(GETALARM_KEY,
                         Callbacks::getAlarm_cb,
                         GETALARM_HELP);

    mainMenu->addCommand(ALARMENABLED_KEY,
                         Callbacks::alarmEnabled_cb,
                         ALARMENABLED_HELP);

    mainMenu->addCommand(ALARMACTIVE_KEY,
                         Callbacks::alarmActive_cb,
                         ALARMACTIVE_HELP);

    mainMenu->addCommand(ENABLEALARM_KEY,
                         Callbacks::enableAlarm_cb,
                         ENABLEALARM_HELP);

    mainMenu->addCommand(SETALARM_KEY,
                         Callbacks::setAlarm_cb,
                         SETALARM_HELP);

    mainMenu->addCommand(CLEARALARM_KEY,
                         Callbacks::clearAlarm_cb,
                         CLEARALARM_HELP);

    mainMenu->addCommand(RESETALARM_KEY,
                         Callbacks::resetAlarm_cb,
                         RESETALARM_HELP);

    mainMenu->addCommand(TIMERENABLED_KEY,
                         Callbacks::timerEnabled_cb,
                         TIMERENABLED_HELP);

    mainMenu->addCommand(TIMERACTIVE_KEY,
                         Callbacks::timerActive_cb,
                         TIMERACTIVE_HELP);

    mainMenu->addCommand(ENABLETIMER_KEY,
                         Callbacks::enableTimer_cb,
                         ENABLETIMER_HELP);

    mainMenu->addCommand(SETTIMER_KEY,
                         Callbacks::setTimer_cb,
                         SETTIMER_HELP);

    mainMenu->addCommand(CLEARTIMER_KEY,
                         Callbacks::clearTimer_cb,
                         CLEARTIMER_HELP);

    mainMenu->addCommand(RESETTIMER_KEY,
                         Callbacks::resetTimer_cb,
                         RESETTIMER_HELP);

    mainMenu->addCommand(SETSQUAREWAVE_KEY,
                         Callbacks::setSquareWave_cb,
                         SETSQUAREWAVE_HELP);

    mainMenu->addCommand(CLEARSQUAREWAVE_KEY,
                         Callbacks::clearSquareWave_cb,
                         CLEARSQUAREWAVE_HELP);

    mainMenu->addCommand(CLEARVOLTLOW_KEY,
                         Callbacks::clearVoltLow_cb,
                         CLEARVOLTLOW_HELP);

    sui.println_P(LOOP);
}

void loop(void) {
    if (sui.checkForUser(eepromData.suiUserTimeout)) {
        sui.enter();
        while (sui.userPresent())
            sui.handleRequests();
    }
    Callbacks::printdatetime_cb();
}
