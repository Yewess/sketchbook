#ifndef CALLBACKS_H
#define CALLBACKS_H

#include "globals.h"

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
    // does not run if in another callback
    if (lcdButtons & BUTTON_SELECT) {
        if (currentMenu->child) {
            currentMenu = currentMenu->child;
            drawMenu();
        } else if (currentMenu->callback) {
            currentCallback = currentMenu->callback;
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
    if (currentCallback) {
        if ( (*currentCallback)(currentTime) ) {
            currentCallback = NULL;
        }
        lcdButtons = 0; // Callback handles own buttons
    } else {
        handleButtons(currentTime);
        // handleLCDState clears buttons
    }
}

boolean monitorCallback(unsigned long *currentTime) {
    static unsigned long lastPaint=0;
    nodeInfo_t *node=NULL;

    if (lcdButtons) { // any key pressed
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
            strcpy(lcdBuf[1], "Port# xx ID# xx ");
            // port ID w/o NULL terminator
            memcpy(&(lcdBuf[1][6]), byteHexString(node->port_id), 2);
            // node ID w/o NULL terminator
            memcpy(&(lcdBuf[1][13]), byteHexString(node->node_id), 2);
            if (node->receive_count >= GOODMSGMIN) {
                lcdBuf[1][15] = '!';
            }
        } else {
            strcpy(lcdBuf[0], "Monitoring...   ");
        }
        printLcdBuf();
    }
    return false;
}

boolean portIDSetupCallback(unsigned long *currentTime) {
    D("portIDSetupCB\n");
}

boolean travelAdjustCallback(unsigned long *currentTime) {
    D("travelAdjustCB\n");
}

boolean portToggleCallback(unsigned long *currentTime) {
    D("portToggleCB\n");
}

boolean vacToggleCallback(unsigned long *currentTime) {
    D("vacToggleCB\n");
}

#endif // CALLBACKS_H
