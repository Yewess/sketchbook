#ifndef STATEMACHINE_H
#define STATEMACHINE_H

#include "lcd.h"
#include "menu.h"

void updateState(vacstate_t newState, unsigned long *currentTime) {
    const char *stateStr;

    if (newState > VAC_ENDSTATE) {
        newState = VAC_ENDSTATE;
    }

    if (newState != actionState) {
        PRINTTIME(*currentTime);
        STATE2STRING(stateStr, actionState);
        D("State: "); D(stateStr);
        STATE2STRING(stateStr, newState);
        D(" -> "); D(stateStr);
        D("\n");
        actionState = newState;
        lastStateChange = *currentTime;
    }
}

void handleActionState(unsigned long *currentTime) {

    // Handle overall state
    switch (actionState) {

        case VAC_LISTENING:
            // The only place lastActive is updated
            lastActive = currentActive = activeNode(currentTime);;
            if (currentActive != NULL) {
                updateState(VAC_VACPOWERUP, currentTime);
            }
            break;

        case VAC_VACPOWERUP:
            currentActive = activeNode(currentTime);
            if (currentActive != NULL) { // node remained active
                vacControl(true); // Power ON
                if (timerExpired(currentTime, &lastStateChange, VACPOWERTIME)) {
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
                if (timerExpired(currentTime, &lastStateChange, SERVOPOWERTIME)) {
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
                     timerExpired(currentTime, &lastStateChange, SERVOMOVETIME)) {
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
                     timerExpired(currentTime, &lastStateChange, SERVOPOWERTIME)) {
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
            if (timerExpired(currentTime, &lastStateChange, VACPOWERTIME)) {
                // waited long enough
                updateState(VAC_SERVOPOSTPOWERUP, currentTime);
            } // wait longer
            break;

        case VAC_SERVOPOSTPOWERUP:
            // ignore any nodes comming online, must go through VAC_SERVOSTANDBY
            lastActive = currentActive = NULL;
            servoControl(true); // Power on
            if (timerExpired(currentTime, &lastStateChange, SERVOPOWERTIME)) {
                // Servo powerup finished
                updateState(VAC_SERVOSTANDBY, currentTime);
            } // else wait some more
            break;

        case VAC_SERVOSTANDBY:
            // ignore any nodes comming online, must go through VAC_SERVOSTANDBY
            lastActive = currentActive = NULL;
            moveServos(0); // open all ports
            if (timerExpired(currentTime, &lastStateChange, SERVOMOVETIME)) {
                updateState(VAC_SERVOPOSTPOWERDN, currentTime);
            } // else wait longer
            break;

        case VAC_SERVOPOSTPOWERDN:
            // ignore any nodes comming online
            lastActive = currentActive = NULL;
            servoControl(false); // Power off
            if (timerExpired(currentTime, &lastStateChange, SERVOPOWERTIME)) {
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

void handleButtonPress(unsigned long *currentTime) {
    if (lcdButtons & BUTTON_SELECT) {
        lcdButtons = 0;
        menuSelect(currentTime);
    } else if (lcdButtons & BUTTON_LEFT) {
        lcdButtons = 0;
        menuLeft(currentTime);
    } else if (lcdButtons & BUTTON_UP) {
        lcdButtons = 0;
        menuUp(currentTime);
    } else if (lcdButtons & BUTTON_RIGHT) {
        lcdButtons = 0;
        menuRight(currentTime);
    } else if (lcdButtons & BUTTON_DOWN) {
        lcdButtons = 0;
        menuDown(currentTime);
    }
}

void updateLCDState(lcdState_t newState, unsigned long *currentTime) {
    const char *stateStr;

    if (newState > LCD_ENDSTATE) {
        newState = LCD_ENDSTATE;
    }

    if (newState != lcdState) {
        PRINTTIME(*currentTime);
        LCDSTATE2STRING(stateStr,lcdState);
        D("State: "); D(stateStr);
        LCDSTATE2STRING(stateStr,newState);
        D(" -> "); D(stateStr);
        D("\n");
        lcdState = newState;
    }
}

void handleLCDState(unsigned long *currentTime) {

    // Give some runtime to callback (if any)
    handleCallback(currentTime);

    switch (lcdState) {

        case LCD_ACTIVEWAIT:
            if (lcdButtons) {
                monitorMode = false;
                updateLCDState(LCD_INMENU, currentTime);
                handleButtonPress(currentTime);
            } else {
                if (currentActive != NULL) {
                    updateLCDState(LCD_RUNNING, currentTime);
                } else if (timerExpired(currentTime, &lastButtonChange, LCDSLEEPTIME)) {
                    updateLCDState(LCD_SLEEPWAIT, currentTime);
                } // else wait more
            }
            break;

        case LCD_SLEEPWAIT:
            monitorMode = false;
            lcd.setBacklight(0); // OFF
            if (lcdButtons) {
                lcd.setBacklight(0x1); // ON
                lcdButtons = 0; // throw buttons away
                updateLCDState(LCD_INMENU, currentTime);
            } else if (currentActive != NULL) {
                lcd.setBacklight(0x1); // ON
                updateLCDState(LCD_RUNNING, currentTime);
            } else {
                //D("Zzz ");
            }
            break;

        case LCD_INMENU:
            if (lcdButtons) {
                handleButtonPress(currentTime);
            } else if (timerExpired(currentTime, &lastButtonChange, LCDMENUTIME)) {
                monitorMode = false;
                if (currentActive != NULL) { // VACUUMING
                    updateLCDState(LCD_RUNNING, currentTime);
                } else { // Not Vacuuming
                    updateLCDState(LCD_ACTIVEWAIT, currentTime);
                }
            }
            break;

        case LCD_RUNNING:
            if (lcdButtons) {
                lcdButtons = 0; // throw buttons away
                updateLCDState(LCD_INMENU, currentTime);
            } else if (currentActive != NULL) { // VACUUMING
                drawRunning(currentTime, currentActive->node_name);
            } else { // Not Vacuuming
                updateLCDState(LCD_ACTIVEWAIT, currentTime);
            }
            break;

        case LCD_ENDSTATE:
        default:
            monitorMode = false;
            updateLCDState(LCD_ACTIVEWAIT, currentTime);
            drawMenu();
            break;
    }
}

#endif // STATEMACHINE_H
