#ifndef CALLBACKS_H
#define CALLBACKS_H

#include "globals.h"
#include "control.h"

// menu items defined in menu.h
extern menuEntry_t m_setup;

boolean blinkBacklight(unsigned long *currentTime) {
    if (timerExpired(currentTime, &lastButtonChange, LCDBLINKTIME)) {
        D("Light\n");
        lcd.setBacklight(0x1); // ON
        return true;
    } else {
        D("Dark\n");
        lcd.setBacklight(0x0); // OFF
        return false;
    }
}

// has to be here b/c dependencies
void handleButtons(unsigned long *currentTime) {

    // always turn the light on for buttons
    if (lcdButtons) {
        lcd.setBacklight(0x1); // ON
    }

    // does not run if in another callback
    if (lcdButtons & BUTTON_SELECT) {
        if (currentMenu->child) {
            currentMenu = currentMenu->child;
            drawMenu();
        } else if (currentMenu->callback) {
            currentCallback = currentMenu->callback;
            lcdButtons = 0; // Don't process in callback also
        } else { // Nav. Error Blink
            currentCallback = blinkBacklight;
        }
    } else if (lcdButtons & BUTTON_LEFT) {
        // Menu is at most one-level deep, just go back to main
        currentMenu = &m_setup;
        drawMenu();
    } else if (lcdButtons & BUTTON_UP) {
        if (currentMenu->prev_sibling) {
            currentMenu = currentMenu->prev_sibling;
            drawMenu();
        } else { // Nav. Error Blink
            currentCallback = blinkBacklight;
        }
    } else if (lcdButtons & BUTTON_RIGHT) {
        if (currentMenu->child) {
            currentMenu = currentMenu->child;
            drawMenu();
        } else if (currentMenu->callback) {
            currentCallback = currentMenu->callback;
            lcdButtons = 0; // Don't process in callback also
        } else { // Nav. Error Blink
            currentCallback = blinkBacklight;
        }
    } else if (lcdButtons & BUTTON_DOWN) {
        if (currentMenu->next_sibling) {
            currentMenu = currentMenu->next_sibling;
            drawMenu();
        } else { // Nav. Error Blink
            currentCallback = blinkBacklight;
        }
    }
}

void handleCallbackButtons(unsigned long *currentTime) {
    // Enter callback if any set
    if (currentCallback != NULL) {
        if ( (*currentCallback)(currentTime) == true) {
            currentCallback = NULL;
            drawMenu();
        }
    } else {
        handleButtons(currentTime);
        // handleLCDState clears buttons
    }
}

boolean monitorCallback(unsigned long *currentTime) {
    static unsigned long lastPaint=0;
    nodeInfo_t *node=NULL;

    if (lcdButtons) { // any key pressed
        // reset all node counters
        updateNodes(NULL);
        lcdButtons = 0;
        monitorMode = false;
        return true;
    }
    monitorMode = true; // Don't turn on anything
    // update lcdBuf & print every STATUSINTERVAL
    if (timerExpired(currentTime, &lastPaint, STATUSINTERVAL)) {
        lastPaint = *currentTime;
        clearLcdBuf();
        node = activeNode(currentTime, true); // Bypass GOODMSGMIN
        if (node) {
            strcpy(lcdBuf[0], node->node_name);
            if (strlen(node->node_name) < lcdCols) {
                memset(lcdBuf[strlen(node->node_name)], int(' '),
                       lcdCols - strlen(node->node_name));
            }
            strcpy(lcdBuf[1], "Port# xx ID# xx ");
            // port ID w/o NULL terminator
            memcpy(&(lcdBuf[1][6]), byteHexString(node->port_id), 2);
            // node ID w/o NULL terminator
            memcpy(&(lcdBuf[1][13]), byteHexString(node->node_id), 2);
            if (node->receive_count >= GOODMSGMIN) {
                lcdBuf[1][15] = '!';
            }
        } else if (message.node_id) {
            strcpy(lcdBuf[0], "Unknown Node    ");
            strcpy(lcdBuf[1], "Node ID# xx     ");
            memcpy(&(lcdBuf[1][9]), byteHexString(message.node_id), 2);
        } else {
            strcpy(lcdBuf[0], "Monitoring...   ");
        }
        printLcdBuf();
    }
    return false;
}

boolean portIDSetupCallback(unsigned long *currentTime) {
    D("portIDSetupCB\n");
    return true;
}

boolean travelAdjustCallback(unsigned long *currentTime) {
    D("travelAdjustCB\n");
    return true;
}

boolean portToggleCallback(unsigned long *currentTime) {
    D("portToggleCB\n");
    return true;
}

boolean vacToggleCallback(unsigned long *currentTime) {
    static bool onSelected = true;
    static unsigned long lastPaint=0;

    // HiJack monitor mode to freeze vac state machine
    if (lcdButtons) {
        if (lcdButtons & BUTTON_SELECT) {
            onSelected = !onSelected;
            lastPaint=0;
        } else { // exit
            vacControl(false, true); // vac OFF
            onSelected = true;
            monitorMode = false;
            lastPaint=0;
            return true;
        }
    }

    if (timerExpired(currentTime, &lastPaint, STATUSINTERVAL)) {
        clearLcdBuf();
        strcpy(lcdBuf[0], "Press Select to");
        if (onSelected) {
            strcpy(lcdBuf[1], "toggle Vac. on  ");
            vacControl(false, true); // vac OFF
        } else {
            strcpy(lcdBuf[1], "toggle Vac. off ");
            vacControl(true, true); // vac ON
        }
        printLcdBuf();
        lastPaint = *currentTime;
    }

    monitorMode = true;
    return false; // keep looping
}

#endif // CALLBACKS_H
