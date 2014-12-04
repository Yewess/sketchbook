#include <avr/eeprom.h>
#include <string.h>
#include <Wire.h>
#include <Rtc_Pcf8563.h>  // https://github.com/elpaso/Rtc_Pcf8563.git
#include "rtc_cal.h"

/*
 * Globals
 */

// Located in .eeprom section, do not reference!
EepromData eepromData EEMEM;

Clock* clock{NULL};  // created in setup()

/*
 * Implementations
 */

bool Eeprom::load(void) {
    EepromData temp;
    memset(&temp, 0xFF, sizeof(EepromData));
    eeprom_busy_wait();
    // reference safe, function expects .eeprom relative address
    eeprom_read_block(&temp, &eepromData, sizeof(EepromData));
    Serial.println(F("\tLoaded EEPROM"));
    if (magicValid(temp.magic, temp.version))
        memcpy(this, &temp, sizeof(eepromData));
    else
        return false;
    return true;
}

bool Eeprom::save(void) {
    Serial.println(F("\tSaving EEPROM"));
    eeprom_busy_wait();
    // reference safe, function expects .eeprom relative address
    eeprom_update_block(this, &eepromData, sizeof(EepromData));
    return load();
}

void Eeprom::erase(void) {
    EepromData temp;
    memset(&temp, 0xFF, sizeof(EepromData));
    eeprom_busy_wait();
    eeprom_update_block(&temp, &eepromData, sizeof(EepromData));
}

void Eeprom::dump(void) {
    Serial.print(F("Eeprom dump ("));
    Serial.print(sizeof(eepromData));
    Serial.print(F(" bytes):"));
    for (uint16_t i=0; i < sizeof(eepromData); i++) {
        if (i % 8 == 0)
            Serial.print("\n\t");
        uint16_t addr = reinterpret_cast<uint16_t>(&eepromData) + i;
        eeprom_busy_wait();
        uint16_t value = eeprom_read_byte(reinterpret_cast<uint8_t*>(addr));
        Serial.print(value, HEX);
        Serial.print(",");
    }
    Serial.println();
}

// Presumes all fields already, years since 1900
uint64_t& TM::toHzPd(uint64_t& dest, uint16_t hertz) const {
    // convert to seconds, then hertz periods last
    dest = tm_sec;
    // minutes
    dest += tm_min * 60;
    // hours
    dest += tm_hour * 60 * 60;
    // days
    dest += tm_yday * 24 * 60 * 60;
    // years (excluding this year - already incorporated into tm_yday)
    for (uint16_t year = 0; year < tm_year; year++)
        // true == 1 && false == 0
        dest += (365 + TM::isLeapYear(year)) * 24 * 60 * 60;
    dest *= hertz;
    return dest += tm_hzpd;
}

