#ifndef TM_H
#define TM_H

namespace clock {

typedef int8_t week_type;  // day of week
typedef int8_t date_type;  // month, month-day
typedef int8_t time_type;  // hour, minute, second
typedef int16_t year_type; // year, year-day
typedef uint32_t epoch_type;  // Large number of seconds or ticks
typedef double freq_type;  // hertz

class TMPod {
    public:
    year_type tm_year = -1;     /* Year - 2000 */
    year_type tm_yday = -1;     /* Day in the year (0-365, 1 Jan = 0) */
    time_type tm_hour = -1;     /* Hours (0-23) */
    time_type tm_min = -1;      /* Minutes (0-59) */
    time_type tm_sec = -1;      /* Seconds (0-60) */
    date_type tm_mday = -1;     /* Day of the month (1-31) */
    date_type tm_mon = -1;      /* Month (0-11) */
    week_type tm_wday = -1;     /* Day of the week (0-6, Sunday = 0) */
    epoch_type tm_ticks = -1;   /* Ticks after tm_sec */
    freq_type tm_hertz = -1.0;  /* Frequency of tm_ticks */
};

class TM : public TMPod {
    public:
    // Constructors
    TM(void);
    TM(const TMPod& source);
    TM(const TMPod* source);

    // Operators
    TM& operator=(const TM& rhs);

    // return number of days in tm_year + 2000 year & tm_mon
    year_type daysInMonth(void) const;

    // return weekday from tm_mday, tm_mon and tm_year + 2000
    week_type whatWeekday(void) const;

    // return number of days in tm_year + 2000, tm_mon and tm_mday
    year_type daysInYear(void) const;

    // Integrate additional seconds and ticks
    // return false if tm_hertz or tm_ticks are not set
    bool intSecTick(const epoch_type& seconds, const epoch_type& ticks);

    // Fill out TM from seconds (since 1-1-2000) + ticks @ hertz
    bool fromSecTick(const epoch_type& seconds,
                     const epoch_type& ticks,
                     const freq_type& hertz);

    // Convert TM into seconds/ticks since 1-1-2000 or sinceSeconds/Ticks
    void toSecTick(epoch_type& seconds, epoch_type& ticks) const;
    // Return tm_ticks @ tm_hertz as microseconds
    void toMicroSec(epoch_type& microSec) const;

    // Print out date, time and ticks
    void toSerial(void) const;

    private:
    static_assert(sizeof(freq_type) >= sizeof(epoch_type),
                  "TM member size error");
    static_assert(sizeof(epoch_type) >= sizeof(year_type),
                  "TM member size error");
    static_assert(sizeof(year_type) >= sizeof(time_type),
                  "TM member size error");
    static_assert(sizeof(time_type) >= sizeof(date_type),
                  "TM member size error");
    static_assert(sizeof(date_type) >= sizeof(week_type),
                  "TM member size error");

