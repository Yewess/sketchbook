#ifndef MASTERYEWAYE_H
#define MASTERYEWAYE_H

#define NODEBUG
#ifndef NODEBUG
    #define DEBUG
    #define D(...) Serial.print(__VA_ARGS__)
    #define DL(...) Serial.println(__VA_ARGS__)
#else
    #undef DEBUG
    #define D(...) if (false)
    #define DL(...) if (false)
#endif // NODEBUG

struct Pin {

    #ifdef DEBUG
        static const uint8_t serialRx = 0;
        static const uint8_t serialTx = 1;
        static const uint32_t serialBaud = 115200;
    #endif // DEBUG

    // Low-side peripheral switches
    static const uint8_t lcdMosfet = 12;
    static const uint8_t owbMosfet = 13;
    static const uint8_t arfMosfet = A3;

    // One-wire bus
    static const uint8_t owbA = 11;
    static const uint8_t owbB = 10;

    // Rotary Encoder
    static const uint8_t encA = 2;
    static const uint8_t button = 4; // button
    static const uint8_t encC = 3;

    // Battery Sense pin
    static const uint8_t battSense = A0;

    // LCD Connections
    static const uint8_t lcdBL = 9;
    static const uint8_t lcdRS = A1;
    static const uint8_t lcdEN = A2;
    static const uint8_t lcdD4 = 5;
    static const uint8_t lcdD5 = 6;
    static const uint8_t lcdD6 = 7;
    static const uint8_t lcdD7 = 8;

    // IIC
    static const uint8_t sda = A4;
    static const uint8_t scl = A5;
};

struct PowerSense {
    static const unsigned int arfMv = 3859;
    static const unsigned int powDivR1 = 11000; // Ohms
    static const unsigned int powDivR2 = 15000; // Ohms
    // Voltage divider applied to + terminal to scale range under arfMv value
    //static const double powDivFact = ( (double)powDivR2 /
    //                                 ((double)powDivR1 + (double)powDivR2));
    static const double powDivFact = 0.5769230769230769;
    static const unsigned int battFull = 6200; // 4x AA in mV
    static const unsigned int battEmpty = 4200; // Exhausted
    static const unsigned int offMv = 100; // Compensate for protection diode drop
};

struct OwbStatus {
    static const uint8_t converting = 1;
    static const uint8_t notFound = 3;
    static const uint8_t romBadCrc = 4;
    static const uint8_t ramBadCrc = 5;
    static const uint8_t busError = 6;
    static const uint8_t notDs18x20 = 7;
    static const uint8_t complete = 8;
    static const uint8_t none = 9;
};

struct EncState {
    static const uint8_t current = 0;
    static const uint8_t one = 1;
    static const uint8_t four = 2;
    static const uint8_t twelve = 3;
    static const uint8_t battery = 4;
};

typedef struct Owb {
    static const int16_t checkStatus = -850;
    OneWire bus;
    uint8_t status;
    int16_t x10degrees;
    SimpleMovingAvg oneHour;
    SimpleMovingAvg fourHour;
    SimpleMovingAvg twelveHour;
    Owb(uint8_t pin, uint16_t onePoints, uint16_t fourPoints, uint16_t twelvePoints)
        : bus(pin), status(OwbStatus::none), x10degrees(checkStatus),
          oneHour(onePoints), fourHour(fourPoints), twelveHour(twelvePoints) {};
} Owb;

typedef struct Sma {
    Owb owbA;
    Owb owbB;
    Sma(uint8_t pinA, uint8_t pinB,
        uint16_t onePointsA, uint16_t onePointsB,
        uint16_t fourPointsA, uint16_t fourPointsB,
        uint16_t twelvePointsA, uint16_t twelvePointsB)
        : owbA(pinA, onePointsA, fourPointsA, twelvePointsA),
          owbB(pinB, onePointsB, fourPointsB, twelvePointsB) {};
} Sma;

static const uint16_t wdtSleep8 = 7806UL; // 8 sec avg WDT osc time
static const Millis lcdTime = 500UL; // < 1000ms is unreadable
static const Millis wakeTime = 100UL;  // update wake counter
static const Millis encTime = 1UL;
#ifdef DEBUG
    static const uint8_t sleepCycleMultiplier = 1;
    static const Millis batterySample = 270000UL; // 4.5 minutes (in ms)
    static const Millis tempSmaSample = 60000UL; // 1 minute (in ms)
    static const Millis tempSmaOne = 360000UL; // 6 minutes SMA
    static const Millis tempSmaFour = 1440000UL; // 24 minute SMA
    static const Millis tempSmaTwelve = 4320000UL;  // 1.2 hour SMA
#else
    static const uint8_t sleepCycleMultiplier = 8;
    static const Millis batterySample = 2700000UL; // 45 minutes (in ms)
    static const Millis tempSmaSample = 600000UL; // 10 minutes (in ms)
    static const Millis tempSmaOne = 3600000UL; // 1 hour SMA
    static const Millis tempSmaFour = 14400000UL; // 4 hour SMA
    static const Millis tempSmaTwelve = 43200000UL;  // 12 hours SMA
#endif // DEBUG
static const uint8_t lcdRows = 2;
static const uint8_t lcdCols = 16;
static const bool celsius = false;
static const int8_t wakeMinMultiplier = 10; // 1 sec
static const int16_t wakeMaxMultiplier = 300; // 30 sec

// globals

uint16_t encMinMax = 4; // MSB: Min; LSB: Max
uint8_t encValue = EncState::current;
bool lcdDisplay = false;
bool lcdRefresh = true;
int16_t wakeCounter = wakeMinMultiplier; // stay awake until 0
volatile Millis sleepCycleCounter = 0;

LiquidCrystal lcd(Pin::lcdRS, Pin::lcdEN, Pin::lcdD4,
                  Pin::lcdD5, Pin::lcdD6, Pin::lcdD7);

Encoder enc(Pin::encA, Pin::encC);

Button button(Pin::button, BUTTON_PULLUP);

Sma sma(Pin::owbA, Pin::owbB,
        tempSmaOne / tempSmaSample, tempSmaOne / tempSmaSample,
        tempSmaFour / tempSmaSample, tempSmaFour / tempSmaSample,
        tempSmaTwelve / tempSmaOne, tempSmaTwelve / tempSmaOne);

Millis currentTime = 0;

unsigned int battMvPrev = 0;
unsigned int battMv = 0;

#ifdef DEBUG
    bool pinsPrinted = false;
#endif //DEBUG

#endif // MASTERYEWAYE_H
