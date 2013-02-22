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
    static enum { FIELD_NODEID,
                  FIELD_PORTID,
                  FIELD_NAME} fieldSelect = FIELD_NODEID;
    nodeInfo_t *node = &(nodeInfo[node_index]);
    char curch;
    static boolean blinkOn=false;

    monitorMode = true; // Don't turn on anything

    // Write screen template
    clearLcdBuf();
    strcpy(lcdBuf[0], lcdPortIDLine);
    strcpy(lcdBuf[1], node->node_name);
    // draw IDs
    memcpy(&(lcdBuf[0][6]), byteHexString(node->port_id), 2);
    memcpy(&(lcdBuf[0][13]), byteHexString(node->node_id), 2);
    // draw selection
    if (blinkOn | !changing) {
        switch (fieldSelect) {
            case FIELD_PORTID:
                lcdBuf[0][5] = lcdRArrow;
                lcdBuf[0][8] = lcdLArrow;
                break;
            case FIELD_NAME:
                lcdBuf[0][5] =  lcdDArrow;
                lcdBuf[0][8] =  lcdDArrow;
                lcdBuf[0][12] = lcdDArrow;
                lcdBuf[0][15] = lcdDArrow;
                break;
            default:
            case FIELD_NODEID:
                lcdBuf[0][12] = lcdRArrow;
                lcdBuf[0][15] = lcdLArrow;
                break;
        }
    }

    // setup cursor
    lcd.setCursor(position, 1);
    // current character cache
    curch = node->node_name[position];

    // handle key presses
    if (lcdButtons) {
        if (lcdButtons & BUTTON_RIGHT) {
            if (changing & (fieldSelect == FIELD_NAME)) {
                if (position == NODENAMEMAX-2) {
                    position = 0;
                } else {
                    position++;
                }
            } else { // next field
                position=0;
                switch (fieldSelect) {
                    case FIELD_PORTID:
                        fieldSelect = FIELD_NAME;
                        break;
                    case FIELD_NAME:
                        fieldSelect = FIELD_NODEID;
                        break;
                    default:
                    case FIELD_NODEID:
                        fieldSelect = FIELD_PORTID;
                        break;
                }
            }
        }
        if (lcdButtons & BUTTON_LEFT) {
            if (!changing) { // exit
                node_index=0;
                position=0;
                changing=false;
                fieldSelect = FIELD_NODEID;
                lastPaint=0;
                lcd.noCursor();
                lcd.noBlink();
                monitorMode = false;
#ifndef DEBUG
                writeNodeIDServoMap(); // only updates changed info
#endif
                return true; // EXIT callback
            } else {
                if (fieldSelect == FIELD_NAME) { // name change
                    if (position == 0) {
                        position = NODENAMEMAX-2;
                    } else {
                        position--;
                    }
                }
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
                switch (fieldSelect) {
                    case FIELD_PORTID:
                        node->port_id += 1; // will wrap automaticaly
                        break;
                    case FIELD_NAME:
                        lastPaint = 0; // force fast update
                        if (curch >= CHARUPPER) {
                            curch = CHARLOWER;
                        } else {
                            curch++;
                        }
                        node->node_name[position] = curch;
                        break;
                    default:
                    case FIELD_NODEID:
                        node->node_id += 1; // will wrap automaticaly
                        break;
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
                switch (fieldSelect) {
                    case FIELD_PORTID:
                        node->port_id -= 1; // will wrap automaticaly
                        break;
                    case FIELD_NAME:
                        lastPaint = 0; // force fast update
                        if (curch <= CHARLOWER) {
                            curch = CHARUPPER;
                        } else {
                            curch--;
                        }
                        node->node_name[position] = curch;
                        break;
                    default:
                    case FIELD_NODEID:
                        node->node_id -= 1; // will wrap automaticaly
                        break;
                }
            }
        }
        if (lcdButtons & BUTTON_SELECT) {
            changing = !changing;
        }
    }

    if (timerExpired(currentTime, &lastPaint, BUTTONCHANGE/2)) {
        blinkOn = !blinkOn;
        lastPaint = *currentTime;
        if ((!blinkOn) & changing & (fieldSelect == FIELD_NAME)){ // Draw the cursor
            lcdBuf[1][position] = LCDBLOCK;
        }
    }
    printLcdBuf();

    return false; // keep looping
}