    const year_type mdays[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    const date_type daysm[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const week_type dowt[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    const time_type min_seconds = 60U;
    const time_type min_hours = 60U;
    const date_type day_hours = 24U;
    const epoch_type hour_seconds = 3600UL;  // 60 * 60
    const epoch_type day_seconds = 86400UL;  // 60 * 60 * 24
    const epoch_type year_seconds = 31536000UL;  // 60 * 60 * 24 * 365

    // return no. leap-days between start and end (exclusive) canonical years
    year_type leapYearsBetween(year_type start,
                               year_type end) const;

    // return true if canonical year is a leap-year
    bool isLeapYear(const year_type year) const;

    // return true if tm_year + 2000 is a leap year
    bool isLeapYear(void) const;

    // return number of days in canonical year & month
    year_type daysInMonth(const year_type year, const date_type month) const;

    // return number of days in canonical year, month and day
    year_type daysInYear(const year_type year,
                         const date_type month,
                         const date_type day) const;

    // return weekday from day, month and canonical year
    week_type whatWeekday(date_type day,
                          date_type month,
                          year_type year) const;
};

/*
 * PUBLIC
 */

inline TM::TM(void) {}
inline TM::TM(const TMPod& source) : TMPod(source) {}

// Operators
inline TM& TM::operator= ( const TM& rhs) {
    tm_year = rhs.tm_year;
    tm_yday = rhs.tm_yday;
    tm_hour = rhs.tm_hour;
    tm_min = rhs.tm_min;
    tm_sec = rhs.tm_sec;
    tm_mday = rhs.tm_mday;
    tm_mon = rhs.tm_mon;
    tm_wday = rhs.tm_wday;
    tm_ticks = rhs.tm_ticks;
    tm_hertz = rhs.tm_hertz;
    return *this;
}

bool TM::intSecTick(const epoch_type& seconds, const epoch_type& ticks) {
    if ((tm_hour < 0) || (tm_sec < 0))  // uninitialized
        return false; // error

    if ((tm_hertz < 1) || (tm_ticks < 0))  // don't know ticks or hertz
        return false; // error

    if ((seconds == 0) && (ticks == 0))
        return true; // nothing to do

    // need larger type than tm_sec, prevent overflow
    epoch_type new_seconds = seconds + tm_sec;
    // Only convert once
    freq_type newTicks = ticks + tm_ticks;
    // add in whole seconds inside ticks
    new_seconds = new_seconds + (newTicks / tm_hertz);
    // leftover ticks
    tm_ticks = fmod(newTicks, tm_hertz);
    //Serial.print(F("\ttm_ticks ")); Serial.println(tm_ticks);

    ldiv_t result = ldiv(new_seconds, TM::min_seconds);
    tm_sec = result.rem;  // old tm_sec added in above
    //Serial.print(F("\ttm_sec ")); Serial.println(tm_sec);
    if (tm_sec > 59)
        return false;  // error
    epoch_type new_minutes = tm_min + result.quot;

    result = ldiv(new_minutes, TM::min_hours);
    tm_min = result.rem;
    //Serial.print(F("\ttm_min ")); Serial.println(tm_min);
    epoch_type new_hours = tm_hour + result.quot;

    result = ldiv(new_hours, TM::day_hours);
    tm_hour = result.rem;
    //Serial.print(F("\ttm_hour ")); Serial.println(tm_hour);
    // Handle year-day + leap-day before calculate tm_mon, tm_mday, & tm_wday
    epoch_type new_days = tm_yday + result.quot;

    if (isLeapYear())
        result = ldiv(new_days, 366);
    else
        result = ldiv(new_days, 365);
    tm_yday = result.rem;
    tm_year += result.quot;

    week_type leap_day = isLeapYear();  // tm_year could have changed
    if (leap_day && (tm_yday == 60)) {  // feb 29th
        tm_mday = 29;
        tm_mon = 2;
    } else {
        if (tm_yday > 60)
            tm_yday -= leap_day;  // march 1st
        // const int mdays[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
        for (tm_mon = 12; tm_mon > 1; tm_mon--)
            if ((tm_yday) > TM::mdays[tm_mon - 1]) {  // found it!
                tm_mday = tm_yday - TM::mdays[tm_mon - 1];
                break;
            }
    }

    tm_wday = whatWeekday();
    /*
    Serial.print(F("\ttm_wday ")); Serial.println(tm_wday);
    Serial.print(F("\ttm_mday ")); Serial.println(tm_mday);
    Serial.print(F("\ttm_mon ")); Serial.println(tm_mon);
    Serial.print(F("\ttm_yday ")); Serial.println(tm_yday);
    Serial.print(F("\ttm_year ")); Serial.println(tm_year);
    */
    return true;
}

// Fill out TM from seconds (since 1-1-2000) + ticks @ hertz
inline bool TM::fromSecTick(const epoch_type& seconds,
                            const epoch_type& ticks,
                            const freq_type& hertz) {
    tm_hertz = hertz;
    tm_sec = 0;
    tm_ticks = 0;
    tm_min = 0;
    tm_hour = 0;
    tm_mday = 1;
    tm_mon = 1;
    tm_year = 0;  // 2000
    tm_wday = 0;  // intSecTick() will set
    tm_yday = 1;
    return intSecTick(seconds, ticks);
}

// Convert TM into seconds since 1-1-2000, + ticks @ tm_hertz
// set seconds/ticks to -1 on error
inline void TM::toSecTick(epoch_type& seconds, epoch_type& ticks) const {
    if ((tm_year < 0) || (tm_yday < 0) || (tm_hour < 0) ||
        (tm_min < 0) || (tm_sec < 0)) {
        seconds = ticks = -1; // error
    } else {
        seconds = leapYearsBetween(2000, 2000 + tm_year) * day_seconds;
        seconds += isLeapYear() * day_seconds;
        seconds += tm_yday * day_seconds;
        seconds += tm_hour * hour_seconds;
        // assume big enough to not wrap
        seconds += tm_min * min_seconds;
        seconds += tm_sec;
        ticks = tm_ticks;  // truncate
    }
}

// Return tm_ticks @ tm_hertz as microseconds
inline void TM::toMicroSec(epoch_type& microSec) const {
    microSec = (tm_ticks * 1000000.0) / tm_hertz;
}

void TM::toSerial(void) const {
    if ((tm_year < 1) || (tm_year > 1000))
        Serial.print(F("????"));
    else
        Serial.print(2000 + tm_year, DEC);
    Serial.print(F("."));
    if ((tm_mon < 1) || (tm_mon > 12))
        Serial.print(F("?"));
    else
        Serial.print(tm_mon, DEC);
    Serial.print(F("."));
    if ((tm_mday < 1) || (tm_mday > daysInMonth()))
        Serial.print(F("?"));
    else
        Serial.print(tm_mday, DEC);
    Serial.print(F(" "));
    if ((tm_hour < 0) || (tm_hour > 23))
        Serial.print(F("?"));
    else
        Serial.print(tm_hour, DEC);
    Serial.print(F(":"));
    if ((tm_min < 0) || (tm_min > 59))
        Serial.print(F("?"));
    else
        Serial.print(tm_min, DEC);
    Serial.print(F(":"));
    if ((tm_sec < 0) || tm_sec > 60)  // leap scond?
        Serial.print(F("?"));
    else
        Serial.print(tm_sec, DEC);
    switch (tm_wday) {
        case 0: Serial.print(F(" Sun day #")); break;
        case 1: Serial.print(F(" Mon day #")); break;
        case 2: Serial.print(F(" Tue day #")); break;
        case 3: Serial.print(F(" Wed day #")); break;
        case 4: Serial.print(F(" Thu day #")); break;
        case 5: Serial.print(F(" Fri day #")); break;
        case 6: Serial.print(F(" Sat day #")); break;
        default: Serial.print(F(" ??? day #")); break;
    }
    if ((tm_yday < 0) || tm_yday > daysInYear())
        Serial.print(F("?"));
    else
        Serial.print(tm_yday, DEC);
    Serial.print(F(", + "));
    if (tm_ticks >= 4000000000UL)
        Serial.print(F("?"));
    else
        Serial.print(tm_ticks, DEC);
    Serial.print(F(" @ "));
    if (tm_hertz < 0)
        Serial.print(F("?"));
    else
        Serial.print(tm_hertz);
    Serial.print(F("Hz"));
    Serial.println();
}

/*
 * PRIVATE
 */

inline year_type TM::leapYearsBetween(year_type start,
                                      year_type end) const {
    end--;
    return ((end / 4) - (end / 100) + (end / 400)) -
           ((start / 4) - (start / 100) + (start / 400));
}

inline bool TM::isLeapYear(const year_type year) const {
    if ((year % 4) != 0)
        return false;
    else if ((year % 100) != 0)
        return true;
    else if ((year % 400) != 0)
        return false;
    else
        return true;
}

inline bool TM::isLeapYear(void) const {
    return isLeapYear(2000 + tm_year);
}

inline year_type TM::daysInMonth(const year_type year,
                                 const date_type month) const {
    //const date_type daysm[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    date_type dim = TM::daysm[month - 1];
    if (month == 2 && isLeapYear(year))
        dim += 1;
    return dim;
}

inline year_type TM::daysInMonth(void) const {
    return daysInMonth(2000 + tm_year, tm_mon);
}

inline year_type TM::daysInYear(const year_type year,
                                const date_type month,
                                const date_type day) const {
    // const int mdays[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    year_type total = TM::mdays[month - 1] + day;
    if ((month > 2) and isLeapYear(year))
        total += 1;
    return total;
}

inline year_type TM::daysInYear(void) const {
    return daysInYear(2000 + tm_year, tm_mon, tm_mday);
}

inline week_type TM::whatWeekday(date_type day,
                                 date_type month,
                                 year_type year) const {
    // Credit: Tomohiko Sakamoto
    // http://en.wikipedia.org/wiki/Determination_of_the_day_of_the_week
    year -= month < 3;
    return (year + year / 4 - year / 100 + year / 400 +
            TM::dowt[month - 1] + day) % 7;
}

inline week_type TM::whatWeekday(void) const {
    return whatWeekday(tm_mday, tm_mon, 2000 + tm_year);
}

}  // namespace clock

#endif // TM_H
