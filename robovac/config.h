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
#include <RoboVac.h>

/* Definitions */
#define DEBUG // undefine to turn off all serial output

// Constants used once (to save space)
#define POLLINTERVAL 25 // @ 300 baud, takes 100ms to receive 30 bytes
#define STATEINTERVAL (POLLINTERVAL+3) // update state almost as quickly
#define STATUSINTERVAL 5003 // only used for debugging
#define GOODMSGMIN 3 // Minimum number of good messages in...
#define THRESHOLD 10000 // ..10 second reception threshold, to signal start
#define MAXNODES 10 // number of nodes to keep track of
#define SERVOPOWERTIME 250 // ms to wait for servo's to power up/down
#define SERVOMOVETIME 1000 // ms to wait for servo's to move
#define VACPOWERTIME 2000 // ms to wait for vac to power on

/* constants */
const int statusLEDPin = 13;
const int rxDataPin = 12;
const int signalStrengthPin = A0;
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
