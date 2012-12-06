
#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
/* Definitions */

// Constants used once (to save space)
#define POLLINTERVAL 25 // @ 300 baud, takes 100ms to receive 30 bytes
#define STATEINTERVAL (POLLINTERVAL+3) // update state almost as quickly
#define LCDINTERVAL (STATEINTERVAL+14) // UI can be a bit slower
#define STATUSINTERVAL 1015 // DEBUGGING and LCD updates
#define BUTTONCHANGE 100 // minimum time between button changes
#define GOODMSGMIN 2 // Minimum number of good messages in...
#define THRESHOLD 6000 // ..6 second reception threshold, to signal start
#define CHARLOWER 32 // lower limit for node id name ASCII character
#define CHARUPPER 125 // upper limit for node id name ASCII characters

/* constants */
const int statusLEDPin = 13;
const int rxDataPin = 12;
const int signalStrengthPin = 0;
const int servoPowerControlPin = 7;
const int vacPowerControlPin = 2;
const int servoCenterPW = 368;

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

// UI State
char lcdBuf[lcdRows][lcdCols+1] = {'\0'};
lcdState_t lcdState;
uint8_t lcdButtons; // Buttons pressed during last lcdEvent()
unsigned long lastButtonChange; // last time button state changed
menuEntry_t *currentMenu; // Current entry point in menu
menuEntryCallback_t currentCallback = NULL; // address of currently running callback
const char lcdPortIDLine[] = "Port# xx ID# xx ";
uint8_t downArrowChar[] = {B00000,
                           B00000,
                           B00100,
                           B00100,
                           B10101,
                           B01110,
                           B00100,
                           B00000};

// I2C Device init.
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

#endif // GLOBALS_H
