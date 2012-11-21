#ifndef STATEMACHINE_H
#define STATEMACHINE_H

void updateState(vacstate_t newState, unsigned long currentTime) {
    const char *stateStr;

    if (newState > VAC_ENDSTATE) {
        actionState = VAC_ENDSTATE;
    }

    PRINTMESSAGE(currentTime, message, signalStrength);
    PRINTTIME(currentTime);
    STATE2STRING(actionState);
    D("State Change: "); D(stateStr);
    STATE2STRING(newState);
    D(" -> "); D(stateStr);
    D("\n");
    actionState = newState;
    lastStateChange = currentTime;
}

void handleActionState(unsigned long currentTime) {

    // Handle overall state
    switch (actionState) {

        case VAC_LISTENING:
            // The only place lastActive is updated
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
            if (currentActive != NULL) { // some node remained active
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
            lastActive = currentActive = NULL;
            vacControl(false); // Power OFF
            if (currentTime > (lastStateChange + VACPOWERTIME)) {
                // waited long enough
                updateState(VAC_SERVOPOSTPOWERUP, currentTime);
            } // wait longer
            break;

        case VAC_SERVOPOSTPOWERUP:
            // ignore any nodes comming online, must go through VAC_SERVOSTANDBY
            lastActive = currentActive = NULL;
            servoControl(true); // Power on
            if (currentTime > (lastStateChange + SERVOPOWERTIME)) {
                // Servo powerup finished
                updateState(VAC_SERVOSTANDBY, currentTime);
            } // else wait some more
            break;

        case VAC_SERVOSTANDBY:
            // ignore any nodes comming online, must go through VAC_SERVOSTANDBY
            lastActive = currentActive = NULL;
            moveServos(0); // open all ports
            if (currentTime > (lastStateChange + SERVOMOVETIME)) {
                updateState(VAC_SERVOPOSTPOWERDN, currentTime);
            } // else wait longer
            break;

        case VAC_SERVOPOSTPOWERDN:
            // ignore any nodes comming online
            lastActive = currentActive = NULL;
            servoControl(false); // Power off
            if (currentTime > (lastStateChange + SERVOPOWERTIME)) {
                updateState(VAC_ENDSTATE, currentTime);
            } // not enough time
            break;

        case VAC_ENDSTATE:
        default:
            // ignore any nodes comming online
            lastActive = currentActive = NULL;
            // Make sure everything powered off
            servoControl(false); // Power off
            vacControl(false); // Power OFF
            updateState(VAC_LISTENING, currentTime);
            break;
    }
}

void handleLCDState(unsigned int currentTime) {

}

#endif // STATEMACHINE_H
