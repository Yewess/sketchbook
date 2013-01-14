
#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
/* Definitions */

// Constants used once (to save space)
#define POLLINTERVAL (25) // @ 300 baud, takes 100ms to receive 30 bytes
#define STATEINTERVAL (POLLINTERVAL+3) // update state almost as quickly
#define LCDINTERVAL (9) // Must poll LCD buttons fast
#define STATUSINTERVAL (3015) // DEBUGGING and LCD updates
#define BUTTONCHANGE (300) // minimum time between button changes
#define GOODMSGMIN (2) // Minimum number of good messages in...
#define THRESHOLD (6000) // ..6 second reception threshold, to signal start
#define CHARLOWER (32) // lower limit for node id name ASCII character
#define CHARUPPER (125) // upper limit for node id name ASCII characters
#define PWMFREQ (60.0) // servo PWM frequency to use (60Hz == 16ms)
#define USPERBIT ((1000000.0 / PWMFREQ) / 4096.0) // uS per bit for 12bit res @ PWMFREQ
#define PWMMAX (2000.0 / USPERBIT) // maximum bits for 2ms long pulse
#define PWMMIN (1000.0 / USPERBIT) // minimum bits for 1ms long pulse
#define PWMCENTER (PWMMIN + ((PWMMAX-PWMMIN) / 2.0))

/* constants */
const int statusLEDPin = 13;
const int rxDataPin = 12;
const int signalStrengthPin = 0;
const int vacPowerControlPin = 2;
const int servoCenterPW = int(PWMCENTER);
const int servoMinPW = int(PWMMIN);
const int servoMaxPW = int(PWMMAX);
const int lcdDArrow = 4; // Down arrow character ID
const int lcdUArrow = 5; // Up arrow character ID
const int lcdRArrow = 126; //  right arrow character ID
const int lcdLArrow = 127; // left arrow character ID


/* globals */

// node state
nodeInfo_t nodeInfo[MAXNODES];
nodeInfo_t *currentActive = NULL;
nodeInfo_t *lastActive = NULL;

// message state
message_t message;
unsigned long lastStateChange = 0; // last time state was changed
int signalStrength = 0;

// System State
vacstate_t actionState = VAC_SERVOPOSTPOWERUP; // make double-sure all ports open
boolean monitorMode = false; // don't actually move servos / switch vac
vacPower_t vacpower;
unsigned long lastOnTime=0;
unsigned long lastOffTime=0;

// UI State
char lcdBuf[lcdRows][lcdCols+1] = {'\0'};
char runningBuf[lcdRows][lcdCols+1] = {'\0'}; // used for drawRunning
lcdState_t lcdState;
uint8_t lcdButtons; // Buttons pressed during last lcdEvent()
unsigned long lastButtonChange; // last time button state changed
menuEntry_t *currentMenu; // Current entry point in menu
menuEntryCallback_t currentCallback = NULL; // address of currently running callback
const char lcdPortIDLine[] = "Port# xx ID# xx ";
const char lcdRangeLine[] =  "Lxxxx    Hxxxx  ";
uint8_t downArrowChar[] = {B00000,
                           B00000,
                           B00100,
                           B00100,
                           B10101,
                           B01110,
                           B00100,
                           B00000};

uint8_t UpArrowChar[] = {B00000,
                         B00100,
                         B01110,
                         B10101,
                         B00100,
                         B00100,
                         B00000,
                         B00000};

// I2C Device init.
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

#endif // GLOBALS_H
