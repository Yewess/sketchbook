
#ifndef EVENTS_H
#define EVENTS_H

#include <TimedEvent.h>
#include <Wire.h>
#include <Adafruit_RGBLCDShield.h>
#include <RoboVac.h>
#include "globals.h"
#include "nodeinfo.h"
#include "statemachine.h"

void robovacStateEvent(TimerInformation *Sender) {
    unsigned long currentTime = millis();
    updateNodes(&currentTime);
    if (monitorMode == false) {
        handleActionState(&currentTime);
    }
}

void pollRxEvent(TimerInformation *Sender) {
    boolean CRCGood = false;
    uint8_t buffLen = sizeof(message_t);
    uint8_t *messageBuff = (uint8_t *) &message;
    unsigned long currentTime = millis();
    nodeInfo_t *node;

    signalStrength = analogRead(signalStrengthPin);
    if (vw_have_message()) {
        digitalWrite(statusLEDPin, HIGH);
        CRCGood = vw_get_message(messageBuff, &buffLen);
        node = findNode(message.node_id);
        if ( CRCGood && validMessage(&message) && node ) {
            switch (message.msgType) {
                case MSG_HELLO:
                    node->first_heard = message.up_time;
                    node->last_heard = currentTime;
                    node->batteryPercent = message.batteryPercent;
                    node->receive_count = 0; // node rebooted
                    break;
                case MSG_STATUS:
                    // Validate up_time always > first_heard
                    if (message.up_time >= node->first_heard) {
                        node->batteryPercent = message.batteryPercent;
                        node->last_heard = currentTime;
                    }
                    break;
                case MSG_BREACH:
                    // Validate up_time always > first_heard
                    if (message.up_time >= node->first_heard) {
                        node->batteryPercent = message.batteryPercent;
                        node->receive_count = node->receive_count + 1;
                        node->last_heard = currentTime;
                    }
            }
        }
    }
}

void statusEvent(TimerInformation *Sender) {
    SP("---\n");
    printNodes();
    digitalWrite(statusLEDPin, LOW);
}

void lcdEvent(TimerInformation *Sender) {
    unsigned long currentTime = millis();
    uint8_t new_buttons=0;

    new_buttons = lcd.readButtons();
    if (new_buttons != lcdButtons) {
        lastButtonChange = currentTime;
        lcdButtons = new_buttons;
    } else { // no change
        if ((lcdButtons != 0) && timerExpired(&currentTime, &lastButtonChange, BUTTONCHANGE)) {
            lastButtonChange = currentTime;
            // allow second press through
        } else {
            // block second press of same button in < BUTTONCHANGE ms
            lcdButtons = 0;
        }
    }
    // Clears lcdButtons that were handled
    handleLCDState(&currentTime);
}

#endif // EVENTS_H
