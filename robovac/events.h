
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
            node->last_heard = currentTime;
            node->receive_count = node->receive_count + 1;
        }
    }
}

void statusEvent(TimerInformation *Sender) {
    digitalWrite(statusLEDPin, LOW);
    printNodes();
}

void lcdEvent(TimerInformation *Sender) {
    unsigned long currentTime = millis();
    uint8_t new_buttons=0;

    new_buttons = lcd.readButtons();
    if (new_buttons != lcdButtons) {
        lastButtonChange = currentTime;
        lcdButtons = new_buttons;
    }
    // Clears lcdButtons that were handled
    handleLCDState(&currentTime);
}

#endif // EVENTS_H
