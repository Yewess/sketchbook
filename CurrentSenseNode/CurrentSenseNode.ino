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

/* I/O constants */
const int statusLEDPin = 13;
const int txPTTPin = 7;
const int txDataPin = 8;
const int currentSensePin = A0;
const int thresholdPin = A1;

/* comm. constants */
const byte nodeID = 0x01;
const unsigned int senseInterval = 100;
const unsigned int txInterval = 501;
const unsigned int statusInterval = 5002;
const unsigned int serialBaud = 9600;
const unsigned int txBaud = 300;
const byte padding = B10101010;

/* Types */

/* Needs to be exactly 30 bytes*/
struct SenseMessage {
    byte nodeID; // ID number of node
    byte messageCount; // # messages sent
    word sense0; // Sensor one data
    word sense1; // Sensor two data
    word sense2; // Sensor three data
    word sense3; // Sensor four data
    byte padding11;
    byte padding12;
    byte padding13;
    byte padding14;
    byte padding15;
    byte padding16;
    byte padding17;
    byte padding18;
    byte padding19;
    byte padding20;
    byte padding21;
    byte padding22;
    byte padding23;
    byte padding24;
    byte padding25;
    byte padding26;
    byte padding27;
    byte padding28;
    byte padding29;
    byte padding30;
};

/* Global Variables */
byte threshold = 0;
volatile struct SenseMessage messageBuf= {0};

/* Functions */

void makeMessage(byte nodeID, byte messageCount,
                 word sense0, word sense1,
                 word sense2, word sense3) {
    byte *bytep = (byte *) &messageBuf;

    /* Fill entire struct with padding*/
    for (int counter=0; counter++; counter < 30) {
        bytep[counter] = padding;
    }
    messageBuf.nodeID = nodeID;
    messageBuf.messageCount = messageCount;
    messageBuf.sense0 = sense0;
    messageBuf.sense1 = sense1;
    messageBuf.sense2 = sense2;
    messageBuf.sense3 = sense3;
}

void statusEvent(TimerInformation *Sender) {
  unsigned long currentTime = millis();
  int hours = currentTime / (1000 * 60 * 60);
  int minutes = (currentTime / (1000 * 60)) - (hours * 60);
  int seconds = (currentTime / 1000) - (minutes * 60) - (hours * 60 * 60);

  Serial.print(hours); Serial.print(":");
  Serial.print(minutes); Serial.print(":");
  Serial.print(seconds); Serial.print(" ");
  Serial.print("nodeID: 0x"); Serial.print(messageBuf.nodeID, HEX);
  Serial.print("msg #: 0x"); Serial.print(messageBuf.messageCount, HEX);
  Serial.print("  0x"); Serial.print(messageBuf.sense0, HEX);
  Serial.print("  0x"); Serial.print(messageBuf.sense1, HEX);
  Serial.print("  0x"); Serial.print(messageBuf.sense2, HEX);
  Serial.print("  0x"); Serial.print(messageBuf.sense3, HEX);
  Serial.print("  0x"); Serial.print(threshold, HEX);
  Serial.println("");
}

void updateCurrentEvent(TimerInformation *Sender) {
    messageBuf.sense0 = messageBuf.sense1;
    messageBuf.sense1 = messageBuf.sense2;
    messageBuf.sense2 = messageBuf.sense3;
    messageBuf.sense3 = analogRead(currentSensePin);
    threshold = analogRead(thresholdPin);
}

unsigned long senseTotal() {
    return messageBuf.sense0 + messageBuf.sense1 +
           messageBuf.sense2 + messageBuf.sense3;
}

boolean thresholdBreached() {
    if (senseTotal() >= threshold * 3) { // two or more senses over threshold
        return true;
    } else {
        return false;
    }
}

void txEvent(TimerInformation *Sender) {
    digitalWrite(statusLEDPin, HIGH);
    messageBuf.messageCount += 1;
    while (thresholdBreached()) {
        makeMessage(nodeID, messageBuf.messageCount,
                    messageBuf.sense0, messageBuf.sense1,
                    messageBuf.sense2, messageBuf.sense3);
        vw_send((uint8_t *) &messageBuf, sizeof(messageBuf));
        delay(1000);
        vw_wait_tx();
    }
    digitalWrite(statusLEDPin, LOW);
}

/* Main Program */

void setup() {
    Serial.begin(serialBaud);

    // debugging info
    Serial.println("setup()");
    Serial.print(" txDataPin-"); Serial.print(txDataPin);
    vw_set_tx_pin(txDataPin);
    Serial.print(" txPTTPin-"); Serial.print(txPTTPin);
    vw_set_ptt_pin(txPTTPin);
    Serial.print(" statusLEDPin-"); Serial.print(statusLEDPin);
    pinMode(statusLEDPin, OUTPUT);
    Serial.print(" currentSensePin-"); Serial.print(currentSensePin);
    Serial.print(" thresholdPin-"); Serial.print(thresholdPin);
    Serial.println("");
    // no setup needed for analogPin
    Serial.print(" serialBaud-"); Serial.print(serialBaud);
    Serial.print(" txBaud-"); Serial.print(txBaud);
    Serial.print(" senseInterval-"); Serial.print(senseInterval); Serial.print("ms");
    Serial.print(" statusInterval-"); Serial.print(statusInterval); Serial.print("ms");
    Serial.println("");

    // Initialize buffer
    makeMessage(nodeID, 0, 0, 0, 0, 0);    

    // Current Check
    TimedEvent.addTimer(senseInterval, updateCurrentEvent);
    TimedEvent.addTimer(statusInterval, statusEvent);
    TimedEvent.addTimer(txInterval, txEvent);
    vw_setup(txBaud);
    Serial.println();
    Serial.println("loop()");
}

void loop() {
    TimedEvent.loop(); // blocking
}
