#ifndef LCD_THERMOMETER_H
#define LCD_THERMOMETER_H

/*
 * Macros
 */

//#define NODEBUG
#ifndef NODEBUG
    #define DEBUG
#endif

#ifdef DEBUG
    #define D(...) Serial.print(__VA_ARGS__)
    #define DL(...) Serial.println(__VA_ARGS__)
#else
    #define D(...) if (false)
    #define DL(...) if (false)
#endif // DEBUG

struct Data {
    // types

    typedef struct LcdBlState {
        static const uint8_t up  = 0;
        static const uint8_t on  = up + 1;
        static const uint8_t down = on + 1;
        static const uint8_t off = down + 1;
    } LcdBlState;

    typedef struct LcdDispState {
        static const uint8_t local_now = 0;
        static const uint8_t remote_now = local_now + 1;
        static const uint8_t diff_now = remote_now + 1;
        static const uint8_t local_60mSMA = diff_now + 1;
        static const uint8_t remote_60mSMA = local_60mSMA + 1;
        static const uint8_t diff_60mSMA = remote_60mSMA + 1;
        static const uint8_t local_24hSMA = diff_60mSMA + 1;
        static const uint8_t remote_24hSMA = local_24hSMA + 1;
        static const uint8_t diff_24hSMA = remote_24hSMA + 1;
    } LcdDispState;

    // LCD Interface
    static const uint8_t PIN_LCD_RS = A0;
    static const uint8_t PIN_LCD_EN = A1;
    static const uint8_t PIN_LCD_DA0 = A2;
    static const uint8_t PIN_LCD_DA1 = A3;
    static const uint8_t PIN_LCD_DA2 = 4;
    static const uint8_t PIN_LCD_DA3 = 5; // funky pwm
    static const uint8_t PIN_LCD_DA4 = 6; // funky pwm
    static const uint8_t PIN_LCD_DA5 = 7;
    static const uint8_t PIN_LCD_DA6 = 8;
    static const uint8_t PIN_LCD_DA7 = 12;
    static const uint8_t PIN_LCD_BL = 3; // pwm

    // LCD Details
    static const uint8_t LCD_COLS = 20;
    static const uint8_t LCD_ROWS = 2;

    // One Wire Bus
    static const uint8_t PIN_OWB_LOCAL = A4; // i2c SDA
    static const uint8_t PIN_OWB_REMOTE = A5; // i2c SCL
    static const uint8_t MAXRES = 10;
    static const int16_t FAULTTEMP = 12345;

    // Power
    static const uint8_t PIN_STATUS_LED = 13;

    // Timing data
    static const Millis LCD_UPDATE_INTERVAL = 1000; // < 1000ms is unreadable
    static const Millis TEMP_SAMPLE_INTVL   = 600000; // 10 minutes
    static const Millis TEMP_SMA_WINDOW_S   = 3600000; // 1 hour SMA (short)
    static const Millis TEMP_SMA_WINDOW_L   = 86400000; // 24 hour SMA (long)
    static const Millis SMA_POINTS_S = (TEMP_SMA_WINDOW_S / TEMP_SAMPLE_INTVL);
    static const Millis SMA_POINTS_L = (TEMP_SMA_WINDOW_L / TEMP_SMA_WINDOW_S);

    // Data Members
    OneWire owb_local;
    OneWire owb_remote;

    MaxDS18B20::MaxRom rom_local;
    MaxDS18B20::MaxRom rom_remote;
    MaxDS18B20* max_local_p;
    MaxDS18B20* max_remote_p;

    LCD lcd;

    Millis current_time;

    SimpleMovingAvg sma_local_s;
    SimpleMovingAvg sma_local_l;
    SimpleMovingAvg sma_remote_s;
    SimpleMovingAvg sma_remote_l;

    StateMachine lcdBlState;
    StateMachine lcdDispState;

    uint8_t lcdBlVal;

    // Member Functions
    // Constructor
    Data(void);
    void setup(void);
    void loop(void);

    private:
    void init_max(OneWire& owb, MaxDS18B20::MaxRom& rom, MaxDS18B20*& max_pr) const;
    void start_conversion(MaxDS18B20::MaxRom& rom, MaxDS18B20*& max_pr) const ;
    void wait_conversion(MaxDS18B20::MaxRom& rom, MaxDS18B20*& max_pr)const ;
    int16_t get_temp(MaxDS18B20::MaxRom& rom, MaxDS18B20*& max_pr)const ;
    int16_t sma_append(SimpleMovingAvg& sma, MaxDS18B20::MaxRom& rom,
                       MaxDS18B20*& max_pr) const;
} data;

#endif // LCD_THERMOMETER_H