void TM::setWeekDay(void) {
    // calculate day of the week
    // http://en.wikipedia.org/wiki/
    //     Determination_of_the_day_of_the_week
    //     #Implementation-dependent_methods_of_Sakamoto.2C_Lachman.2C_Keith_and_Craver
    static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int y=tm_year, m=tm_mon, d=tm_mday;
    y -= m < 3;
    tm_wday = (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
}

void TM::fromHzPd(const uint64_t& src, uint16_t hertz) {
    uint32_t seconds = src / hertz;  // truncated
    tm_hzpd = src % hertz;  // remainder

    // years
    const uint32_t day_seconds = 60U * 60 * 24;
    for (tm_year = 0;  // 1900
         seconds >= day_seconds * 365 + isLeapYear(tm_year + 1900);
         tm_year++ ) {  // POST convert it into a year
        // subtract year (in seconds) THEN increment tm_year
        seconds -= day_seconds * 365 + isLeapYear(tm_year + 1900);
    }

    // months
    for (tm_mon = JAN;  // 0
         seconds >= day_seconds * daysInMonth(tm_mon, tm_year);
         tm_mon++ ) {  // POST convert into a month
        // subtract month (in seconds) THEN increment tm_mon
        seconds -= day_seconds * daysInMonth(tm_mon, tm_year);
    }

    // days
    tm_mday = seconds / day_seconds;
    seconds %= day_seconds;

    // hours
    tm_hour = seconds / (60 * 60);
    seconds %= (60 * 60);

    // minutes
    tm_min = seconds / 60;
    seconds %= 60;

    // seconds is what was left over
    setYearDay();

    // needs year, month & day
    setWeekDay();
}

void Eeprom::setLastAdjust(const TM& to) {
    lastAdjust.tm_sec = to.tm_sec;
    lastAdjust.tm_min = to.tm_min;
    lastAdjust.tm_hour = to.tm_hour;
    lastAdjust.tm_mday = to.tm_mday;
    lastAdjust.tm_mon = to.tm_mon;
    lastAdjust.tm_year = to.tm_year;
    lastAdjust.tm_wday = to.tm_wday;
    lastAdjust.tm_yday = to.tm_yday;
    lastAdjust.tm_hzpd = to.tm_hzpd;
}

Clock::Clock(Eeprom& eeprom) : eeprom(eeprom) {
    rtc.getDateTime();

    if (rtc.getVoltLow()) {
        eeprom.setPpmError(0);
        // **must** also be set, when clock is set
        eeprom.clearLastAdjust();
        eeprom.save();
        rtc.initClock();  // zaps everything
        Clock::altmTick = -1;
    } else
        Clock::altmTick = 0;
    lastErrorCheck = 0;
    clkOutHz = eeprom.getClkoutHz();
    setupPin(eeprom.getClkoutPin(), eeprom.getClkoutMode());
    setupPin(eeprom.getIntPin(), eeprom.getIntMode());

    // synchronize ticks = 0 to an rtc seconds changeover
    Serial.println(F("Synchronizing seconds..."));
    uint8_t current_second;
    do {
        rtc.getDateTime();
        current_second = rtc.getSecond();
    } while (rtc.getSecond() == current_second);
    // one tick already elapsed and was missed
    Clock::ticks = -1;  // attach interrupts quickly after this!

    // Int pin counting depends on clkout pin counting, avoice race
    noInterrupts();
    attachInterrupt(eeprom.getClkoutPin(), Clock::tickISR, FALLING);
    attachInterrupt(eeprom.getIntPin(), Clock::intISR, LOW);
    interrupts();

    update();  // update remaining current values
    memcpy(&start, &current, sizeof(start)); // record startup time
    Serial.print(F("Currently: "));
    Serial.print(rtc.formatDate());
    Serial.print(" ");
    Serial.print(rtc.formatTime());
}

void Clock::tickISR(void) {
    Clock::ticks++;
    if (Clock::ticks % clkOutHz == 0)
        Clock::ticks = 0;
}

void Clock::intISR(void) {
    // clkout enabled and alarm/timer enabled
    if ((Clock::ticks != -1) && (Clock::altmTick != -1))
        Clock::altmTick = Clock::ticks;
}

// must call at least once every second!
void Clock::update(void) {
    // Timing critical stuff
    rtc.getDateTime();
    bool alarmActive = rtc.alarmActive();
    bool timerActive = rtc.timerActive();
    // Int pin counting depends on clkout pin counting, avoice race
    noInterrupts();
    current.tm_hzpd = Clock::ticks;  // unadjusted

    // an alarm -or- a timer went off
    if ((Clock::ticks != -1) && (Clock::altmTick != -1)) {
        if (alarmActive) {
            memset(&alarmTM, 0, sizeof(alarmTM));
            // temporarily relative (corrected below)
            alarmTM.tm_hzpd = Clock::ticks - Clock::altmTick;
        }
        if (timerActive) {
            memset(&timerTM, 0, sizeof(timerTM));
            // temporarily relative (corrected below)
            timerTM.tm_hzpd = Clock::ticks - Clock::altmTick;;
        }
        Clock::altmTick = -1; // disable alarm until end of function
    }
    interrupts();

    // Un-adjusted values
    current.tm_sec = rtc.getSecond();
    current.tm_min = rtc.getMinute();
    current.tm_hour = rtc.getHour();
    current.tm_mday = rtc.getDay();
    current.tm_mon = rtc.getMonth();
    if (rtc.getCentury())
        current.tm_year = rtc.getYear();
    else
        current.tm_year = rtc.getYear() + 100;
    current.tm_wday = rtc.getWeekday();
    current.tm_yday = -1;
    current.setYearDay();
    current.tm_hzpd = current.tm_hzpd;  // already set above

    // Don't correct if no relative adjustment data available
    if ((eeprom.getPpmError() != 0) && (eeprom.getLastAdjust().tm_mon > 0))
        ppmAdjust();

    if (alarmActive || timerActive) {
        // Clear alarm/timer flags & record ppm adjusted alarm/timer times
        uint64_t altm_hzpd = current.toHzPd(altm_hzpd, eeprom.getClkoutHz());
        if (alarmActive) {
            // copy current time > alarmTM
            uint64_t alarm_hzpd = altm_hzpd + alarmTM.tm_hzpd; // apply offset
            alarmTM.fromHzPd(alarm_hzpd, eeprom.getClkoutHz());
            rtc.resetAlarm(); // ready for next event
            Serial.print(F("Alarm at: "));
            Serial.print(rtc.formatDate());
            Serial.println(rtc.formatTime());
        }
        if (timerActive) {
            // copy current time > timerTM
            uint64_t timer_hzpd = altm_hzpd + timerTM.tm_hzpd;  // apply offset
            timerTM.fromHzPd(timer_hzpd, eeprom.getClkoutHz());
            rtc.resetTimer();
            Serial.print(F("Timer at: "));
            Serial.print(rtc.formatDate());
            Serial.println(rtc.formatTime());
        }
    }
}

void Clock::setPpmError(uint32_t& reference_elapsed_ms,
                        uint32_t& measured_elapsed_pd) {
    // Don't truncate ms -> sec conversion
    uint32_t ref_el_pd = (reference_elapsed_ms / 1000.0) * eeprom.getClkoutHz();
    // diff is error magnitude
    int32_t rel_diff_pd = ref_el_pd - measured_elapsed_pd;
    // calc ratio & convert to ppm & store
    eeprom.setPpmError((rel_diff_pd * 1000000L) / measured_elapsed_pd);
}

// FIXME: Add serialSetPpmError()
// FIXME: Add serialSetDateTime()

/*
 * Main Program
 */

void setup(void) {
    Serial.begin(115200);
    Serial.println(F("Setup()"));

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    Wire.begin();

    // call constructor _after_ Serial is setup
    static Eeprom eeprom;
    static Clock _clock(eeprom);
    clock = &_clock;

    Serial.println(F("loop()"));
}

void loop(void) {
}
