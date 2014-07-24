#ifndef MASTERYEWAYE_H
#define MASTERYEWAYE_H

//#define NODEBUG
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

struct OwbStatus {
    static const uint8_t converting = 1;
    static const uint8_t notFound = 3;
    static const uint8_t romBadCrc = 4;
    static const uint8_t ramBadCrc = 5;
    static const uint8_t busError = 6;
    static const uint8_t notDs18x20 = 7;
    static const uint8_t complete = 8;
    static const uint8_t changeDev = 9;
};

typedef struct Owb {
    static const int16_t checkStatus = -850;
    OneWire bus;
    uint8_t status;
    int16_t x10degrees;
    SimpleMovingAvg oneHour;
    SimpleMovingAvg fourHour;
    Owb(uint8_t pin, uint16_t onePoints, uint16_t fourPoints)
        : bus(pin), x10degrees(checkStatus),
          oneHour(onePoints), fourHour(fourPoints) {};
} Owb;

typedef struct Sma {
    Owb owbA;
    Owb owbB;
    Sma(uint8_t pinA, uint8_t pinB,
        uint16_t onePointsA, uint16_t onePointsB,
        uint16_t fourPointsA, uint16_t fourPointsB)
        : owbA(pinA, onePointsA, fourPointsA),
          owbB(pinB, onePointsB, fourPointsB) {};
} Sma;

static const uint16_t wdtSleep8 = (Millis)7806; // 8 sec avg WDT osc time
static const Millis lcdUpdate = (Millis)250; // < 1000ms is unreadable
static const Millis wakeTime = (Millis)2000;  // stay awake this long
#ifdef DEBUG
    static const Millis tempSmaSample = (Millis)60000; // 1 minute (in ms)
    static const Millis tempSmaOne = (Millis)360000; // 6 minutes SMA (short)
    static const Millis tempSmaFour = (Millis)1440000; // 24 minute SMA (long)
#else
    static const Millis tempSmaSample = (Millis)600000; // 10 minutes (in ms)
    static const Millis tempSmaOne = (Millis)3600000; // 1 hour SMA (short)
    static const Millis tempSmaFour = (Millis)14400000; // 4 hour SMA (long)
#endif // DEBUG
static const uint8_t lcdRows = 2;
static const uint8_t lcdCols = 16;
static const bool celsius = false;
static const char degreeSymb = 223;

// globals

volatile Millis sleepCycleCounter = 0;

LiquidCrystal lcd(Pin::lcdRS, Pin::lcdEN, Pin::lcdD4,
                  Pin::lcdD5, Pin::lcdD6, Pin::lcdD7);

Encoder enc(Pin::encA, Pin::encC);

Sma sma(Pin::owbA, Pin::owbB,
        tempSmaOne / tempSmaSample, tempSmaOne / tempSmaSample,
        tempSmaFour / tempSmaSample, tempSmaFour / tempSmaSample);

Millis currentTime = 0;

#ifdef DEBUG
    bool pinsPrinted = false;
#endif DEBUG

#endif // MASTERYEWAYE_H
