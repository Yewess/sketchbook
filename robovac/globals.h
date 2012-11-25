
#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
/* Definitions */

// Constants used once (to save space)
#define POLLINTERVAL 25 // @ 300 baud, takes 100ms to receive 30 bytes
#define STATEINTERVAL (POLLINTERVAL+3) // update state almost as quickly
#define LCDINTERVAL (STATEINTERVAL+14) // UI can be a bit slower
#define STATUSINTERVAL 1015 // mainly for debugging / turning LED off
#define GOODMSGMIN 3 // Minimum number of good messages in...
#define THRESHOLD 10000 // ..10 second reception threshold, to signal start

/* constants */
const int statusLEDPin = 13;
const int rxDataPin = 12;
const int signalStrengthPin = 0;
const int servoPowerControlPin = 8;
const int vacPowerControlPin = 9;
const int servoCenterPW = 368;

/* globals */

// node state
nodeInfo_t nodeInfo[MAXNODES];
nodeInfo_t *currentActive = NULL;
nodeInfo_t *lastActive = NULL;

// message state
message_t message;
boolean blankMessage = true; // Signal not to read from message
unsigned long lastReception = 0; // millis() since a message was last received
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

// I2C Device init.
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

#endif // GLOBALS_H
