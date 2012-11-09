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

/* Externals */

/* Definitions */
#define DEBUG

/* I/O constants */
const int statusLEDPin = 13;
const int rxDataPin = 8;

/* comm. constants */
const unsigned int statusInterval = 5003; //milliseconds
const unsigned int pollInterval = 25; // milliseconds
const unsigned int thresholdInterval = 101; // milliseconds
#ifdef DEBUG
const unsigned int serialBaud = 9600;
#endif // DEBUG
const unsigned int rxBaud = 300;
const unsigned int threshold = 10000; // 10 second reception threshold

/* Global Variables */
uint8_t message[VW_MAX_PAYLOAD] = {0};
unsigned long lastReception = 0; // millis() a message was last received
int goodMessageCount = 0;
int badMessageCount = 0;
boolean blankMessage = true;

/* Functions */

#ifdef DEBUG
void printMessage(const uint8_t *message, uint8_t messageLen) {
    Serial.print("Message: ");
    for (char index=0; index < messageLen; index++) {
        Serial.print(message[index], DEC); Serial.print(" ");
    }
    Serial.println();
}
#endif // DEBUG

void copyMessage(uint8_t *destination, uint8_t *source, uint8_t messageLen) {
    for (char index=0; index < messageLen; index++) {
       destination[index] = source[index];
    }  
}

void clearMessage(uint8_t *message, uint8_t messageLen) {
    uint8_t blankMessage[VW_MAX_PAYLOAD] = {0};
    
    copyMessage(message, blankMessage, VW_MAX_PAYLOAD);
}

void pollRxEvent(TimerInformation *Sender) {
    boolean CRCGood = false;
    uint8_t buffLen = VW_MAX_PAYLOAD;
    uint8_t messageBuff[VW_MAX_PAYLOAD] = {0};

    if (vw_have_message()) {
        digitalWrite(statusLEDPin, HIGH);
        CRCGood = vw_get_message(messageBuff, &buffLen);
#ifdef DEBUG
        printMessage(messageBuff, buffLen);
#endif // DEBUG
        if (CRCGood == true) {
            goodMessageCount++;
            copyMessage(message, messageBuff, buffLen);
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

    if (currentTime - lastReception > threshold) {
        if (!blankMessage) {
            clearMessage(message, VW_MAX_PAYLOAD);
            blankMessage = true;
        }
    }
}

#ifdef DEBUG
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
    Serial.print(" \tLast NodeID Received: "); Serial.print(message[0], DEC);
    Serial.print("  Good Messages: "); Serial.print(goodMessageCount);
    Serial.print("  Bad Messages: "); Serial.print(badMessageCount);
    Serial.println("");

    goodMessageCount = 0;
    badMessageCount = 0;
    clearMessage(message, VW_MAX_PAYLOAD);
}
#endif // DEBUG

/* Main Program */

void setup() {
    
    // debugging info
#ifdef DEBUG    
    Serial.begin(serialBaud);
#endif // DEBUG    

    pinMode(rxDataPin, INPUT);
    vw_set_rx_pin(rxDataPin);
    vw_set_ptt_pin(statusLEDPin);
    pinMode(statusLEDPin, OUTPUT);
    digitalWrite(statusLEDPin, LOW);
    vw_setup(rxBaud);
    vw_rx_start();

    // Setup events
    TimedEvent.addTimer(pollInterval, pollRxEvent);
    TimedEvent.addTimer(thresholdInterval, checkThresholdEvent);

#ifdef DEBUG
    TimedEvent.addTimer(statusInterval, printStatusEvent);
    Serial.println("setup()");
    Serial.print("  rxDataPin: "); Serial.print(rxDataPin);
    Serial.print("  statusLEDPin: "); Serial.print(statusLEDPin);
    Serial.print("  serialBaud: "); Serial.print(serialBaud);
    Serial.print("  rxBaud: "); Serial.print(rxBaud);
    Serial.print("\nStat. Int.: "); Serial.print(statusInterval);
    Serial.print("ms  Poll Int.: "); Serial.print(pollInterval);
    Serial.println("ms"); Serial.println("loop()");
#endif // DEBUG
}

void loop() {
    TimedEvent.loop(); // blocking
}
