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

/*
    Dependencies:
    ebl-arduino: http://code.google.com/p/ebl-arduino/
    virtual wire: http://www.pjrc.com/teensy/td_libs_VirtualWire.html
    Adafruit_RGBLCDShield: git://github.com/adafruit/Adafruit-RGB-LCD-Shield-Library.git
    Adafruit_PWMServoDriver: git://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library.git
*/

#define DEBUG // DEBUGGING ON
//#undef DEBUG // DEBUGGING OFF

#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>
#include <VirtualWire.h>
#include <TimedEvent.h>
#include <Adafruit_RGBLCDShield.h>
#include <Adafruit_PWMServoDriver.h>
#include <RoboVac.h>
#include "globals.h"
#include "control.h"
#include "nodeinfo.h"
#include "statemachine.h"
#include "events.h"
#include "callbacks.h"
#include "lcd.h"
#include "menu.h"

/* Main Program */

void setup() {

    lcd.setBacklight(0x0); // OFF

    Serial.begin(SERIALBAUD);
    D("setup()\n");

    pinMode(rxDataPin, INPUT);
    vw_set_rx_pin(rxDataPin);
    pinMode(signalStrengthPin, INPUT);
    vw_set_ptt_pin(statusLEDPin);
    pinMode(statusLEDPin, OUTPUT);
    digitalWrite(statusLEDPin, LOW);
    vw_setup(RXTXBAUD);
    vw_rx_start();

    // Setup events
    TimedEvent.addTimer(POLLINTERVAL, pollRxEvent);
    TimedEvent.addTimer(STATEINTERVAL, robovacStateEvent);
    TimedEvent.addTimer(STATUSINTERVAL, statusEvent);
    TimedEvent.addTimer(LCDINTERVAL, lcdEvent);

    // Setup state machine
    lastStateChange = millis();

    // Initialize array
    setupNodeInfo();

    // Load stored data
    readNodeIDServoMap();
#ifdef DEBUG
    for (byte n=0; n<MAXNODES; n++) {
        nodeInfo[n].node_id = n;
        nodeInfo[n].port_id = n+1;
        nodeInfo[n].servo_min = 300 - n;
        nodeInfo[n].servo_max = 400 + n;
        strcpy(nodeInfo[n].node_name, "Test Node  ");
        nodeInfo[n].node_name[10] = n + 48;
    }

    nodeInfo[1].node_id = 7;
    nodeInfo[1].port_id = 3;
    nodeInfo[1].servo_min = 300;
    nodeInfo[1].servo_max = 400;
    strcpy(nodeInfo[1].node_name, "Test Node 7");
#endif // DEBUG
    // Debugging info.
    printNodes();

    // set up the LCD's number of columns and rows
    // and pwm servo board
    lcd.begin(lcdCols, lcdRows);
    lcd.setBacklight(0x1); // ON
    pwm.begin();
    pwm.setPWMFreq(60);

    // Setup menu
    menuSetup();

    D("\nloop()\n");
}

void loop() {
    TimedEvent.loop(); // blocking
}
