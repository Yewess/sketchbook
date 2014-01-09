#include <stdlib.h>
#include <LiquidCrystal.h>
#include <OneWire.h>

/* Global Constants */
const uint8_t PIN_LCD_DA1 = 2;
const uint8_t PIN_LCD_DA2 = 3;
const uint8_t PIN_LCD_DA3 = 4;
const uint8_t PIN_LCD_DA4 = 5;
const uint8_t PIN_LCD_DA5 = 6;
const uint8_t PIN_LCD_DA6 = 7;
const uint8_t PIN_LCD_DA7 = 8;
const uint8_t PIN_LCD_DA8 = 9;
const uint8_t PIN_LCD_RS = 10;
const uint8_t PIN_LCD_EN = 11;
const uint8_t PIN_ONE_WIRE = 12;
const uint8_t PIN_STATUS_LED = 13;
const uint8_t LCD_COLS = 16;
const uint8_t LCD_ROWS = 2;
const uint8_t LCD_SIZE = LCD_COLS * LCD_ROWS;
const uint16_t LCD_UPDATE_INTERVAL = 1000; // > 1 sec is unreadable
// display characters required per temp:
// 3-digits + '.' + 1-tenth + '°' == 6 characters
const uint8_t MAX_DS18XXX = 4;
const uint8_t DS18XXX_SERIAL_CHARS = 6;

/* Global Variables */
uint8_t last_sample_count = 255;
OneWire ow(PIN_ONE_WIRE);
LiquidCrystal lc(PIN_LCD_RS, PIN_LCD_EN,
                 PIN_LCD_DA1, PIN_LCD_DA2, PIN_LCD_DA3, PIN_LCD_DA4,
                 PIN_LCD_DA5, PIN_LCD_DA6, PIN_LCD_DA7, PIN_LCD_DA8);
// Add one byte for safety, incase a null is appended somewhere
char lcd_live[LCD_SIZE + 1] = {0}; // Last written to screen
char lcd_work[LCD_SIZE + 1] = {0}; // to-be written to screen
unsigned long current_time = 0;
unsigned long lcd_last_update = 0;

// Allow two ways to access same data
struct DS18xxxROMCodeStruct {
    uint8_t family;
    uint8_t serial[DS18XXX_SERIAL_CHARS];
    uint8_t crc8;
};

union DS18xxxROMCodeUnion {
    struct  DS18xxxROMCodeStruct struct_map;
    uint8_t byte_map[sizeof(struct_map)];
} ds18xxx_rom_code;

struct DS18xxxMemoryMapStruct {
    int16_t temperature;
    int8_t  high_alarm;
    int8_t  low_alarm;
    uint8_t configuration;
    uint8_t reserved[3];
    uint8_t crc8;
};

union DS18xxxMemoryMapUnion {
    struct DS18xxxMemoryMapStruct struct_map;
    uint8_t byte_map[sizeof(struct_map)];
} ds18xxx_memory;


/* Functions */

// Arduino's 8bit timer wraps after ~49 days
const unsigned long *timerExpired(const unsigned long *currentTime,
                                  const unsigned long *lastTime,
                                  // sometimes this is a macro
                                  unsigned long interval) {
    static unsigned long elapsedTime=0;

    if (*currentTime < *lastTime) { // currentTime has wrapped
        elapsedTime = ((unsigned long)-1) - *lastTime;
        elapsedTime += *currentTime;
    } else { // no wrap
        elapsedTime = *currentTime - *lastTime;
    }
    if (elapsedTime >= interval) {
        return &elapsedTime;
    } else {
        return (unsigned long *) NULL;
    }
}

// lcd updates very slowly, only make required screen changes
void lcd_update() {
    // Don't update faster than lcd_update_interval miliseconds
    if (!timerExpired(&current_time, &lcd_last_update, LCD_UPDATE_INTERVAL))
        return;
    for (uint8_t idx = 0; idx < LCD_SIZE; idx++) {
        if (lcd_live[idx] != lcd_work[idx]) {
            uint8_t row = idx / LCD_COLS;
            uint8_t col = idx - (row * LCD_COLS);
            lc.setCursor(col, row);
            lc.write(lcd_work[idx]);
            lcd_live[idx] = lcd_work[idx];
        }
    }
}

