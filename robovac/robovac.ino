/*
  Vacuum power and hose selection controller

    Copyright (C) 2012 Chris Evich <chris-arduino@anonomail.me>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
    Dependencies:
    ebl-arduino: http://code.google.com/p/ebl-arduino/
    virtual wire: http://www.pjrc.com/teensy/td_libs_VirtualWire.html

    Pitfalls: virtual wire uses Timer1 (can't use PWM on pin 9 & 10
*/

#include <TimedEvent.h>
#include <VirtualWire.h>
#include "RoboVac.h"

/* Externals */

/* Definitions */
#define DEBUG

// Constants used once (to save space)
#define POLLINTERVAL 25
#define THRESHOLDINTERVAL 101
#define STATUSINTERVAL 5003
#define THRESHOLD 10000 // 10 second reception threshold
#define GOODMSGMIN 10 // Minimum number of good messages to signal start
#define GOODBADRATIO 2 // Good / Bad ratio required to start
#define TIMEOUT 5000 // message confirmation failed
#define SERVOPOWERTIME 250 // ms to wait for servo's to power up/down
#define SERVOMOVETIME 1000 // ms to wait for servo's to move
#define VACPOWERTIME 2000 // ms to wait for vac to power on

/* I/O constants */
const int statusLEDPin = 13;
const int rxDataPin = 8;
const int servoPowerControlPin = 8;
const int vacPowerControlPin = 9;

/* Global Variables */
message_t message;
boolean blankMessage = true; // Signal not to read from message
unsigned long lastReception = 0; // millis() a message was last received
unsigned long lastStateChange = 0; // last time state was changed
int goodMessageCount = 0;
int badMessageCount = 0;
vacstate_t actionState = VAC_ENDSTATE; // Current system state
#ifdef DEBUG
const char *statusMessage;
#endif // DEBUG

/* Functions */

void servoControl(boolean turnOn) {
    if (turnON == true) {
        digitalWrite(servoPowerControlPin, HIGH);
    } else {
        digitalWrite(servoPowerControlPin, LOW);
    }
}

void vacControl(boolean turnOn) {
    if (turnON == true) {
        digitalWrite(vacPowerControlPin, HIGH);
    } else {
        digitalWrite(servoPowerControlPin, LOW);
    }
}

void printMessage(const message_t *message) {
    Serial.print("Message:");
    Serial.print(" Magic: 0x"); Serial.print(message->magic, HEX);
    Serial.print(" Version: "); Serial.print(message->version);
    Serial.print(" node ID: "); Serial.print(message->node_id);
    Serial.print(" Uptime: ");
    Serial.print(message->up_days); Serial.print("d");
    Serial.print(message->up_hours); Serial.print("h");
    Serial.print(message->up_minutes); Serial.print("m");
    Serial.print(message->up_seconds); Serial.print("s");
    Serial.print(message->up_millis); Serial.print("ms");
    Serial.println();
}


void updateState(const char *newMessage) {
    lastStateChange = millis();
    if (actionState >= VAC_ENDSTATE) {
        actionState = VAC_LISTENING;
    } else {
        actionState++;
    }
#ifdef DEBUG
    Serial.print(statusMessage); Serial.print(" -> ");
    Serial.println(newMessage);
    statusMessage = newMessage;
#endif
}


void moveServos(byte node_id) {
    // will be called repeatidly until SERVOMOVETIME expires

    // if node_id == 0
        // open all servos
    // else
        // close all !node_id servos

    // STUB
    Serial.print("#"); Serial.print(node_id);
    Serial.println(" Buzzzzzzzzzz");
}

void handleMessageFault(void) {

    // handle each state specially

    // STUB
}

void handleActionState(void)
    unsigned long currentTime = millis();
    vacstate_t newState = actionState;

    switch (actionState) {

        case VAC_LISTENING:
            if (goodMessageCount >= GOODMSGMIN) {
                updateState("VAC_CONFIRMING");
            }
            break;

        case VAC_CONFIRMING:
            int ratio = goodMessageCount / badMessageCount;

            if (ratio >= GOODBADRATIO) {
                updateState("VAC_SERVOPOWERUP");
            } else {
                if (currentTime > (lastStateChange + TIMEOUT)) {
                    actionState = VAC_ENDSTATE;
                    updateStatus("VAC_LISTENING")
                }
            }
            break;

        case VAC_VACPOWERUP:
            digitalWrite(vacPowerControlPin, HIGH);
            if (currentTime > (lastStateChange + VACPOWERTIME)) {
                updateStatus("VAC_SERVOPOWERUP");
            }
            break;

        case VAC_SERVOPOWERUP:
            digitalWrite(servoPowerControlPin, HIGH);
            if (currentTime > (lastStateChange + SERVOPOWERTIME)) {
                updateStatus("VAC_SERVOACTION");
            }
            break;

        case VAC_SERVOACTION:
            moveServos(message->node_id);
            if (currentTime > (lastStateChange + SERVOMOVETIME)) {
                updateStatus("VAC_SERVOPOWERDN");
            }
            break;

        case VAC_SERVOPOWERDN:
            digitalWrite(servoPowerControlPin, LOW);
            if (currentTime > (lastStateChange + SERVOPOWERTIME)) {
                updateStatus("VAC_VACUUMING");
            }
            break;

        case VAC_VACUUMING:
            // Sign of blank messages > threshold  or ID change
            if ( (blankMessage == true) && (goodMessageCount == 0) &&
                                             (badMessageCount == 0) ) {
                updateStatus("VAC_VACPOWERDN");
            }
            break;

        case VAC_VACPOWERDN:
            digitalWrite(vacPowerControlPin, LOW);
            moveServos(0); // Opens all doors
            if (currentTime > (lastStateChange + VACPOWERTIME)) {
                updateStatus("VAC_ENDSTATE");
            }
            break;

        case VAC_ENDSTATE:
        case default:
            actionState = VAC_ENDSTATE;
            statusMessage("VAC_LISTENING");
            break;
    }
}


