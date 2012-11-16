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

#include "control.h"

void servoControl(boolean turnOn) {
    if (turnOn == true) {
        digitalWrite(servoPowerControlPin, HIGH);
    } else {
        digitalWrite(servoPowerControlPin, LOW);
    }
}

void vacControl(boolean turnOn) {
    if (turnOn == true) {
        digitalWrite(vacPowerControlPin, HIGH);
    } else {
        digitalWrite(servoPowerControlPin, LOW);
    }
}

void moveServos(unsigned char port_id) {
    // will be called repeatidly until SERVOMOVETIME expires
    // port_id could change on any call
    int nodeCount=0;

#ifdef DEBUG
        PRINTTIME(millis());
#endif // DEBUG
    if (port_id == 0) {
#ifdef DEBUG
        Serial.print("Opening all port doors");
#endif // DEBUG
    } else { // Close all ports except port_id
        for (nodeCount=0; nodeCount < MAXNODES; nodeCount++) {
            if (nodeInfo[nodeCount].port_id != port_id) {
#ifdef DEBUG
                Serial.print("Closing node_id ");
                Serial.print(nodeInfo[nodeCount].node_id);
                Serial.print("'s port ");
                Serial.print(nodeInfo[nodeCount].port_id);
#endif // DEBUG
            } else {
#ifdef DEBUG
                Serial.print("Leaving node_id ");
                Serial.print(nodeInfo[nodeCount].node_id);
                Serial.print("'s port ");
                Serial.print(nodeInfo[nodeCount].port_id);
                Serial.print(" open!!!!!!!!");
#endif // DEBUG
            }
        }
    }
#ifdef DEBUG
    Serial.println();
#endif // DEBUG
}