boolean is_ds18xxx() {
    switch (ds18xxx_rom_code.struct_map.family) {
        case 0x28:
        case 0x22:
            return true;
        default:
            return false;
    }
}

String array_to_string(const uint8_t *array, uint8_t length) {
    String conv("");
    for (uint8_t idx=0; idx < length; idx++)
        conv += String(array[idx], HEX) + String(" ");
    return conv;
}

boolean is_rom_code_valid() {
    uint8_t read_crc8 = OneWire::crc8(ds18xxx_rom_code.byte_map, 7);
    uint8_t expt_crc8 = ds18xxx_rom_code.struct_map.crc8; // 8th byte
    //Serial.print("Serial ");
    //Serial.print(array_to_string(ds18xxx_rom_code.struct_map.serial,
    //                             DS18XXX_SERIAL_CHARS));
    if (read_crc8 != expt_crc8) {
        //Serial.println(" CRC not valid");
        return false;
    } else {
        //Serial.println(" CRC valid");
        return true;
    }
}

boolean is_memory_valid() {
    uint8_t read_crc8 = OneWire::crc8(ds18xxx_memory.byte_map, 8);
    uint8_t expt_crc8 = ds18xxx_memory.struct_map.crc8; // 9th byte
    //Serial.print("Data ");
    //Serial.print(array_to_string(ds18xxx_memory.byte_map, 8));
    if (read_crc8 != expt_crc8) {
        //Serial.println(" CRC not valid");
        return false;
    } else {
        //Serial.println(" CRC valid");
        return true;
    }
}

void clean_ds18xxx_vars() {
    memset(&ds18xxx_rom_code, 0, sizeof(ds18xxx_rom_code));
    memset(&ds18xxx_memory, 0, sizeof(ds18xxx_memory));
}

void print_serial() {
    String ds18xxx_serial = "Reading from DS18xxx Serial #: ";
    for (uint8_t idx=0; idx < DS18XXX_SERIAL_CHARS; idx++) {
        ds18xxx_serial += String(ds18xxx_rom_code.struct_map.serial[idx], HEX);
        ds18xxx_serial += String(" ");
    }
    Serial.println(ds18xxx_serial);
}

// Return false if no more devices, otherwise fill
// in ds18xxx_rom_code & ds18xxx_memory
boolean next_ds18xxx() {
    boolean result = ow.search(ds18xxx_rom_code.byte_map);
    if (result && is_rom_code_valid() && is_ds18xxx()) {
        if (ow.reset()) { // device still there
            //print_serial();
            // tell device to sample temperature
            ow.select(ds18xxx_rom_code.byte_map);
            ow.write(0x44); // Convert T
            while (!ow.read_bit())  // probably won't get stuck here
                delay(10); // Don't wait longer than needed
            if (ow.reset()) { // device still there?
                // Read all of devices memory
                ow.select(ds18xxx_rom_code.byte_map);
                ow.write(0xBE); // Read Scratchpad
                ow.read_bytes(ds18xxx_memory.byte_map, sizeof(ds18xxx_memory));
                if (is_memory_valid())
                    return result; // true
            }
        }
  }
  // Wires shorted, bad crc, read error, etc.
  clean_ds18xxx_vars();
  return result; // true or false
}

// 9, 10, 11 or 12bit resolution
uint8_t get_resolution() {
    uint8_t config = ds18xxx_memory.struct_map.configuration;
    // first 5 bits are all 1
    config = config >> 5;
    // 9bit is lowest, and is binary 0 in config then counts up
    return config + 9;
}

