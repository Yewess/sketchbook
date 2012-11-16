
#ifndef GLOBALS_H
#define GLOBALS_H

/* Definitions */
#define DEBUG // undefine to turn off all serial output

// Constants used once (to save space)
#define POLLINTERVAL 25 // @ 300 baud, takes 100ms to receive 30 bytes
#define STATEINTERVAL (POLLINTERVAL+3) // update state almost as quickly
#define STATUSINTERVAL 5003 // only used for debugging
#define GOODMSGMIN 3 // Minimum number of good messages in...
#define THRESHOLD 10000 // ..10 second reception threshold, to signal start
#define MAXNODES 10 // number of nodes to keep track of
#define NODENAMEMAX 27 // name characters + 1
#define SERVOPOWERTIME 250 // ms to wait for servo's to power up/down
#define SERVOMOVETIME 1000 // ms to wait for servo's to move
#define VACPOWERTIME 2000 // ms to wait for vac to power on

/* constants */
const int statusLEDPin = 13;
const int rxDataPin = 12;
const int signalStrengthPin = 0;
const int servoPowerControlPin = 8;
const int vacPowerControlPin = 9;
const int servoCenterPW = 368;

/* globals */
message_t message;
nodeInfo_t nodeInfo[MAXNODES];
nodeInfo_t *currentActive = NULL;
nodeInfo_t *lastActive = NULL;
boolean blankMessage = true; // Signal not to read from message
unsigned long lastReception = 0; // millis() a message was last received
unsigned long lastStateChange = 0; // last time state was changed
int signalStrength = 0;
vacstate_t actionState = VAC_SERVOPOSTPOWERUP; // make double-sure all ports open
const char *statusMessage = "\0";
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

#endif // GLOBALS_H
