/*
  Analog current sensing and encapsulation w/in message packet

    Copyright (C) 2012 Chris Evich <chris-arduino@anonomail.me>

    This program is free software; you can redistribute it and/or modify
    it under the te of the GNU General Public License as published by
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
    virtual wire: http://www.open.com.au/mikem/arduino/
*/

#define DIGISPARK

#include <VirtualWire.h>
#include <RoboVac.h>

/* I/O constants */
const int txEnablePin = 0;
const int txDataPin = 1;
const int currentSensePin = 0; // Pin 5 == Analog pin 0!!!!
const int overridePin = 3; // has 1.5k pull up allready
const int batteryPin = 1; // Pin 2 == Analog pin 1!!!!
const int floatingPin = 4; // for random seed

/* Global Variables */
VirtualWire VWI;
VirtualWire::Transmitter TX(txDataPin);
message_t message;
unsigned long analogReadMicroseconds = 0; // measured in setup()

void setup() {
    //sleep_disable();

    // override button to ground
    pinMode(overridePin, OUTPUT);
    digitalWrite(overridePin, HIGH); // enable internal pull-up

    pinMode(txDataPin, OUTPUT);
    digitalWrite(txDataPin, LOW);

    pinMode(txEnablePin, OUTPUT);
    digitalWrite(txEnablePin, LOW);

    // Calibrate Analog Read speed (rc timers affected by temperature
    //                              or clock ref changed while sleeping)
    // Capture the start time in microseconds
    unsigned long startTime = (millis() * 1000) + micros();
    // 10,000 analog reads for average (below)
    for (int count = 0; count < 10000; count++) {
        message.threshold = analogRead(currentSensePin);
    }
    // Calculate duration in microseconds
    // divide the duration by number of reads
    analogReadMicroseconds = (((millis() * 1000) + micros()) - startTime) / 10000;

    // installs ISR
    VWI.begin(RXTXBAUD);
    TX.begin();
}

void loop() {
    message.msg_count = analogReadMicroseconds;
    digitalWrite(txEnablePin, HIGH); // light + TX on
    TX.send((uint8_t *) &message, sizeof(message));
    TX.await(); // wait to finish sending
    digitalWrite(txEnablePin, LOW); // light + TX off
    delay(10000);
}
