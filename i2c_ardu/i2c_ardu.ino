// i2c lcd control
// by Chris Evich
// License: LGPL v2

/*
 * From http://dsscircuits.com/articles/arduino-i2c-slave-guide.html
 *
 * i2c "Register" get/set model, register types:
 *     Status – read-only device status (useful for interrupts)
 *     Data – read-only data that you are looking to collect from your device.
 *     Mode – r/w "Runtime" operational type, single-shot, continuous, sleep, etc.
 *     Configuration – r/w "Runtime" configuration, baud rate, sample range, etc.
 *     Identification – read-only, store some type of ID or firmware revisions
*/

#include <LiquidCrystal.h>
#include <Wire.h>

enum i2clcd_e {
    I2CLCD_NOOP,                // 0
    I2CLCD_CLEAR,               // 1
    I2CLCD_HOME,                // 2
    I2CLCD_SETCURSOR,           // 3
    I2CLCD_WRITE,               // 4
    I2CLCD_PRINT,               // 5
    I2CLCD_CURSOR,              // 6
    I2CLCD_NOCURSOR,            // 7
    I2CLCD_BLINK,               // 8
    I2CLCD_NOBLINK,             // 9
    I2CLCD_DISPLAY,             // 10
    I2CLCD_NODISPLAY,           // 11
    I2CLCD_SCROLLDISPLAYLEFT,   // 12
    I2CLCD_SCROLLDISPLAYRIGHT,  // 13
    I2CLCD_AUTOSCROLL,          // 14
    I2CLCD_NOAUTOSCROLL,        // 15
    I2CLCD_LEFTTORIGHT,         // 16
    I2CLCD_RIGHTTOLEFT,         // 17
    I2CLCD_CREATECHAR,          // 18
} i2c_call, lcd_call;

const int status_led = 13;
const int lcd_rs = 10;
const int lcd_en = 11;
const int lcd_da[] = {2, 3, 4, 5, 6, 7, 8, 9}; /* d0 - d7 arduino pins*/
const int lcd_cols = 16;
const int lcd_rows = 2;
const int lcd_size = lcd_cols * lcd_rows;
LiquidCrystal lcd(lcd_rs, lcd_en,
                  lcd_da[0], lcd_da[1], lcd_da[2], lcd_da[3],
                  lcd_da[4], lcd_da[5], lcd_da[6], lcd_da[7]);

int i2c_id = 9;

int i2c_data_count = 0;
int lcd_data_count = 0;
char i2c_buffer[lcd_size] = {0};
char lcd_buffer[lcd_size + 1] = {0};
// Signal when busy processing message
boolean i2c_lcd_busy = false; // Signal when busy processing message

void setup() {
    pinMode(status_led, OUTPUT);
    digitalWrite(status_led, LOW);
    lcd.begin(lcd_cols, lcd_rows);
    Wire.begin(i2c_id);
    Wire.onReceive(receiveEvent);
    Wire.onRequest(requestEvent);
    Serial.begin(115200);
    i2c_call = I2CLCD_NOOP;
    lcd_call = I2CLCD_NOOP;
}

void loop() {
    if (i2c_lcd_busy) {
        digitalWrite(status_led, HIGH);
        lcd_call = i2c_call;
        lcd_data_count = i2c_data_count;
        memset(lcd_buffer, 0, lcd_size + 1);
        memcpy(lcd_buffer, i2c_buffer, lcd_size);
        process_message();
        i2c_lcd_busy = false;
    } else {
        delay(1000);
        Serial.println("READY");
        digitalWrite(status_led, LOW);
    }
}

void requestEvent() {
    Wire.write(i2c_lcd_busy);
}

// function that executes whenever data is received from master
// this function is registered as an event, see setup()
void receiveEvent(int howMany) {
    i2c_lcd_busy = true;
    i2c_call = (i2clcd_e) Wire.read(); // receive command byte
    i2c_data_count = 0;
    memset(i2c_buffer, 0, lcd_size);
    while(Wire.available()) // loop through remaining data
    {
        char newData = Wire.read();
        i2c_data_count++;
        if (i2c_data_count < lcd_size)
            i2c_buffer[i2c_data_count - 1] = newData;
    }
}

void process_message() {
    switch (i2c_call) {
        case I2CLCD_NOOP:
            Serial.println("I2CLCD_NOOP");
            break;
        case I2CLCD_CLEAR:
            Serial.println("I2CLCD_CLEAR");
            lcd.clear();
            break;
        case I2CLCD_HOME:
            Serial.println("I2CLCD_HOME");
            lcd.home();
            break;
        case I2CLCD_SETCURSOR:
            Serial.print("I2CLCD_SETCURSOR ");
            Serial.print(lcd_buffer[0], DEC);
            Serial.println(lcd_buffer[1], DEC);
            lcd.setCursor(lcd_buffer[0], lcd_buffer[1]);
            break;
        case I2CLCD_WRITE:
            Serial.print("I2CLCD_WRITE ");
            Serial.println(lcd_buffer[0], DEC);
            lcd.write(lcd_buffer[0]);
            break;
        case I2CLCD_PRINT:
            Serial.print("I2CLCD_PRINT '");
            Serial.print(lcd_buffer);
            Serial.println("'");
            lcd.write(lcd_buffer);
            break;
        case I2CLCD_CURSOR:
            Serial.println("I2CLCD_CURSOR");
            lcd.cursor();
            break;
        case I2CLCD_NOCURSOR:
            Serial.println("I2CLCD_NOCURSOR");
            lcd.noCursor();
            break;
        case I2CLCD_BLINK:
            Serial.println("I2CLCD_BLINK");
            lcd.blink();
            break;
        case I2CLCD_NOBLINK:
            Serial.println("I2CLCD_NOBLINK");
            lcd.noBlink();
            break;
        case I2CLCD_DISPLAY:
            Serial.println("I2CLCD_DISPLAY");
            lcd.display();
            break;
        case I2CLCD_NODISPLAY:
            Serial.println("I2CLCD_NODISPLAY");
            lcd.noDisplay();
            break;
        case I2CLCD_SCROLLDISPLAYLEFT:
            Serial.println("I2CLCD_SCROLLDISPLAYLEFT");
            lcd.scrollDisplayLeft();
            break;
        case I2CLCD_SCROLLDISPLAYRIGHT:
            Serial.println("I2CLCD_SCROLLDISPLAYRIGHT");
            lcd.scrollDisplayRight();
            break;
        case I2CLCD_AUTOSCROLL:
            Serial.println("I2CLCD_AUTOSCROLL");
            lcd.autoscroll();
            break;
        case I2CLCD_NOAUTOSCROLL:
            Serial.println("I2CLCD_NOAUTOSCROLL");
            lcd.noAutoscroll();
            break;
        case I2CLCD_LEFTTORIGHT:
            Serial.println("I2CLCD_LEFTTORIGHT");
            lcd.leftToRight();
            break;
        case I2CLCD_RIGHTTOLEFT:
            Serial.println("I2CLCD_RIGHTTOLEFT");
            lcd.rightToLeft();
            break;
        case I2CLCD_CREATECHAR:
            Serial.print("I2CLCD_CREATECHAR #");
            Serial.println(lcd_buffer[0], DEC);
            for (int c=1; c < 9; c++)
                Serial.println(lcd_buffer[c], BIN);
            lcd.createChar(lcd_buffer[0], (uint8_t *) &lcd_buffer[1]);
            break;
        default:
            Serial.print("unknown command: ");
            Serial.println(i2c_call, DEC);
    }
}
