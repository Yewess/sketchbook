#ifndef CALLBACKS_H
#define CALLBACKS_H

#include "globals.h"
#include "control.h"

// menu items defined in menu.h
extern menuEntry_t m_setup;

boolean blinkBacklight(unsigned long *currentTime) {
    if (timerExpired(currentTime, &lastButtonChange, LCDBLINKTIME)) {
        lcd.setBacklight(0x1); // ON
        return true;
    } else {
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
            lastButtonChange = *currentTime; // Don't process in callback also
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
            lastButtonChange = *currentTime; // Don't process in callback also
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
    if (currentCallback != NULL) {
        if ( (*currentCallback)(currentTime) == true) {
            currentCallback = NULL;
            drawMenu();
            lcdButtons = 0;
            lastButtonChange = *currentTime; // Don't process last button again
        }
    } else {
        handleButtons(currentTime);
        // handleLCDState clears buttons
    }
}

boolean monitorCallback(unsigned long *currentTime) {
    nodeInfo_t *node=NULL;

    if (lcdButtons) { // any key pressed
        // reset all node counters
        updateNodes(NULL);
        monitorMode = false;
        return true;
    }
    monitorMode = true; // Don't turn on anything
    // update lcdBuf & print every STATUSINTERVAL
    clearLcdBuf();
    node = activeNode(currentTime, true); // Bypass GOODMSGMIN
    if (node) {
        strcpy(lcdBuf[0], node->node_name);
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

    // Write screen template
    clearLcdBuf();
    strcpy(lcdBuf[0], lcdPortIDLine);
    strcpy(lcdBuf[1], node->node_name);
    // draw IDs
    memcpy(&(lcdBuf[0][6]), byteHexString(node->port_id), 2);
    memcpy(&(lcdBuf[0][13]), byteHexString(node->node_id), 2);
    // draw selection
    if (blinkOn | !changing) {
        if (nodeIDField) {
            lcdBuf[0][12] = lcdRArrow;
            lcdBuf[0][15] = lcdLArrow;
        } else { // name selection
            lcdBuf[0][5] =  lcdDArrow;
            lcdBuf[0][8] =  lcdDArrow;
            lcdBuf[0][12] = lcdDArrow;
            lcdBuf[0][15] = lcdDArrow;
        }
    }
    // draw cursor
    if (changing) {
        if (nodeIDField) {
            lcd.setCursor(14, 0);
        } else { // changing name
            lcd.setCursor(position, 1);
            curch = node->node_name[position];
        }
    }

    // handle key presses
    if (lcdButtons) {
        if (lcdButtons & BUTTON_RIGHT) {
            if (changing) {
                if (!nodeIDField) { // name change
                    if (position == NODENAMEMAX-2) {
                        position = 0;
                    } else {
                        position++;
                    }
                } // else do nothing for nodeIDField
            } else { // Select other field
                nodeIDField = !nodeIDField;
                position=0;
            }
        }
        if (lcdButtons & BUTTON_LEFT) {
            if (!changing) { // exit
                node_index=0;
                position=0;
                changing=false;
                nodeIDField=true;
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
                        position = NODENAMEMAX-2;
                    } else {
                        position--;
                    }
                } // else do nothing for nodeIDField
            }
        }
        if (lcdButtons & BUTTON_UP) {
            if (!changing) {
                position=0;
                if (node_index >= (MAXNODES - 1)) {
                    node_index = 0;
                } else {
                    node_index++;
                }
            } else { // character change
                if (nodeIDField) {
                   node->node_id += 1; // will wrap automaticaly
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
                position=0;
                if (node_index == 0) {
                    node_index = (MAXNODES - 1);
                } else {
                    node_index--;
                }
            } else { // character change
                if (nodeIDField) {
                    node->node_id -= 1; // will wrap automaticaly
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

    if (timerExpired(currentTime, &lastPaint, BUTTONCHANGE)) {
        blinkOn = !blinkOn;
        lastPaint = *currentTime;
        if ((!blinkOn) & changing & (!nodeIDField)){ // Draw the cursor
            lcdBuf[1][position] = LCDBLOCK;
        }
    }
    printLcdBuf();

    return false; // keep looping
}

boolean travelAdjustCallback(unsigned long *currentTime) {
    static unsigned long lastPaint=0;
    static byte node_index = 0;
    static boolean changing = false; // currently modifying a field
    nodeInfo_t *node = &(nodeInfo[node_index]);
    word minmax = 0;
    byte minmaxMSB = 0;
    byte minmaxLSB = 0;
    int blinkOn=0;

    // make sure servos are on
    servoControl(true);

    // Write screen template
    clearLcdBuf();
    strcpy(lcdBuf[0], lcdPortIDLine);
    // draw IDs
    memcpy(&(lcdBuf[0][6]), byteHexString(node->port_id), 2);
    memcpy(&(lcdBuf[0][13]), byteHexString(node->node_id), 2);
    if (!changing) {
        strcpy(lcdBuf[1], node->node_name);
    } else { // draw hi/lo instead of name
        strcpy(lcdBuf[1], lcdRangeLine); // text
        // arrows
        if (blinkOn % 2) { // Blink on even numbers
            lcdBuf[1][5] = lcdLArrow;
            lcdBuf[1][6] = lcdRArrow;
            lcdBuf[1][14] = lcdUArrow;
            lcdBuf[1][15] = lcdDArrow;
        } else { // Blink off on odd numbers
            lcdBuf[1][6] = lcdLArrow;
            lcdBuf[1][5] = lcdRArrow;
            lcdBuf[1][15] = lcdUArrow;
            lcdBuf[1][14] = lcdDArrow;

        }
        // first low
        minmax = node->servo_min;
        minmaxMSB = highByte(minmax);
        minmaxLSB = lowByte(minmax);
        memcpy(&(lcdBuf[1][1]), byteHexString(minmaxMSB), 2);
        memcpy(&(lcdBuf[1][3]), byteHexString(minmaxLSB), 2);
        // last high
        minmax = node->servo_max;
        minmaxMSB = highByte(minmax);
        minmaxLSB = lowByte(minmax);
        memcpy(&(lcdBuf[1][10]), byteHexString(minmaxMSB), 2);
        memcpy(&(lcdBuf[1][12]), byteHexString(minmaxLSB), 2);
    }

    // handle key presses
    if (lcdButtons) {
        if (lcdButtons & BUTTON_RIGHT) {
            if (changing) {
                // increment low range
                node->servo_min += SERVOINCDEC;
            }
        }
        if (lcdButtons & BUTTON_LEFT) {
            if (!changing) { // exit
                node_index=0;
                changing=false;
                lastPaint=0;
#ifndef DEBUG
                writeNodeIDServoMap(); // only updates changed info when not in debug
#endif
                servoControl(false); // turn off
                return true; // EXIT callback
            } else {
                // decrement low range
                node->servo_min -= SERVOINCDEC;
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
                // increment high range
                node->servo_max += SERVOINCDEC;
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
                // decrement high range
                node->servo_max += SERVOINCDEC;
            }
        }
        if (lcdButtons & BUTTON_SELECT) {
            changing = !changing;
        }
    }

    // constrain minmax to range
    if (node->servo_max > servoMaxPW) {
        node->servo_max = servoMaxPW;
    } else if (node->servo_max < servoMinPW) {
        node->servo_max = servoMinPW;
    }
    if (node->servo_min > servoMaxPW) {
        node->servo_min = servoMaxPW;
    } else if (node->servo_min < servoMinPW) {
        node->servo_min = servoMinPW;
    }

    if (timerExpired(currentTime, &lastPaint, STATUSINTERVAL)) {
        blinkOn++;
        if (blinkOn > 29) {
            blinkOn = 0;
        }
        if (blinkOn < 15) {
            moveServos(node->port_id);
        } else {
            moveServos(CLOSEALLPORT);
        }
        lastPaint = *currentTime;
    }
    printLcdBuf();
    return false; // keep looping
}

boolean portToggleCallback(unsigned long *currentTime) {
    static byte node_index = 0;
    static boolean allOpen = false;
    nodeInfo_t *node = &(nodeInfo[node_index]);

    // make sure servos are on
    servoControl(true);

    // initialize with all servos open
    if (!allOpen) {
        moveServos(OPENALLPORT);
        delay(SERVOMOVETIME);
        allOpen = true;
    }

    // Write screen template
    clearLcdBuf();
    strcpy(lcdBuf[0], lcdPortIDLine);
    strcpy(lcdBuf[1], node->node_name);
    // draw IDs
    memcpy(&(lcdBuf[0][6]), byteHexString(node->port_id), 2);
    memcpy(&(lcdBuf[0][13]), byteHexString(node->node_id), 2);

    // handle key presses
    if (lcdButtons) {
        if ( (lcdButtons & BUTTON_RIGHT) ||
             (lcdButtons & BUTTON_SELECT) ) {
            moveServos(node->port_id); // open port
        }
        if (lcdButtons & BUTTON_LEFT) {
            // exit
                node_index=0;
                allOpen = false;
                monitorMode = false;
                moveServos(OPENALLPORT);
                delay(SERVOMOVETIME);
                servoControl(false); // turn off
                return true; // EXIT callback
        }
        if (lcdButtons & BUTTON_UP) {
            if (node_index >= (MAXNODES - 1)) {
                node_index = 0;
            } else {
                node_index++;
            }
            allOpen = false; // make sure everything opens
        }
        if (lcdButtons & BUTTON_DOWN) {
            if (node_index == 0) {
                node_index = (MAXNODES - 1);
            } else {
                node_index--;
            }
            allOpen = false; // make sure everything opens
        }
    }

    // constrain minmax to range
    if (node->servo_max > servoMaxPW) {
        node->servo_max = servoMaxPW;
    } else if (node->servo_max < servoMinPW) {
        node->servo_max = servoMinPW;
    }
    if (node->servo_min > servoMaxPW) {
        node->servo_min = servoMaxPW;
    } else if (node->servo_min < servoMinPW) {
        node->servo_min = servoMinPW;
    }

    // HiJack monitor mode to freeze vac state machine
    monitorMode = true;
    printLcdBuf();
    return false; // keep looping
}

boolean vacToggleCallback(unsigned long *currentTime) {
    static bool onSelected = true;

    // HiJack monitor mode to freeze vac state machine
    if (lcdButtons) {
        if (lcdButtons & BUTTON_SELECT) {
            onSelected = !onSelected;
        } else { // exit
            vacControl(false, true); // vac OFF
            onSelected = true;
            monitorMode = false;
            return true;
        }
    }

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

    monitorMode = true;
    return false; // keep looping
}

#endif // CALLBACKS_H
