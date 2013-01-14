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
        // Paint runningBuf
        clearRunningBuf();
        memcpy(runningBuf[0], &(stateStr[4]), strlen(stateStr) - 4);
        if (currentActive) {
            memcpy(runningBuf[1],
                   currentActive->node_name, NODENAMEMAX-1 );
        }
        actionState = newState;
        lastStateChange = *currentTime;
    }
}

void handleActionState(unsigned long *currentTime) {

/*
    VAC_LISTENING, // Waiting for Signal
    VAC_VACPOWERUP, // Powering up vacuum
    VAC_SERVOPOWERUP, // Powering up servos
    VAC_SERVOACTION, // Moving Servos
    VAC_SERVOPOWERDN, // Powering down servos
    VAC_VACUUMING, // Waiting for down threshold
    VAC_SERVOPOSTPOWERUP, // Powering up servos again
    VAC_SERVOSTANDBY,     // open all ports
    VAC_SERVOPOSTPOWERDN, // Powering down servos again
    VAC_VACPOWERDN, // Powering down vacuum
    VAC_COOLDOWN // mandatory cooldown period
    VAC_ENDSTATE, // Return to LISTENING
*/

    // Handle overall state
    switch (actionState) {

        case VAC_LISTENING:
            lastActive = currentActive = activeNode(currentTime, false);;
            if (currentActive != NULL) {
                updateState(VAC_VACPOWERUP, currentTime);
            }
            break;

        case VAC_VACPOWERUP:
            vacControl(true, false); // Power ON
            lastOnTime = *currentTime;
            if (timerExpired(currentTime, &lastStateChange, VACPOWERTIME)) {
                // Vac powerup finished
                updateState(VAC_SERVOPOWERUP, currentTime);
            } // else wait more
            break;

        case VAC_SERVOPOWERUP:
            servoControl(true); // Power on
            if (timerExpired(currentTime, &lastStateChange, SERVOPOWERTIME)) {
                // Servo powerup finished
                updateState(VAC_SERVOACTION, currentTime);
            } // else wait some more
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
            } else { // node shutdown during servo move, go to standby
                updateState(VAC_SERVOSTANDBY, currentTime);
            }
            break;

        case VAC_SERVOPOWERDN:
            servoControl(false); // Power off servos
            if (timerExpired(currentTime, &lastStateChange, SERVOPOWERTIME)) {
                // Servo powerdown finished
                updateState(VAC_VACUUMING, currentTime);
            } // else wait some more
            break;

        case VAC_VACUUMING:
            currentActive = activeNode(currentTime, false);
            if (currentActive != NULL) {
                if (lastActive != currentActive) { // node changed!
                    lastActive = currentActive;
                    updateState(VAC_SERVOPOWERUP, currentTime); // move servos
                } // else keep vaccuuming
            } else { // All nodes shutdown, open all ports
                updateState(VAC_SERVOPOSTPOWERUP, currentTime);
            }
            break;

        case VAC_SERVOPOSTPOWERUP:
            servoControl(true); // Power on
            if (timerExpired(currentTime, &lastStateChange, SERVOPOWERTIME)) {
                // Servo powerup finished
                updateState(VAC_SERVOSTANDBY, currentTime);
            } // else wait some more
            break;

        case VAC_SERVOSTANDBY:
            moveServos(OPENALLPORT); // open all ports
            if (timerExpired(currentTime, &lastStateChange, SERVOMOVETIME)) {
                updateState(VAC_SERVOPOSTPOWERDN, currentTime);
            } // else wait longer
            break;

        case VAC_SERVOPOSTPOWERDN:
            servoControl(false); // Power off
            if (timerExpired(currentTime, &lastStateChange, SERVOPOWERTIME)) {
                updateState(VAC_VACPOWERDN, currentTime);
            } // not enough time
            break;

        case VAC_VACPOWERDN:
            vacControl(false, true); // Power OFF
            // don't allow restart until VACPOWERTIME of spin-down happens
            if (timerExpired(currentTime, &lastStateChange, VACDOWNTIME)) {
                lastOffTime = *currentTime;
                lastOnTime = 0;
                updateState(VAC_COOLDOWN, currentTime);
            } // else wait more
            break;


        case VAC_COOLDOWN:
            // Must allow cooldown, ignore nodes
            if (timerExpired(currentTime, &lastOffTime, vacpower.VacOffTime)) {
                lastOffTime = 0;
                updateState(VAC_ENDSTATE, currentTime);
            } // else wait more
            break;

        case VAC_ENDSTATE:
        default:
            // ignore any nodes comming online
            lastActive = currentActive = NULL;
            // Make sure everything powered off
            servoControl(false); // Power off
            vacControl(false, true); // Power OFF
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
            if (currentActive != NULL) {
                updateLCDState(LCD_RUNNING, currentTime);
            } else if (lcdButtons) {
                lcd.setBacklight(1); // ON
                updateLCDState(LCD_INMENU, currentTime);
            }
            break;

        case LCD_INMENU:
            // Prevents transition to LCD_RUNNING
            if (timerExpired(currentTime, &lastButtonChange, LCDMENUTIME)) {
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
            lcd.setBacklight(0x1); // ON
            if (lcdButtons) {
                updateLCDState(LCD_INMENU, currentTime);
            } else if (lastActive != NULL) { // is/was vacuuming
                    drawRunning();
            } else { // Not Vacuuming
                updateLCDState(LCD_ACTIVEWAIT, currentTime);
                drawMenu();
            }
            break;

        case LCD_ENDSTATE:
        default:
            monitorMode = false;
            currentCallback = NULL;
            updateLCDState(LCD_ACTIVEWAIT, currentTime);
            drawMenu();
            break;
    }
}

#endif // STATEMACHINE_H