void pollRxEvent(TimerInformation *Sender) {
    boolean CRCGood = false;
    uint8_t buffLen = VW_MAX_PAYLOAD;
    uint8_t messageBuff[VW_MAX_PAYLOAD] = {0};

    if (vw_have_message()) {
        digitalWrite(statusLEDPin, HIGH);
        CRCGood = vw_get_message(messageBuff, &buffLen);
#ifdef DEBUG
        if (buffLen == MESSAGESIZE) {
            printMessage((message_t *) messageBuff);
        }
#endif // DEBUG
        if (CRCGood == true && validMessage((message_t *) messageBuff)) {
            // Start counting after second good message
            if ( ((message_t *) messageBuff)->node_id == message->node_id ) {
                goodMessageCount++;
            } else if (blankMessage == false) {
                // mid-stream node_id change
                handleMessageFault;
            } else {
                copyMessage(&message, (message_t *) messageBuff);
                blankMessage = false;
                handleActionState();
            }
        } else {
            badMessageCount++;
        }
    } else {
        digitalWrite(statusLEDPin, LOW);
    }
}

void checkThresholdEvent(TimerInformation *Sender) {
    unsigned int currentTime = millis();

    if (currentTime - lastReception > THRESHOLD) {
        if (!blankMessage) {
            blankMessage = true;
            goodMessageCount = 0;
            badMessageCount = 0;
        }
    }
}

void printStatusEvent(TimerInformation *Sender) {
    unsigned long currentTime = millis();
    int seconds = (currentTime / 1000);
    int minutes = seconds / 60;
    seconds = seconds % 60;
    int hours = minutes / 60;
    minutes = minutes % 60;
    int days = hours / 24;
    hours = hours % 24;

    Serial.print(days); Serial.print(":");
    Serial.print(hours); Serial.print(":");
    Serial.print(minutes); Serial.print(":");
    Serial.print(seconds);
    Serial.print("  Good: "); Serial.print(goodMessageCount);
    Serial.print("  Bad: "); Serial.print(badMessageCount);
    if (!blankMessage) {
        printMessage(message)
    }
    Serial.println("");
}

/* Main Program */

void setup() {

    // debugging info
#ifdef DEBUG
    Serial.begin(SERIALBAUD);
#endif // DEBUG

    pinMode(rxDataPin, INPUT);
    vw_set_rx_pin(rxDataPin);
    vw_set_ptt_pin(statusLEDPin);
    pinMode(statusLEDPin, OUTPUT);
    digitalWrite(statusLEDPin, LOW);
    vw_setup(RXTXBAUD);
    vw_rx_start();

    // Setup events
    TimedEvent.addTimer(POLLINTERVAL, pollRxEvent);
    TimedEvent.addTimer(THRESHOLDINTERVAL, checkThresholdEvent);

    // Setup state machine
    lastStateChange = millis();

    // debugging stuff
#ifdef DEBUG
    TimedEvent.addTimer(STATUSINTERVAL, printStatusEvent);
    Serial.println("setup()");
    Serial.print("  rxDataPin: "); Serial.print(rxDataPin);
    Serial.print("  statusLEDPin: "); Serial.print(statusLEDPin);
    Serial.print("  SERIALBAUD: "); Serial.print(SERIALBAUD);
    Serial.print("  RXTXBAUD: "); Serial.print(RXTXBAUD);
    Serial.print("\nStat. Int.: "); Serial.print(STATUSINTERVAL);
    Serial.print("ms  Poll Int.: "); Serial.print(POLLINTERVAL);
    Serial.println("ms"); Serial.println("loop()");
#endif // DEBUG
}

void loop() {
    TimedEvent.loop(); // blocking
}
