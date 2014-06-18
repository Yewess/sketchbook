#ifndef LCD_THERMOMETER_H
#define LCD_THERMOMETER_H

/*
 * Macros
 */

//#define NODEBUG
#ifndef NODEBUG
    #define DEBUG
    #define D(WHAT) Serial.print(WHAT)
    #define DL(WHAT) Serial.println(WHAT)
#else
    #define D(WHAT) if (false)
    #define DL(WHAT) if (false)
#endif // NODEBUG

struct Data {
    static const struct DispState {
        static const uint8_t now = 0;
        static const uint8_t one = 1;
        static const uint8_t four = 2;
        static const uint8_t snooze = 3;
    } DispState;

    static const struct TransState {
        static const uint8_t enter = 0;
        static const uint8_t hold = 1;
        static const uint8_t exit = 2;
    } TransState;

    static const struct LcdBlState {
        static const uint8_t up = 0;
        static const uint8_t down = 2;
        static const uint8_t equal = 3;
        static const uint8_t on = 4;
        static const uint8_t off = 5;
    } LcdBlState;

    #ifdef DEBUG
    static const uint8_t PIN_SERIAL_RX = 0;
    static const uint8_t PIN_SERIAL_TX = 1;
    static const uint32_t SERIAL_BAUD = 115200;
    #endif // DEBUG

    // Peripheral Power
    static const uint8_t PIN_PERIPH_POWER = A0;

    // LCD Interface
    static const uint8_t PIN_LCD_RS = A1;
    static const uint8_t PIN_LCD_EN = A2;
    static const uint8_t PIN_LCD_DA4 = 5;
    static const uint8_t PIN_LCD_DA5 = 6;
    static const uint8_t PIN_LCD_DA6 = 7;
    static const uint8_t PIN_LCD_DA7 = 8;
    static const uint8_t PIN_LCD_BL = 9; // pwm

    // LCD Details
    static const uint8_t LCD_COLS = 16;
    static const uint8_t LCD_ROWS = 2;

    // One Wire Bus
    static const uint8_t PIN_OWB_LOCAL = 11;
    static const uint8_t PIN_OWB_REMOTE = 10;
    static const uint8_t MAXRES = 10;
    static const int16_t FAULTTEMP = 12345;

    // Doing "work" indicator
    static const uint8_t PIN_STATUS_LED = 13;

    // Timing data
    static const uint16_t WDT8SEC           = 8000 + -215; // 8 sec avg WDT osc time
    static const uint32_t SLEEPTIME         = 600000; // 10 minutes
    static const uint8_t SLEEP_CYCLES       = SLEEPTIME / WDT8SEC;
    static const Millis LCD_UPDATE_INTERVAL = 1000; // < 1000ms is unreadable
    static const Millis TEMP_SAMPLE_INTVL   = 600000; // 10 minutes (in ms)
    static const Millis TEMP_SMA_WINDOW_S   = 3600000; // 1 hour SMA (short)
    static const Millis TEMP_SMA_WINDOW_L   = 14400000; // 4 hour SMA (long)
                                      // (TEMP_SMA_WINDOW_S / TEMP_SAMPLE_INTVL);
    static const uint16_t SMA_POINTS_S = 6;
                                      // (TEMP_SMA_WINDOW_L / TEMP_SAMPLE_INTVL);
    static const uint16_t SMA_POINTS_L = 24;
    // Allow re-use of lcdBlStateTimer for transState
    static const uint8_t LCD_BL_RAMP_DIVISIONS = 20;  // updates per second
                                     // 1000 / LCD_BL_RAMP_DIVISIONS;
    const Millis LCD_BL_RAMP_INTERVAL = 50;  // ms intvl btwn ticks
    // Durations, in ramp-div updates (ticks) for each stage
    static const uint16_t LCD_BL_RAMP_TICKS = 20;  // 1-second in ticks
    static const uint16_t LCD_BL_EQ_TICKS = 10; //  .5-seconds in ticks
    static const uint16_t LCD_BL_ON_TICKS = 160;  // 8-seconds in ticks
    static const uint16_t LCD_BL_OFF_TICKS = 0;  // zero-time

    // max value for dispCycles
    static const uint8_t MAX_DISPLAY_CYCLES = 3;  // now, one, four...before snooze
    // Temperature unit
    static const bool CELSIUS = false;
    static const char DEGREE_CHAR = 223;

    // OneWire bus and sensors
    OneWire owb_local;
    OneWire owb_remote;
    MaxDS18B20::MaxRom rom_local;
    MaxDS18B20::MaxRom rom_remote;
    MaxDS18B20* max_local_p;
    MaxDS18B20* max_remote_p;

    uint8_t lcdBlVal;  // PWM value to set (0-255)
    LiquidCrystal lcd;

    // master clock
    Millis current_time;
    uint32_t sleepAdvance; // total sleepCycleCounter
    volatile uint8_t sleepCycleCounter;

    // Temperature readings
    int16_t now_local;
    int16_t now_remote;
    SimpleMovingAvg sma_local_s;
    SimpleMovingAvg sma_local_l;
    SimpleMovingAvg sma_remote_s;
    SimpleMovingAvg sma_remote_l;

    // serviced by main loop, transitions in transState
    StateMachine dispState;  // data display state (now, 1h, 4h,, snooze)
    uint8_t dispCycles;  // number of times through dispState

    // serviced by dispState, transitions in lcdBlState
    StateMachine transState;  // inner-display state (enter, hold, exit, next)
    uint8_t lcdblStateCache;  // Cache during dispState cycle

    // serviced by transState, transitions in lcdBLStateTimer
    StateMachine lcdBlState;    // backlight state (ramp up, ramp-down, hold-on, hold-off)

    // inc dispTics every LCD_BL_RAMP_INTERVAL miliseconds
    TimedEvent lcdBlStateTimer; // transition timer for lcdBlState
    uint16_t lcdblTicks;  // nr lcdBlStateTimer fires

    // Member Functions
    Data(void);
    void setup(void);
    void loop(void);
    void init_max(OneWire& owb, MaxDS18B20::MaxRom& rom, MaxDS18B20*& max_pr) const;
    void all_pins_up(void);
    void all_pins_down(void);
    void start_conversion(MaxDS18B20::MaxRom& rom, MaxDS18B20*& max_pr) const ;
    void wait_conversion(MaxDS18B20::MaxRom& rom, MaxDS18B20*& max_pr)const ;
    int16_t get_temp(MaxDS18B20::MaxRom& rom, MaxDS18B20*& max_pr)const ;
    int16_t sma_append(SimpleMovingAvg& sma, uint16_t temp);
} data;

#endif // LCD_THERMOMETER_H
