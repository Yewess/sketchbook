/*
  Analog current sensing and encapsulation w/in message packet
  
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
*/

#include <TimedEvent.h>
#include <VirtualWire.h>

/* Externals */

/* Definitions */

/* I/O constants */
const int statusLEDPin = 13;
const int rxDataPin = 8;

/* comm. constants */
const unsigned int statusInterval = 5003; //milliseconds
const unsigned int pollInterval = 25; // milliseconds
const unsigned int thresholdInterval = 101; // milliseconds
const unsigned int serialBaud = 9600;
const unsigned int rxBaud = 300;
const unsigned int threshold = 10000; // 10 second reception threshold

/* Global Variables */
char nodeID[VW_MAX_PAYLOAD] = "";
unsigned long lastReception = 0; // millis() a message was last received
int goodMessageCount = 0;
int badMessageCount = 0;

/* Functions */

void pollRxEvent(TimerInformation *Sender) {
    boolean CRCGood = false;
    char nodeIDBuff[VW_MAX_PAYLOAD] = "";

    if (vw_have_message()) {
        digitalWrite(statusLEDPin, HIGH);
        CRCGood = vw_get_message((uint8_t *) nodeIDBuff, VW_MAX_PAYLOAD);
        if (CRCGood) {
            goodMessageCount++;
            nodeID = nodeIDBuff;
        } else {
            badMessageCount++;
        }
    } else {
        digitalWrite(statusLEDPin, LOW);
    }
}

void checkThresholdEvent(TimerInformation *Sender) {
    unsigned int currentTime = millis();

    if (currentTime - lastReception > threshold) {
        nodeID = "";
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
    Serial.print(" \tLast NodeID Received: "); Serial.print(nodeID);
    Serial.print("  Good Messages: "); Serial.print(goodMessageCount);
    Serial.print("  Bad Messages: "); Serial.print(badMessageCount);
    Serial.println("");

    goodMessageCount = 0;
    badMessageCount = 0;
    nodeID = 0;
}

/* Main Program */

void setup() {
    // debugging info
    Serial.begin(serialBaud);
    Serial.println("setup()");
    Serial.print("  rxDataPin: "); Serial.print(rxDataPin);
    pinMode(rxDataPin, INPUT);
    vw_set_rx_pin(rxDataPin);
    vw_set_ptt_pin(statusLEDPin);
    Serial.print("  statusLEDPin: "); Serial.print(statusLEDPin);
    pinMode(statusLEDPin, OUTPUT);
    digitalWrite(statusLEDPin, LOW);
    Serial.print("  serialBaud: "); Serial.print(serialBaud);
    Serial.print("  rxBaud: "); Serial.print(rxBaud);
    vw_setup(rxBaud);
    Serial.print("\nStat. Int.: "); Serial.print(statusInterval);
    Serial.print("ms  Poll Int.: "); Serial.print(pollInterval);
    vw_rx_start();

    // Setup events
    TimedEvent.addTimer(statusInterval, printStatusEvent);
    TimedEvent.addTimer(pollInterval, pollRxEvent);
    TimedEvent.addTimer(thresholdInterval, checkThresholdEvent);
    // Signal completion
    Serial.println("ms"); Serial.println("loop()");
}

void loop() {
    TimedEvent.loop(); // blocking
}
