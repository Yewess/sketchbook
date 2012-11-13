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

    Pitfalls: virtual wire uses Timer1 (can't use PWM on pin 9 & 10
*/

#include <EEPROM.h>
#include <TimedEvent.h>
#include <VirtualWire.h>
#include <RoboVac.h>
#include "config.h"
#include "nodeinfo.h"
#include "statemachine.h"
#include "control.h"
#include "events.h"

/* Main Program */

void setup() {

    // debugging info
#ifdef DEBUG
    Serial.begin(SERIALBAUD);
    Serial.println("setup()");
#endif // DEBUG

    // Initialize array
    setupNodeInfo();
    // Load stored map data
    readNodeIDServoMap();
#ifdef DEBUG
    nodeInfo[2].node_id = 7;
    nodeInfo[2].port_id = 42;
    strcpy(nodeInfo[2].node_name, "TEST NODE 7");
    nodeInfo[4].node_id = 9;
    nodeInfo[4].port_id = 44;
    strcpy(nodeInfo[4].node_name, "TEST NODE 10");
#endif // DEBUG

    // Make sure content matches
    writeNodeIDServoMap();
    printNodeInfo();

    pinMode(rxDataPin, INPUT);
    pinMode(signalStrengthPin, INPUT);
    vw_set_rx_pin(rxDataPin);
    vw_set_ptt_pin(statusLEDPin);
    pinMode(statusLEDPin, OUTPUT);
    digitalWrite(statusLEDPin, LOW);
    vw_setup(RXTXBAUD);
    vw_rx_start();

    // Setup events
    TimedEvent.addTimer(POLLINTERVAL, pollRxEvent);
    TimedEvent.addTimer(STATEINTERVAL, handleActionState);
    TimedEvent.addTimer(STATUSINTERVAL, printStatusEvent);

    // Setup state machine
    lastStateChange = millis();

    // debugging stuff
#ifdef DEBUG
    Serial.print("rxDataPin: "); Serial.print(rxDataPin);
    Serial.print("  statusLEDPin: "); Serial.print(statusLEDPin);
    Serial.print("  SERIALBAUD: "); Serial.print(SERIALBAUD);
    Serial.print("  RXTXBAUD: "); Serial.print(RXTXBAUD);
    Serial.print("\nStat. Int.: "); Serial.print(STATUSINTERVAL);
    Serial.print("ms  Poll Int.: "); Serial.print(POLLINTERVAL);
    Serial.println("ms"); Serial.println("loop()");
#endif // DEBUG
}

void loop() {
    TimedEvent.loop(); // blocking
}
