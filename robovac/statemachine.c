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

#include "statemachine.h"

/* functions */

void updateState(vacstate_t newState, unsigned long currentTime) {
    if (newState > VAC_ENDSTATE) {
        actionState = VAC_ENDSTATE;
    }
#ifdef DEBUG
    const char *stateStr;

    printMessage(currentTime);
    PRINTTIME(currentTime);
    STATE2STRING(actionState);
    Serial.print("State Change: "); Serial.print(stateStr);
    STATE2STRING(newState);
    Serial.print(" -> "); Serial.print(stateStr);
    Serial.println();
#endif
    actionState = newState;
    lastStateChange = currentTime;
}


void handleActionState(TimerInformation *Sender) {
    unsigned long currentTime = millis();

    // Handle overall state
    switch (actionState) {

        case VAC_LISTENING:
            lastActive = currentActive = activeNode(currentTime);
            if (currentActive != NULL) {
                updateState(VAC_VACPOWERUP, currentTime);
            }
            break;

        case VAC_VACPOWERUP:
            currentActive = activeNode(currentTime);
            if (currentActive != NULL) { // node remained active
                vacControl(true); // Power ON
                if (currentTime > (lastStateChange + VACPOWERTIME)) {
                    // powerup finished
                    updateState(VAC_SERVOPOWERUP, currentTime);
                } // else wait some more
            } else { // node shut down during power up
                updateState(VAC_VACPOWERDN, currentTime); // power down
            }
            break;

        case VAC_SERVOPOWERUP:
            currentActive = activeNode(currentTime);
            if (currentActive != NULL) { // node remained active
                servoControl(true); // Power on
                if (currentTime > (lastStateChange + SERVOPOWERTIME)) {
                    // Servo powerup finished
                    updateState(VAC_SERVOACTION, currentTime);
                } // else wait some more
            } else { // node shutdown during servo power up
                updateState(VAC_SERVOPOSTPOWERDN, currentTime);
            }
            break;

        case VAC_SERVOACTION:
            currentActive = activeNode(currentTime);
            if (currentActive != NULL) { // node remained active
                moveServos(currentActive->port_id);
                if ( (lastActive == currentActive) && // node didn't change
                     (currentTime > (lastStateChange + SERVOMOVETIME))) {
                    updateState(VAC_SERVOPOWERDN, currentTime);
                } // not enough time and/or node id changed
            } else { // node shutdown
                updateState(VAC_SERVOSTANDBY, currentTime);
            }
            break;

        case VAC_SERVOPOWERDN:
            currentActive = activeNode(currentTime);
            if (currentActive != NULL) { // node remained active
                servoControl(false); // Power off
                if ( (lastActive == currentActive) && // node didn't change
                     (currentTime > (lastStateChange + SERVOPOWERTIME))) {
                    updateState(VAC_VACUUMING, currentTime);
                } // not enough time
            } else { // node shutdown
                updateState(VAC_SERVOPOSTPOWERUP, currentTime);
            }
            break;

        case VAC_VACUUMING:
            currentActive = activeNode(currentTime);
            if (currentActive != NULL) { // node remained active
                if (lastActive != currentActive) { // node changed!
                    updateState(VAC_SERVOPOWERUP, currentTime);
                } // else keep vaccuuming
            } else { // node shutdown
                updateState(VAC_VACPOWERDN, currentTime);
            }
            break;

        case VAC_VACPOWERDN:
            // ignore any nodes comming online, must go through VAC_SERVOSTANDBY
            // before VAC_VACPOWERUP
            vacControl(false); // Power OFF
            if (currentTime > (lastStateChange + VACPOWERTIME)) {
                // waited long enough
                updateState(VAC_SERVOPOSTPOWERUP, currentTime);
            } // wait longer
            break;

        case VAC_SERVOPOSTPOWERUP:
            // ignore any nodes comming online, must go through VAC_SERVOSTANDBY
            servoControl(true); // Power on
            if (currentTime > (lastStateChange + SERVOPOWERTIME)) {
                // Servo powerup finished
                updateState(VAC_SERVOSTANDBY, currentTime);
            } // else wait some more
            break;

        case VAC_SERVOSTANDBY:
            // ignore any nodes comming online, must go through VAC_SERVOSTANDBY
            moveServos(0); // open all ports
            if (currentTime > (lastStateChange + SERVOMOVETIME)) {
                updateState(VAC_SERVOPOSTPOWERDN, currentTime);
            } // else wait longer
            break;

        case VAC_SERVOPOSTPOWERDN:
            servoControl(false); // Power off
            if (currentTime > (lastStateChange + SERVOPOWERTIME)) {
                updateState(VAC_ENDSTATE, currentTime);
            } // not enough time
            break;

        case VAC_ENDSTATE:
        default:
            // Make sure everything powered off
            servoControl(false); // Power off
            vacControl(false); // Power OFF
            updateState(VAC_LISTENING, currentTime);
            break;
    }
}
