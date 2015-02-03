#ifndef EXCEPTION_H
#define EXCEPTION_H

enum Exceptions {E_NO_ERROR, E_EEPROM_INIT, E_RTC_INIT, E_CLKOUT_INIT,
                 E_CLOCK_INIT, E_RTC_UNSET, E_CLOCK_STALL};

class Exception {
    private:
    uint16_t pauseDelay;
    uint16_t blinkDelay;
    public:
    Exception(uint16_t pauseDelayValue, uint16_t blinkDelayValue)
              :
              pauseDelay(pauseDelayValue), blinkDelay(blinkDelayValue) {};
    void raise(int indicator) {
        if (indicator == 0)
            return;  // no exception
        digitalWrite(LED_BUILTIN, LOW);
        Serial.print(F("EXCEPTION: "));
        Serial.println(indicator);
        while (true) {
            delay(pauseDelay);
            for (uint32_t c = abs(indicator) * 2; c > 0; c--) {
                digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
                delay(blinkDelay);
            }
        }
    }
};

#endif // EXCEPTION_H
