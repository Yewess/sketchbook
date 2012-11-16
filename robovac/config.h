/*
  Vacuum power and hose selection controller

    Copyright (C) 2012 Chris Evich <chris-arduino@anonomail.me>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Adafruit_RGBLCDShield>
#include <Adafruit_PWMServoDriver>
#include <RoboVac.h>
#include "control.h"
#include "events.h"
#include "lcd.h"
#include "nodeinfo.h"
#include "statemachine.h"

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

/* Types */
typedef struct nodeInfo_s {
    byte node_id; // node ID
    byte port_id; // servo node_id is mapped to
    word servo_min; // minimum limit of travel in 4096ths of 60hz PWM(?)
    word servo_max; // maximum limit of travel
    unsigned char receive_count; // number of messages received in THRESHOLD
    unsigned char new_count; // messages just received
    unsigned long last_heard; // timestamp last message was received
    char node_name[NODENAMEMAX]; // name of the node
} nodeInfo_t;

typedef enum vacstate_e {
    VAC_LISTENING, // Waiting for Signal
    VAC_VACPOWERUP, // Powering up vacuum
    VAC_SERVOPOWERUP, // Powering up servos
    VAC_SERVOACTION, // Moving Servos
    VAC_SERVOPOWERDN, // Powering down servos
    VAC_VACUUMING, // Waiting for down threshold
    VAC_VACPOWERDN, // Powering down vacuum
    VAC_SERVOPOSTPOWERUP, // Powering up servos again
    VAC_SERVOSTANDBY,     // open all ports
    VAC_SERVOPOSTPOWERDN, // Powering down servos again
    VAC_ENDSTATE, // Return to listening
} vacstate_t;

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

#endif // CONFIG_H
