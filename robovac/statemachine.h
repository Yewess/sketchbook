#ifndef STATEMACHINE_H
#define STATEMACHINE_H

#include <RoboVac.h>
#include "globals.h"
#include "lcd.h"
#include "menu.h"
#include "control.h"

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

    // monitorMode == false

    // Handle overall state
    switch (actionState) {

        case VAC_LISTENING:
            // The only place lastActive is updated
            lastActive = currentActive = activeNode(currentTime, false);;
            if (currentActive != NULL) {
                updateState(VAC_VACPOWERUP, currentTime);
            }
            break;

        case VAC_VACPOWERUP:
            currentActive = activeNode(currentTime, false);
            if (currentActive != NULL) { // node change doesn't matter
                vacControl(true); // Power ON
                if (timerExpired(currentTime, &lastStateChange, VACPOWERTIME)) {
                    // powerup finished
                    updateState(VAC_SERVOPOWERUP, currentTime);
                } // else wait some more
            } else if (currentActive == NULL) { // node shut down during power up
                updateState(VAC_VACPOWERDN, currentTime); // power down
            }
            break;

        case VAC_SERVOPOWERUP:
            currentActive = activeNode(currentTime, false);
            if (currentActive != NULL) { // node change doesn't matter
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
            currentActive = activeNode(currentTime, false);
            if (currentActive != NULL) { // node remained active
                moveServos(currentActive->port_id); // work on currentActive
                if (lastActive != currentActive) { // Node changed
                    lastActive = currentActive; // Wait another cycle
                    lastStateChange = *currentTime;
                } else { // no node change
                    if (timerExpired(currentTime, &lastStateChange, SERVOMOVETIME)) {
                        updateState(VAC_SERVOPOWERDN, currentTime);
                    } // else wait more
                }
            } else { // node shutdown during servo move, go back to standby
                updateState(VAC_SERVOSTANDBY, currentTime);
            }
            break;

        case VAC_SERVOPOWERDN:
            currentActive = activeNode(currentTime, false);
            if (currentActive != NULL) { // node remained active
                if (lastActive != currentActive) { // node change
                    lastActive = currentActive;
                    updateState(VAC_SERVOACTION, currentTime); // Re-move servos
                } else { // node did not change
                    servoControl(false); // Power off servos
                    updateState(VAC_VACUUMING, currentTime);
                }
            } else { // node shutdown
                updateState(VAC_SERVOSTANDBY, currentTime);
            }
            break;

        case VAC_VACUUMING:
            currentActive = activeNode(currentTime, false);
            if (currentActive != NULL) {
                if (lastActive != currentActive) { // node changed!
                    lastActive = currentActive;
                    updateState(VAC_SERVOPOWERUP, currentTime); // move servos
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
            updateState(VAC_SERVOPOSTPOWERUP, currentTime);
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

    // Service callback or button presses
    handleCallbackButtons(currentTime);

    switch (lcdState) {

        case LCD_ACTIVEWAIT:
            if (lcdButtons) {
                updateLCDState(LCD_INMENU, currentTime);
                lcdButtons = 0;
            } else {
                if ((monitorMode == false) && (currentActive != NULL)) { // A node went active
                    updateLCDState(LCD_RUNNING, currentTime);
                } else if (timerExpired(currentTime, &lastButtonChange, LCDSLEEPTIME)) {
                    updateLCDState(LCD_SLEEPWAIT, currentTime);
                } // else wait more
            }
            break;

        case LCD_SLEEPWAIT:
            lcd.setBacklight(0); // OFF
            if (lcdButtons) {
                lcd.setBacklight(0x1); // ON
                lcdButtons = 0;
                updateLCDState(LCD_INMENU, currentTime);
            } else if (currentActive != NULL) {
                lcd.setBacklight(0x1); // ON
                updateLCDState(LCD_RUNNING, currentTime);
            } else {
                //D("Zzz ");
            }
            break;

        case LCD_INMENU:
            // Prevents transition to LCD_RUNNING
            if (lcdButtons) {
                lcdButtons = 0;
            } else if (timerExpired(currentTime, &lastButtonChange, LCDMENUTIME)) {
                if (monitorMode == false) {
                    if (currentActive != NULL) { // VACUUMING
                        updateLCDState(LCD_RUNNING, currentTime);
                    } else { // Not Vacuuming
                        updateLCDState(LCD_ACTIVEWAIT, currentTime);
                    }
                } // in Monitor Mode
            } // still navigating menu
            break;

        case LCD_RUNNING:
            if (lcdButtons) {
                lcdButtons = 0;
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
            lcdButtons = 0;
            currentCallback = NULL;
            updateLCDState(LCD_ACTIVEWAIT, currentTime);
            drawMenu();
            break;
    }
}

#endif // STATEMACHINE_H
