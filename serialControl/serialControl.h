#ifndef SERIALCONTROL_H
#define SERIALCONTROL_H

struct StateBits {
    static const uint8_t input = 0;
    static const uint8_t inputPullup = 1;
    static const uint8_t output = 2;
    static const uint8_t analog = 3;
    static const uint8_t reserved1 = 4;
    static const uint8_t reserved2 = 5;
    static const uint8_t reserved3 = 6;
    static const uint8_t reserved4 = 7;
};

typedef struct Pin {
    const char* name;
    const uint8_t number;
    // constructor
    Pin(const char *pin_name, const uint8_t pin_number, boolean isDigital=true);
    // mode change
    void modeInput(void);
    void modeInputPullup(void);
    void modeOutput(void);
    // I/O
    int16_t readDigital(int16_t* newValue=NULL); // returns old value
    int16_t readAnalog(int16_t* newValue=NULL); // returns old value
    int16_t oldValue(int16_t* oldValue=NULL) const; // no read
    int16_t writeHigh(int16_t* oldValue=NULL);  // returns old value
    int16_t writeLow(int16_t* oldValue=NULL);  // returns old value
    // Info
    bool isInput(void) const;
    bool isInputPullup(void) const;
    bool isOutput(void) const;
    bool isAnalog(void) const;
    char *strDetail(char *buff, uint8_t max_len) const;

    private:
    int16_t value; // -1: unknown/invalid
    uint8_t state;  // bitfield
} Pin;

typedef struct Pins {
    const uint8_t nPins;
    Pin* pin;

    Pins(const uint8_t number_pins, Pin* pins)
         : nPins(number_pins), pin(pins) {}
    void allModeInput(void) {
        for (uint8_t c = 0; c < nPins; c++)
            pin[c].modeInput();
    };
    void allModeInputPullup(void) {
        for (uint8_t c = 0; c < nPins; c++)
            pin[c].modeInputPullup();
    };
    void allModeOutput(void) {
        for (uint8_t c = 0; c < nPins; c++)
            pin[c].modeOutput();
    };
} Pins;

typedef class Atmega328p : public Pins {
    public:
    static const uint8_t NPINS = 20;
    Atmega328p(void) : Pins::Pins(NPINS, pin) { allModeInput(); };
    Pin pin[NPINS] = {Pin("Digital 0", 0),
                      Pin("Digital 1", 1),
                      Pin("Digital 2", 2),
                      Pin("Digital 3", 3),
                      Pin("Digital 4", 4),
                      Pin("Digital 5", 5),
                      Pin("Digital 6", 6),
                      Pin("Digital 7", 7),
                      Pin("Digital 8", 8),
                      Pin("Digital 9", 9),
                      Pin("Digital 10", 10),
                      Pin("Digital 11", 11),
                      Pin("Digital 12", 12),
                      Pin("Digital 13", 13),
                      Pin("Analog 0 (14)", 14, false),
                      Pin("Analog 1 (15)", 15, false),
                      Pin("Analog 2 (16)", 16, false),
                      Pin("Analog 3 (17)", 17, false),
                      Pin("Analog 4 (18)", 18, false),
                      Pin("Analog 5 (19)", 19, false)};
} Atmega328p;


namespace MenuCB {
    void digitalRead(void);
    void analogRead(void);
    void allRead(void);
    void writeHigh(void);
    void writeLow(void);
    void setInput(void);
    void setInputPullup(void);
    void setOutput(void);
};

/*
 * Implementation
 */

Pin::Pin(const char *pin_name, const uint8_t pin_number, boolean isDigital)
         : name(pin_name), number(pin_number), value(-1) {
    state = 0;
    if (!isDigital)
        bitSet(state, StateBits::analog);
}

void Pin::modeOutput(void) {
    if (bitRead(state, StateBits::output))
        return;
    // Turn off pullup, so output doesn't go HIGH
    if (bitRead(state, StateBits::inputPullup))
        pinMode(number, INPUT);
    pinMode(number, OUTPUT);
    bitClear(state, StateBits::inputPullup);
    bitClear(state, StateBits::input);
    bitSet(state, StateBits::output);
    value = 0; // LOW
}

