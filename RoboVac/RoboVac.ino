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
#include <RoboVac.h>

/* Externals */

/* Definitions */
#define DEBUG

// Constants used once (to save space)
#define POLLINTERVAL 25
#define THRESHOLDINTERVAL 101
#define STATUSINTERVAL 5003
#define THRESHOLD 10000 // 10 second reception threshold
#define SERIALBAUD 9600
#define RXBAUD 300

/* Types */

/* I/O constants */
const int statusLEDPin = 13;
const int rxDataPin = 8;

/* Global Variables */
message_t message;
boolean blankMessage = true; // Signal not to read from message
unsigned long lastReception = 0; // millis() a message was last received
int goodMessageCount = 0;
int badMessageCount = 0;

/* Functions */

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
            goodMessageCount++;
            copyMessage(message, (message_t *) messageBuff);
            blankMessage = false;
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
    if (blankMessage) {
        Serial.print(" \tLast NodeID Received: "); Serial.print("?");
    } else {
        Serial.print(" \tLast NodeID Received: "); Serial.print(message.node_id, DEC);
    }
    Serial.print("  Good Messages: "); Serial.print(goodMessageCount);
    Serial.print("  Bad Messages: "); Serial.print(badMessageCount);
    Serial.println("");

    goodMessageCount = 0;
    badMessageCount = 0;
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
    vw_setup(RXBAUD);
    vw_rx_start();

    // Setup events
    TimedEvent.addTimer(POLLINTERVAL, pollRxEvent);
    TimedEvent.addTimer(THRESHOLDINTERVAL, checkThresholdEvent);

#ifdef DEBUG
    TimedEvent.addTimer(STATUSINTERVAL, printStatusEvent);
    Serial.println("setup()");
    Serial.print("  rxDataPin: "); Serial.print(rxDataPin);
    Serial.print("  statusLEDPin: "); Serial.print(statusLEDPin);
    Serial.print("  SERIALBAUD: "); Serial.print(SERIALBAUD);
    Serial.print("  RXBAUD: "); Serial.print(RXBAUD);
    Serial.print("\nStat. Int.: "); Serial.print(STATUSINTERVAL);
    Serial.print("ms  Poll Int.: "); Serial.print(POLLINTERVAL);
    Serial.println("ms"); Serial.println("loop()");
#endif // DEBUG
}

void loop() {
    TimedEvent.loop(); // blocking
}