boolean travelAdjustCallback(unsigned long *currentTime) {
    static unsigned long lastPaint=0;
    static byte node_index = 0;
    static boolean changing = false; // currently modifying entry
    static boolean change_low_last = true;
    static boolean blinkOn = true;
    nodeInfo_t *node = &(nodeInfo[node_index]);
    word minmax = 0;
    byte minmaxMSB = 0;
    byte minmaxLSB = 0;

    monitorMode = true; // Don't turn on anything

    // make sure servos are on
    servoControl(true);

    // handle key presses
    if (lcdButtons) {
        if (lcdButtons & BUTTON_RIGHT) {
            if (changing) {
                // increment low range
                node->servo_min += SERVOINCDEC;
                change_low_last = true;
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
                moveServos(OPENALLPORT);
                delay(SERVOMOVETIME);
                servoControl(false); // turn off
                monitorMode = false;
                return true; // EXIT callback
            } else {
                // decrement low range
                node->servo_min -= SERVOINCDEC;
                change_low_last = true;
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
                change_low_last = false;
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
                node->servo_max -= SERVOINCDEC;
                change_low_last = false;
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

    // Write screen template
    clearLcdBuf();
    strcpy(lcdBuf[0], lcdPortIDLine);
    // draw IDs
    memcpy(&(lcdBuf[0][6]), byteHexString(node->port_id), 2);
    memcpy(&(lcdBuf[0][13]), byteHexString(node->node_id), 2);

    if (!changing) {
        strcpy(lcdBuf[1], node->node_name);
        moveServos(CLOSEALLPORT);
    } else { // draw hi/lo instead of name
        strcpy(lcdBuf[1], lcdRangeLine); // text
        // arrows
        if (change_low_last) {
            if (blinkOn) {
                lcdBuf[1][5] = lcdLArrow; //low on
                lcdBuf[1][6] = lcdRArrow; //low on
                lcdBuf[1][15] = lcdUArrow; //hi off
                lcdBuf[1][14] = lcdDArrow; //hi off
            } else {
                lcdBuf[1][6] = lcdLArrow; //low off
                lcdBuf[1][5] = lcdRArrow; //low off
                lcdBuf[1][15] = lcdUArrow; //hi off
                lcdBuf[1][14] = lcdDArrow; //hi off
            }
        } else { // changed high
            if (blinkOn) {
                lcdBuf[1][14] = lcdUArrow; //hi on
                lcdBuf[1][15] = lcdDArrow; //hi on
                lcdBuf[1][6] = lcdLArrow; //low off
                lcdBuf[1][5] = lcdRArrow; //low off
            } else { // Blink off
                lcdBuf[1][15] = lcdUArrow; //hi off
                lcdBuf[1][14] = lcdDArrow; //hi off
                lcdBuf[1][6] = lcdLArrow; //low off
                lcdBuf[1][5] = lcdRArrow; //low off
            }
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

    // move servos
    if (changing) {
        if (change_low_last) {
            moveServos(CLOSEALLPORT); // close
        } else { // changed hi limit
            moveServos(node->port_id); // open
        }
    }

    if (timerExpired(currentTime, &lastPaint, 1000)) {
        blinkOn = !blinkOn;
        lastPaint = *currentTime;
    }
    printLcdBuf();
    return false; // keep looping
}

boolean powerTimersCallback(unsigned long *currentTime) {
    static unsigned long lastPaint=0;
    static boolean changing = false; // currently modifying entry
    static boolean change_VacPowerTime = true;
    static boolean blinkOn = true;
    byte timeMSB = 0;
    byte timeLSB = 0;

    // handle key presses
    if (lcdButtons) {
        // Right button does nothing

        if (lcdButtons & BUTTON_RIGHT) {
            if (!changing) { // exit
                changing=false;
                lastPaint=0;
                change_VacPowerTime = true;
#ifndef DEBUG
                writeNodeIDServoMap(); // only updates changed info when not in debug
#endif
                return true; // EXIT callback
            } else {
                if (change_VacPowerTime) {
                    vacpower.VacPowerTime -= 100;
                } else {
                    vacpower.VacOffTime -= 100;
                }
            }
        }
        if (lcdButtons & BUTTON_LEFT) {
            if (!changing) { // exit
                changing=false;
                lastPaint=0;
                change_VacPowerTime = true;
#ifndef DEBUG
                writeNodeIDServoMap(); // only updates changed info when not in debug
#endif
                return true; // EXIT callback
            } else {
                if (change_VacPowerTime) {
                    vacpower.VacPowerTime += 100;
                } else {
                    vacpower.VacOffTime += 100;
                }
            }
        }
        if (lcdButtons & BUTTON_UP) {
            if (!changing) {
                change_VacPowerTime = !change_VacPowerTime;
            } else { // number change
                if (change_VacPowerTime) {
                    vacpower.VacPowerTime += 10;
                } else {
                    vacpower.VacOffTime += 10;
                }
            }
        }
        if (lcdButtons & BUTTON_DOWN) {
            if (!changing) {
                change_VacPowerTime = !change_VacPowerTime;
            } else { // number change
                if (change_VacPowerTime) {
                    vacpower.VacPowerTime -= 10;
                } else {
                    vacpower.VacOffTime -= 10;
                }
            }
        }
        if (lcdButtons & BUTTON_SELECT) {
            changing = !changing;
        }
    }

    // constrain minmax to range
    if (vacpower.VacPowerTime > 3600) {
        vacpower.VacPowerTime = 0;
    }
    if (vacpower.VacOffTime > 14400) {
        vacpower.VacOffTime = 0;
    }

    // Write screen template
    clearLcdBuf();
    strcpy(lcdBuf[0], "Min On  XXXXsec ");
    strcpy(lcdBuf[1], "Cool Dn XXXXsec ");

    // Handle blink & selector arrows
    if (blinkOn || changing) {
        // arrows
        if (change_VacPowerTime) {
            lcdBuf[0][7] = lcdLArrow;
            lcdBuf[0][15] = lcdRArrow;
        } else {
            lcdBuf[1][7] = lcdLArrow;
            lcdBuf[1][15] = lcdRArrow;
        }
    }

    // Draw numbers
    timeMSB = highByte(vacpower.VacPowerTime);
    timeLSB = lowByte(vacpower.VacPowerTime);
    memcpy(&(lcdBuf[0][8]), byteHexString(timeMSB), 2);
    memcpy(&(lcdBuf[0][10]), byteHexString(timeLSB), 2);
    timeMSB = highByte(vacpower.VacOffTime);
    timeLSB = lowByte(vacpower.VacOffTime);
    memcpy(&(lcdBuf[1][8]), byteHexString(timeMSB), 2);
    memcpy(&(lcdBuf[1][10]), byteHexString(timeLSB), 2);

    if (timerExpired(currentTime, &lastPaint, 1000)) {
        blinkOn = !blinkOn;
        lastPaint = *currentTime;
    }
    printLcdBuf();
    return false; // keep looping
}


boolean portToggleCallback(unsigned long *currentTime) {
    static byte node_index = 0;
    nodeInfo_t *node = &(nodeInfo[node_index]);

    // make sure servos are on
    servoControl(true);

    // handle key presses
    if (lcdButtons) {
        if ( (lcdButtons & BUTTON_RIGHT) ||
             (lcdButtons & BUTTON_SELECT) ||
             (lcdButtons & BUTTON_LEFT) ) {
            // exit
                node_index=0;
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
        }
        if (lcdButtons & BUTTON_DOWN) {
            if (node_index == 0) {
                node_index = (MAXNODES - 1);
            } else {
                node_index--;
            }
        }
    }

    moveServos(node->port_id);
    delay(SERVOMOVETIME);

    // Write screen template
    clearLcdBuf();
    strcpy(lcdBuf[0], lcdPortIDLine);
    strcpy(lcdBuf[1], node->node_name);
    // draw IDs
    memcpy(&(lcdBuf[0][6]), byteHexString(node->port_id), 2);
    memcpy(&(lcdBuf[0][13]), byteHexString(node->node_id), 2);


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