void Pin::modeInput(void) {
    if (bitRead(state, StateBits::input))
        return;
    // If left HIGH, will set input pullup
    if (bitRead(state, StateBits::output))
        digitalWrite(number, LOW);
    pinMode(number, INPUT);
    bitClear(state, StateBits::output);
    bitClear(state, StateBits::inputPullup);
    bitSet(state, StateBits::input);
    value = -1; // unknown
}

void Pin::modeInputPullup(void) {
    if (bitRead(state, StateBits::inputPullup))
        return;
    // Warning Pin will go HIGH!
    pinMode(number, INPUT_PULLUP);
    bitClear(state, StateBits::input);
    bitClear(state, StateBits::output);
    bitSet(state, StateBits::inputPullup);
    value = -1; // unknown
}

int16_t Pin::readDigital(int16_t* newValue) {
    uint16_t oldValue = value;
    value = digitalRead(number);
    if (newValue)
        *newValue = value;
    return oldValue;
}

int16_t Pin::readAnalog(int16_t* newValue) {
    bool wasOutput = bitRead(state, StateBits::output);
    int16_t old = oldValue();
    modeInput();  // output HIGH would disturb value
    if (!bitRead(state, StateBits::analog))
        return -1;  // invalid
    else
        value = analogRead(number);
    if (newValue)
        *newValue = value;
    if (wasOutput)
        Pin::modeOutput();  // back to original output mode
        if (!isAnalog() && old > -1)
            if (old != 0)
                writeHigh();
            else
                writeLow();
        // else - analog write not supported yet
    return old;
}

int16_t Pin::oldValue(int16_t* oldValue) const {
    if (oldValue)
        *oldValue = value;
    return value;
}

int16_t Pin::writeHigh(int16_t* oldValue) {
    if (isOutput()) {
        digitalWrite(number, HIGH);
        if (oldValue)
            *oldValue = value;
        value = HIGH;
        return value;
    } else // not an output, return invalid
        if (oldValue)
            *oldValue = -1;
        return -1;
}

int16_t Pin::writeLow(int16_t* oldValue) {
    if (isOutput()) {
        digitalWrite(number, LOW);
        if (oldValue)
            *oldValue = value;
        value = LOW;
        return value;
    } else // not an output, return invalid
        if (oldValue)
            *oldValue = -1;
        return -1;
}

inline bool Pin::isInput(void) const {
    if (bitRead(state, StateBits::input))
        return true;
    else
        return false;
}

inline bool Pin::isInputPullup(void) const {
    if (bitRead(state, StateBits::inputPullup))
        return true;
    else
        return false;
}

inline bool Pin::isOutput(void) const {
    if (bitRead(state, StateBits::output))
        return true;
    else
        return false;
}

inline bool Pin::isAnalog(void) const {
    if (bitRead(state, StateBits::analog))
        return true;
    else
        return false;
}

char *Pin::strDetail(char *buff, uint8_t max_len) const {
    uint8_t minLen = strlen(name) + 3 + 1 + 4 + 1;
    if (max_len < minLen)
        return NULL;  // not enough room

    strcpy(buff, name);
    buff += strlen(buff);

    if (isInput())
        strcpy(buff, "(i) ");
    else if (isInputPullup())
        strcpy(buff, "(!) ");
    else if (isOutput())
        strcpy(buff, "(o) ");
    else
        strcpy(buff, "(?) ");
    buff += strlen(buff);

    if (value < 0)
        strcpy(buff, "?");
    else if (isAnalog())
        itoa(value, buff, 10); // \n is added
    else if (value != 0)
        strcpy(buff, "HIGH"); // \n is added
    else
        strcpy(buff, "LOW"); // \n is added
    return buff;
}

#endif // SERIALCONTROL_H
