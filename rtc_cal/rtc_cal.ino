#include <avr/eeprom.h>
#include <string.h>
#include <Wire.h>
#include <Rtc_Pcf8563.h>  // https://github.com/elpaso/Rtc_Pcf8563.git

class Eeprom;

// Hold all EEPROM initial data private, prevent accidents.
class EepromData {
    friend Eeprom;
    uint64_t magic{0x25f7bb7900000003};

    uint32_t ticksPerHour{60UL * 60UL * 32768UL};
    uint8_t clkoutPin{12};
    uint8_t clkoutMode{INPUT_PULLUP};

    uint8_t intPin{8};
    uint8_t intMode{INPUT_PULLUP};

    enum bitsBV { CALIBRATED }; // for reference only
    uint8_t bits{0b00000000};
} eepromData EEMEM;  // data will be in .eep file

// Access methods for EepromData
class Eeprom : EepromData {
    public:
    bool magicValid(uint64_t& what_magic) { return what_magic == EepromData::magic; }

    bool load(void) {
        EepromData temp;
        memset(&temp, 0xFF, sizeof(EepromData));
        eeprom_busy_wait();
        eeprom_read_block(&temp, &eepromData, sizeof(EepromData));
        Serial.println(F("\tLoaded EEPROM"));
        if (magicValid(temp.magic))
            memcpy(this, &temp, sizeof(eepromData));
        else
            return false;
        return true;
    }

    bool save(void) {
        Serial.println(F("\tSaving EEPROM"));
        eeprom_busy_wait();
        eeprom_update_block(this, &eepromData, sizeof(EepromData));
        return load();
    }

    void erase(void) {
        EepromData temp;
        memset(&temp, 0xFF, sizeof(EepromData));
        eeprom_busy_wait();
        eeprom_update_block(&temp, &eepromData, sizeof(EepromData));
    }

    Eeprom(void) {
        Serial.println(F("Initializing EEPROM"));
        if (!load()) {
            // Data not flashed, or magic changed, re-init from defaults
            Serial.println(F("\tInvalid magic, resetting"));
            EepromData temp;
            memcpy(this, &temp, sizeof(EepromData));
            save();
        }
        if (magicValid(magic))
            Serial.println(F("\tmagic is valid"));
        else
            Serial.println(F("\tmagic invalid!"));
    }

    void dump(void) {
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

    // accessor methods
    int32_t getTicksPerHour(void) const { return ticksPerHour; }
    void setTicksPerHour(double value) { ticksPerHour = value; }

    bool getCalibrated(void) const { return bitRead(bits, bitsBV::CALIBRATED); }
    void setCalibrated(bool value) { bitWrite(bits, bitsBV::CALIBRATED, value); }

    uint8_t getClkoutPin(void) const { return clkoutPin; }
    void setClkoutPin(uint8_t value) { clkoutPin = value; }
    uint8_t getClkoutMode(void) const { return clkoutMode; }
    void setClkoutMode(uint8_t value) { clkoutPin = value; }

    uint8_t getIntPin(void) const { return intPin; }
    void setIntPin(uint8_t value) { intPin = value; }
    uint8_t getIntMode(void) const { return intMode; }
    void setIntMode(uint8_t value) { intMode = value; }
};

class Clock {
    Eeprom& _eeprom;

    void setupPin(uint8_t pin, uint8_t mode) { pinMode(pin, mode); }

    bool tick(bool pinValue) {
        // Falling edge is what counts
        static bool was_high = false;
        if (pinValue) {  // pin is high
            if (!was_high)
                was_high = true;
        } else {  // pin is low
            if (was_high) {
                was_high = false;
                return true;
            }
        }
        return false;
    }
    uint32_t countTicksPerHour(void) {
        uint32_t current=0;
        uint32_t lastSecond=millis();
        uint32_t ticks = 0UL;
        uint32_t start = millis();
        uint32_t stop = start + 1000UL * 60UL * 60UL;  // ms per hour
        int32_t last_ms_err=0;
        digitalWrite(LED_BUILTIN, HIGH);
        while (true) {
            current = millis();
            noInterrupts();
            if (tick(digitalRead(_eeprom.getClkoutPin())))
                ticks++;
            if (current >= stop)
                break;
            interrupts();
            if (current >= (lastSecond + 1000)) {
                lastSecond = current;
                digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
                uint32_t seconds = ticks / 32767UL;
                uint32_t expected_ticks = seconds * 32768;
                int32_t diff = expected_ticks - ticks;
                int32_t ms_err = ((1000000L / 32767L) * diff) / 1000L;
                Serial.print("\r");
                Serial.print(ms_err); Serial.print(F("mS error ("));
                Serial.print(ms_err - last_ms_err);
                Serial.print(F(" ms/s)          "));
                last_ms_err = ms_err;
            }
        }
        interrupts();
        digitalWrite(LED_BUILTIN, LOW);
        Serial.println();
        return ticks;
    }

    public:
    Clock(Eeprom& eeprom) : _eeprom(eeprom) {
        setupPin(_eeprom.getClkoutPin(), _eeprom.getClkoutMode());
        setupPin(_eeprom.getIntPin(), _eeprom.getIntMode());
    }
    void calTicksPerHour(void);  // Takes 24 hours!
};

void Clock::calTicksPerHour(void) {
    if (!_eeprom.getCalibrated())
        // Monitor over 24 hours (incorp. temp changes etc)
        for (int i = 0; i < 24; i++) {
            uint32_t eepromTPH = _eeprom.getTicksPerHour();
            uint32_t measured = countTicksPerHour();  // Takes an hour!
            int32_t diff = measured - eepromTPH;
            Serial.println();
            Serial.print(eepromTPH);
            Serial.print(F(" - ")); Serial.print(measured);
            Serial.print(F(" = ")); Serial.print(diff);
            Serial.print(F("  (")); Serial.print(measured / (60UL * 60UL));
            Serial.println(F(" Hz)"));
            static int32_t lastDiff = 0x7FFFFFFF;
            if (abs(diff) < abs(lastDiff)) {
                _eeprom.setTicksPerHour(measured);
                lastDiff = diff;
            }
            // In case of bugs, don't break EEPROM
            static int writesLeft = 24;
            if (--writesLeft > 0)
                _eeprom.save();
    }
    _eeprom.setCalibrated(true);
}

Rtc_Pcf8563 rtc;
Clock* clock;

void setup(void) {
    Serial.begin(115200);
    Serial.println(F("Setup()"));

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    // call constructor _after_ Serial is setup
    static Eeprom eeprom;
    static Clock _clock(eeprom);
    clock = &_clock;
    clock->calTicksPerHour();

    Wire.begin();
    rtc.clearStatus();
    rtc.setSquareWave(SQW_32KHZ);

    Serial.println(F("loop()"));
}

void loop(void) {
}
