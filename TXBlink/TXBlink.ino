/*
  Transmit patterns on WRL-10534 based 433Mhz transmitter

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

/* Requires ebl-arduino: http://code.google.com/p/ebl-arduino/ */
#include <TimedEvent.h>

/* Externals */

/* I/O constants */
const int statusLEDPin = 13;
const int potPin = A1;
const int txPin = 8;

/* comm. constants */
const byte syncPreamble = B01010101;
const word serialBaud = 9600;

/* Types */

/* Globals */

unsigned int potPos = 0; /* Hz*/
unsigned int ms = 0;
unsigned int us = 0;

/* Functions */

void updatePotPos(TimerInformation *Sender) {
    potPos = map(analogRead(potPin), 0, 1023, 1, 1000);
    /*
        1000/Hz  == miliseconds
        1000000/Hz == microseconds
        but integer math is faster
    */
    ms = 1000 / potPos;
    us = (1000000 / potPos) - (ms * 1000);
}

void outputCycle() {
    digitalWrite(txPin, HIGH);
    delay(ms);
    delayMicroseconds(us);
    digitalWrite(txPin, LOW);
    delay(ms);
    delayMicroseconds(us);
}

void printFreq(TimerInformation *Sender) {
    Serial.print("Frequency: "); Serial.print(potPos);
    Serial.print("Hz ");
    Serial.print(ms); Serial.print("."); Serial.print(us);
    Serial.println("ms");
}

/* Main Program */

void setup() {
    pinMode(statusLEDPin, OUTPUT);
    pinMode(txPin, OUTPUT);
    Serial.begin(serialBaud);

    TimedEvent.addTimer(10, updatePotPos);
    //TimedEvent.addTimer(2345, printFreq);
    Serial.println("Starting loop()");
}

void loop() {
    TimedEvent.loop();
    outputCycle();
}
