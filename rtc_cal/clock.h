#ifndef CLOCK_H
#define CLOCK_H

#include "tm.h"

namespace clock {

class Clock {
    public:
    // Constructor
    Clock(void);

    // Initialize clock, relative to set_time, return false on error
    bool init(const TM& set_time);

    // Synchronize starting time, relative to set_time at starting
    // (possibly starting.tm_hertz != init() frequency)
    // return false on error
    bool run(const TM& starting, uint8_t irq_num, uint8_t irq_mode);

    // Retrieve and store current time into tm, should be called
    // at least once every minute to prevent eventual internal overflow.
    bool now(TM& current_time);

    // Calculate actual sample frequency based on sample of ticks over
    // a known time period at a reference frequency.  Store result in
    // actual_hertz
    static void calcHertz(const epoch_type& expected_ticks,
                          const freq_type& expected_hertz,
                          const epoch_type& actual_ticks,
                          freq_type& actual_hertz );
    private:
    TM setTime;  // Time when clock was synchronized @ ideal hertz
    // Additional seconds & ticks relative to setTime @ runHertz
    epoch_type runSeconds;
    freq_type runHertz;
    static volatile epoch_type runTicks;

    // ISR to count ticks since run()
    static void tickISR(void);
};

volatile epoch_type Clock::runTicks = 0;

inline Clock::Clock(void) : runSeconds(-1), runHertz(-1) {}

void Clock::tickISR(void) {
        Clock::runTicks++;
}

inline bool Clock::init(const TM& set_time) {
    if ((set_time.tm_hour < 0) || (set_time.tm_min < 1) ||
        (set_time.tm_sec < 0) || (set_time.tm_hertz < 0))

        return false;

    setTime = set_time;
    return true;
}

inline bool Clock::run(const TM& starting, uint8_t irq_num, uint8_t irq_mode) {
    // clock not initialized
    if ((setTime.tm_sec < 0) || (setTime.tm_ticks < 0))
        return false;

    // offset hertz not set
    if (starting.tm_hertz < 0)
        return false;

    //noInterrupts();  // timing critical
    runHertz = starting.tm_hertz;
    // error only applies to time between setTime and startTime
    epoch_type startSeconds = 0;
    epoch_type startTicks = 0;
    epoch_type setSeconds = 0;
    epoch_type setTicks = 0;
    starting.toSecTick(startSeconds, startTicks);
    /*
    Serial.print(F("\tstart sec/tick "));
    Serial.print(startSeconds);
    Serial.print(F(" / "));
    Serial.print(startTicks);
    Serial.print(F(" == "));
    starting.toSerial();
    */
    setTime.toSecTick(setSeconds, setTicks);
    /*
    Serial.print(F("\tset sec/tick "));
    Serial.print(setSeconds);
    Serial.print(F(" / "));
    Serial.print(setTicks);
    Serial.print(F(" == "));
    setTime.toSerial();
    */
    if ((startSeconds < setSeconds) || (startTicks < setTicks)) {
        interrupts();
        //Serial.print(F("startSeconds or ticks greater than setSeconds or ticks"));
        return false;
    }
    runSeconds = startSeconds - setSeconds;
    runTicks = startTicks - setTicks;
    /*
    Serial.print(F("\trunSeconds / runTicks "));
    Serial.print(runSeconds);
    Serial.print(F(" / "));
    Serial.println(runTicks);
    */
    attachInterrupt(irq_num, Clock::tickISR, irq_mode);
    //interrupts(); // timing relaxed
    return true;
}

bool Clock::now(TM& current_time) {
    // Clock not running
    if (runSeconds == (epoch_type)-1)
        return false;

    noInterrupts();  // begin critical section
    epoch_type _runTicks = Clock::runTicks;
    interrupts();  // end critical section

    current_time = setTime;

    // Not automatically applied
    current_time.tm_hertz = runHertz;

    freq_type __runTicks = _runTicks;  // convert once
    epoch_type additionalSeconds = floor(__runTicks / runHertz);
    runSeconds += additionalSeconds;
    epoch_type leftoverTicks = fmod(__runTicks, runHertz);
    epoch_type runTicksReduction = additionalSeconds * runHertz;

    // Time consuming calculation
    bool result = current_time.intSecTick(runSeconds, _runTicks);

    // Catch new ticks elapsed during calculatng
    noInterrupts();  // begin critical section
    _runTicks = Clock::runTicks = Clock::runTicks - runTicksReduction;
    interrupts();  // end critical section

    // Set additional ticks occuring during calculation
    current_time.tm_ticks = _runTicks;

    return result;
}

inline void Clock::calcHertz(const epoch_type& expected_ticks,
                             const freq_type& expected_hertz,
                             const epoch_type& actual_ticks,
                             freq_type& actual_hertz ) {
    actual_hertz = actual_ticks / (expected_ticks / expected_hertz);
}

}  // namespace clock

#endif // CLOCK_H