int16_t parse_data() {
    // Convert the data to actual temperature
    // because the result is a 16 bit signed integer, it should
    // be stored to an "int16_t" type, which is always 16 bits
    // even when compiled on a 32 bit processor.
    int16_t result = ds18xxx_memory.struct_map.temperature;
    // at lower res, the low bits are undefined, zero them
    switch (get_resolution()) {
        case 9:
            return result & ~7; // 11111111 11111000 mask
        case 10:
            return result & ~3; // 11111111 11111100 mask
        case 11:
            return result & ~1; // 11111111 11111110 mask
        case 12:
            return result; // no manipulation needed
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("SETUP");
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, HIGH);
    lc.begin(LCD_COLS, LCD_ROWS);
    lc.clear();
    lc.noCursor();
    lc.noBlink();
    lc.noAutoscroll();
    memset(lcd_work, (int)' ', LCD_SIZE);
    Serial.println("LOOP");
}

void loop() {
    char dtostrfbuf[10] = {0};
    uint8_t sample_count ,length, idx;
    int16_t temps[MAX_DS18XXX];
    float celsius, fahrenheit, average;
    // No need for super-accurate operational clock
    current_time = millis();

    lcd_update();
    average = 0.0;
    sample_count = 0;
    for (idx=0; idx < MAX_DS18XXX; idx++)
        temps[idx] = 0xFFFF; // sentenel
    ow.reset();
    ow.reset_search();
    for (idx=0; idx < MAX_DS18XXX; idx++) {
        digitalWrite(PIN_STATUS_LED, !digitalRead(PIN_STATUS_LED));
        if (next_ds18xxx()) {
            sample_count++;
            temps[idx] = parse_data();
            average += (float)temps[idx] / 16.0; // calibrated in degrees Celsius
        } else {
            //String line("Read from ");
            //line += String(sample_count, DEC);
            //line += String(" devices");
            //Serial.println(line);
            break;
        }
    }

    // Compare detected devices to last time
    if (last_sample_count == 255) // first loop
        last_sample_count = sample_count;
    else {
        if (last_sample_count != sample_count) // number changed
            memset(lcd_live, (int)' ', LCD_SIZE); // redraw entire lcd
    }

    // calculate average
    average /= (float)sample_count;
    average = average * 1.8 + 32.0; // convert to Fahrenheit

    Serial.print("Avg.: ");
    Serial.print(average,1);
    Serial.print((char)176);
    Serial.println("F");

    for (idx=0; idx < sample_count; idx++) {
        celsius = (float)temps[idx] / 16.0;
        fahrenheit = celsius * 1.8 + 32.0;
        Serial.print(fahrenheit, 1);
        Serial.print(" ");
    }
    Serial.println("");

    // Draw LCD screen, average first then individual readings
    String line("Tavg.: ");
    String suffix((char)0xdf); suffix += String("F"); // degree symbol
    /// 7 character width for float with 2 decimal places
    line += String(dtostrf(average, 6, 2, dtostrfbuf));
    line += suffix;
    length = (LCD_COLS - line.length()) / 2; // padding to center
    line.toCharArray(lcd_work + length, LCD_COLS - length);


    // Devices may be added/removed at any time, re-calculate placement
    // # of readings per row
    idx = 0;
    length = LCD_COLS / (6 + 1); // digits + space
    for (uint8_t row=1; row < LCD_ROWS; row++) {
        Serial.print("Processing row "); Serial.println(row);
        for (uint8_t this_row=0; this_row < length; this_row++) {
            if (idx >= sample_count) {
                Serial.println("    done");
                return;
            }
            Serial.print("    row_sample:"); Serial.print(this_row);
            char *col_ptr = lcd_work + (row * LCD_COLS); // row offset
            col_ptr += this_row * (7 + 1); // sample offset
            uint16_t remaining = (lcd_work + LCD_SIZE) - col_ptr;
            Serial.print(" col_ptr: "); Serial.print((uint16_t) col_ptr);
            Serial.print(" remaining: "); Serial.println(remaining);
            celsius = (float)temps[idx] / 16.0;
            fahrenheit = celsius * 1.8 + 32.0;
            String sample(dtostrf(fahrenheit, -6, 2, dtostrfbuf));
            sample += String((char)0xdf); // degree symbol
            sample.toCharArray(col_ptr, remaining);
            idx++; // next reading
        }
    }
}
