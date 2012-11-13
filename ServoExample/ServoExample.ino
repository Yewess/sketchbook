/*************************************************** 
  This is an example for our Adafruit 16-channel PWM & Servo driver
  Servo test - this will drive 16 servos, one after the other

  Pick one up today in the adafruit shop!
  ------> http://www.adafruit.com/products/815

  These displays use I2C to communicate, 2 pins are required to  
  interface. For Arduino UNOs, thats SCL -> Analog 5, SDA -> Analog 4

  Adafruit invests time and resources providing this open source code, 
  please support Adafruit and open-source hardware by purchasing 
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.  
  BSD license, all text above must be included in any redistribution
 ****************************************************/

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <Adafruit_MCP23017.h>
#include <Adafruit_RGBLCDShield.h>

// These #defines make it easy to set the backlight color
#define ON 0x1
#define OFF 0x0

Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

// called this way, it uses the default address 0x40
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
// you can also call it with a different address you want
//Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x41);

// Depending on your servo make, the pulse width min and max may vary, you 
// want these to be as small/large as possible without hitting the hard stop
// for max range. You'll have to tweak them as necessary to match the servos you
// have!
#define SERVOMIN  150 // this is the 'minimum' pulse length count (out of 4096)
#define SERVOMAX  600 // this is the 'maximum' pulse length count (out of 4096)

// our servo # counter
uint8_t servonum = 0;

int servomin = SERVOMIN;
int servomax = SERVOMAX;
boolean changingMax = true;
boolean servomoved = false;
uint8_t buttons = 0;

void setup() {
  Serial.begin(9600);

  pwm.begin();
  
  pwm.setPWMFreq(60);  // Analog servos run at ~60 Hz updates
  
  lcd.begin(16, 2);
  
  lcd.setBacklight(ON);
  //lcd.autoscroll();
  lcd.clear();
  lcd.setCursor(0,0);  
  lcd.print("#: 0 Min: "); lcd.print(servomin);
  lcd.setCursor(0,1);
  lcd.print(" [Max]: "); lcd.print(servomax);
}

void loop() {
  buttons = lcd.readButtons();
  
  if (buttons) {
    lcd.clear();
    lcd.setCursor(0,0);
    servomoved = false;

    if (buttons & BUTTON_SELECT) {
        changingMax = !changingMax;
    }    

    if ((buttons & BUTTON_RIGHT) && (servonum < 10)) {
        servonum++;
    }
    
    if ((buttons & BUTTON_LEFT) && (servonum > 0)) {
        servonum--;
    }

    lcd.print("#: "); lcd.print(servonum);

    if (buttons & BUTTON_UP) {
        if (changingMax) {
            servomax++;
        } else {
            servomin++;
        }
    }
    if (buttons & BUTTON_DOWN) {
        if (changingMax) {
            servomax--;
        } else {
            servomin--;
        }
    }

    if (changingMax) {
        lcd.print(" Min: ");
    } else {
        lcd.print(" [Min]: ");
    }
    lcd.print(servomin);
    lcd.setCursor(0,1);
    if (!changingMax) {
        lcd.print(" Max: ");
    } else {
        lcd.print(" [Max]: ");
    }
    lcd.print(servomax);
  }
  
  // Drive each servo one Time
  if (!servomoved) {
     servomoved = true;
     Serial.print("Servo "); Serial.print(servonum);
     if (!changingMax) {
       Serial.print(" *");
     } else {
       Serial.print(" ");
     }
     Serial.print(servomin);
     if (changingMax) {
       Serial.print(" - *");
       pwm.setPWM(servonum, 0, servomax);       
     } else {
       Serial.print(" - ");
       pwm.setPWM(servonum, 0, servomin);       
     }
     Serial.println(servomax);
   }
}
