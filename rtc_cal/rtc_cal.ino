#include <Wire.h>
#include <Rtc_Pcf8563.h>
#include "tm.h"
#include "eeprom.h"
#include "clock.h"
#include "exception.h"
#include "command_mode.h"

/*
 * Globals
 */

Rtc_Pcf8563 rtc;
clock::Clock rtc_clock;
clock::TM currentTime;

/*
 * Functions
 */

bool initRtc(void) {
    // Toggle alarm minute to confirm presense
    bool wasEnabled = rtc.alarmEnabled();
    clock::time_type alarmMinute = rtc.getAlarmMinute();
    clock::time_type alarmHour = rtc.getAlarmHour();
    clock::date_type alarmDay = rtc.getAlarmDay();
    clock::week_type alarmWeekday = rtc.getAlarmWeekday();
    rtc.setAlarm(constrain(alarmMinute + 1, 0, 59),
                 constrain(alarmHour + 1, 0, 23),
                 constrain(alarmDay + 1, 1, 31),
                 constrain(alarmWeekday + 1, 0, 6));
    clock::time_type endAlarmMinute = rtc.getAlarmMinute();
    clock::time_type endAlarmHour = rtc.getAlarmHour();
    clock::date_type endAlarmDay = rtc.getAlarmDay();
    clock::week_type endAlarmWeekday = rtc.getAlarmWeekday();
    // don't leave broken values when raising exception
    rtc.setAlarm(alarmMinute,
                 alarmHour,
                 alarmDay,
                 alarmWeekday);
    // setting implies enabling
    if (!wasEnabled)
        rtc.clearAlarm();
    // raise exception if values did not change (no rtc present)
    if (endAlarmMinute == constrain(alarmMinute + 1, 0, 59) &&
        endAlarmHour == constrain(alarmHour + 1, 0, 23) &&
        endAlarmDay == constrain(alarmDay + 1, 1, 31) &&
        endAlarmWeekday == constrain(alarmWeekday + 1, 0, 6))

        return true; // passed
    else
        return false;  // failed
}

bool initClkOut(void) {
    Eep_type eep(defaults, &eemem);
    EepromData* eepromData = eep.data();
    if (eepromData->clkoutCfg == SQW_DISABLE)
        return false;
    rtc.setSquareWave(eepromData->clkoutCfg);
    return true;
}


bool clockInit(void) {
    Eep_type eep(defaults, &eemem);
    EepromData* eepromData = eep.data();
    if (!rtc_clock.init(eepromData->setTime))
        return false;
    currentTime.tm_ticks = 0;
    // ssssllloowwwww
    currentTime.tm_hertz = eepromData->clkoutHz +
                        (eepromData->hzOffset / 1000.0);
    rtc.getDateTime();
    currentTime.tm_sec = rtc.getSecond() + 1;
    // synchronize @ next seconds == 0 ticks
    while (currentTime.tm_sec >= rtc.getSecond())
        rtc.getDateTime();
    // this must happen really fast
    currentTime.tm_min = rtc.getMinute();
    currentTime.tm_hour = rtc.getHour();
    currentTime.tm_mday = rtc.getDay();
    currentTime.tm_mon = rtc.getMonth();
    currentTime.tm_year = rtc.getYear();
    currentTime.tm_wday = rtc.getWeekday();
    currentTime.tm_yday = rtc.daysInYear(0,  // year 2000+
                                         rtc.getYear(),
                                         currentTime.tm_mon,
                                         currentTime.tm_mday);
    //Serial.print(F("Starting clock @ "));
    //currentTime.toSerial();
    return rtc_clock.run(currentTime,
                         eepromData->clkoutIntNo,
                         eepromData->clkoutIntMode);
}


void initRandom(uint16_t n1, uint16_t n2, uint16_t n3,
                uint16_t n4, uint16_t n5, uint16_t n6) {
    uint16_t seed = (n1 << 5) & (n2 << 4) | (n3 << 3);
    seed  ^= (n4 << 2) & (n5 << 1) + n6;
    seed += n1 + n3 + n5;
    seed -= n2 - n4 - n6;
    randomSeed(seed);
}

/*
 * Main Program
 */

void setup(void) {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    Serial.begin(115200);
    delay(1);
    Serial.println(F("\n\nSetup()"));
    Serial.flush();

    Exception exception(2500, 250);
    Eep_type eep(defaults, &eemem);
    EepromData* eepromData = eep.data();
    if (!eepromData) {
        exception.raise(E_EEPROM_INIT);
    }

    delay(eepromData->cmdModeWindow);  // Time to put first code in buffer
    if (commandModeEnter())
        commandMode();

    pinMode(eepromData->clkoutPin, eepromData->clkoutMode);
    pinMode(eepromData->intPin, eepromData->intMode);

    if (!initRtc())
        exception.raise(E_RTC_INIT);

    rtc.getDateTime();
    if (rtc.getVoltLow())
        exception.raise(E_RTC_UNSET);

    initRandom(rtc.getSecond(), rtc.getYear(), rtc.getMinute(),
               rtc.getMonth(), rtc.getHour(), rtc.getDay());

    if (!initClkOut())
        exception.raise(E_CLKOUT_INIT);

    if (!clockInit())
        exception.raise(E_CLOCK_INIT);

    currentTime = eepromData->setTime;
    bool noError = rtc_clock.now(currentTime);
    if (!noError)
        exception.raise(E_CLOCK_STALL);

    digitalWrite(LED_BUILTIN, LOW);
    Serial.println(F("loop()"));
}

void loop(void) {
    rtc_clock.now(currentTime);
    currentTime.toSerial();
    delay(30000);
}
