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
                memset(&(lcdBuf[0][strlen(node->node_name)]),
                       int(' '),
                       lcdCols - strlen(node->node_name));
            }
            strcpy(lcdBuf[1], lcdPortIDLine);
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
    static unsigned long lastPaint=0;
    static byte node_index = 0;
    static byte position = 0; // if (changing), char offset being changed
    static boolean changing = false; // currently modifying a field
    static boolean nodeIDField = true; // false==name field
    nodeInfo_t *node = &(nodeInfo[node_index]);
    char curch;
    static boolean blinkOn=false;

    blinkOn = !blinkOn;

    // Write screen template
    clearLcdBuf();
    strcpy(lcdBuf[0], lcdPortIDLine);
    strcpy(lcdBuf[1], node->node_name);
    // pad node name with blanks
    if (strlen(node->node_name) < lcdCols) {
        memset(&(lcdBuf[1][strlen(node->node_name)]),
               int(' '),
               lcdCols - strlen(node->node_name));
    }
    // draw IDs
    memcpy(&(lcdBuf[0][6]), byteHexString(node->port_id), 2);
    memcpy(&(lcdBuf[0][13]), byteHexString(node->node_id), 2);
    // draw selection
    if (blinkOn) {
        if (nodeIDField) {
            lcdBuf[0][12] = LCDRARROW;
            lcdBuf[0][15] = LCDLARROW;
        } else { // name selection
            lcdBuf[0][5] = LCDDARROW;
            lcdBuf[0][8] = LCDDARROW;
            lcdBuf[0][12] = LCDDARROW;
            lcdBuf[0][15] = LCDDARROW;
        }
    }
    // draw cursor
    if (changing) {
        lcd.cursor();
        lcd.blink();
        if (nodeIDField) {
            lcd.setCursor(14, 0);
        } else { // changing name
            lcd.setCursor(position, 1);
            curch = node->node_name[position];
        }
    } else { // not changing
        lcd.noCursor();
        lcd.noBlink();
    }

    if (timerExpired(currentTime, &lastPaint, STATUSINTERVAL)) {
        printLcdBuf();
        lastPaint = *currentTime;
    }

    // handle key presses
    if (lcdButtons) {
        if (lcdButtons & BUTTON_RIGHT) {
            if (changing) {
                if (!nodeIDField) { // name change
                    if (position == 15) {
                        position = 0;
                    } else {
                        position++;
                    }
                } // else do nothing for nodeIDField
            } else { // Select other field
                nodeIDField = !nodeIDField;
            }
        }
        if (lcdButtons & BUTTON_LEFT) {
            if (!changing) { // exit
                node_index=0;
                position=0;
                changing=false;
                nodeIDField=true;
                monitorMode = false;
                lastPaint=0;
                lcd.noCursor();
                lcd.noBlink();
#ifndef DEBUG
                writeNodeIDServoMap(); // only updates changed info
#endif
                return true; // EXIT callback
            } else {
                if (!nodeIDField) { // name change
                    if (position == 0) {
                        position = 15;
                    } else {
                        position--;
                    }
                } // else do nothing for nodeIDField
            }
        }
        if (lcdButtons & BUTTON_UP) {
            if (!changing) {
                if (node_index >= (MAXNODES - 1)) {
                    node_index = 0;
                } else {
                    node_index++;
                }
            } else { // character change
                if (nodeIDField) {
                    if (node->node_id >= 254) {
                        node->node_id = 1;
                    } else {
                        node->node_id += 1;
                    }
                } else { // name field
                    if (curch >= CHARUPPER) {
                        curch = CHARLOWER;
                    } else {
                        curch++;
                    }
                    node->node_name[position] = curch;
                }
            }
        }
        if (lcdButtons & BUTTON_DOWN) {
            if (!changing) {
                if (node_index == 0) {
                    node_index = (MAXNODES - 1);
                } else {
                    node_index--;
                }
            } else { // character change
                if (nodeIDField) {
                    if (node->node_id <= 1) {
                        node->node_id = 254;
                    } else {
                        node->node_id -= 1;
                    }
                } else { // name field
                    if (curch <= CHARLOWER) {
                        curch = CHARUPPER;
                    } else {
                        curch--;
                    }
                    node->node_name[position] = curch;
                }
            }
        }
        if (lcdButtons & BUTTON_SELECT) {
            changing = !changing;
        }
    }

    monitorMode = true;
    return false; // keep looping
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
