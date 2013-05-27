/*
  Analog current sensing debug receiver

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

#include <RoboVac.h>
#include "VirtualWire.h"

/* Global Variables */
int dataPin =  5;
message_t message;
VirtualWire VWI;
VirtualWire::Receiver RX(dataPin);
int8_t result = 0;
unsigned long last_msg_cnt = (unsigned long) -1;

void printTime() {
    unsigned long millisec = millis();
    int seconds = (millisec / 1000);
    millisec = millisec % 1000;
    int minutes = seconds / 60;
    seconds = seconds % 60;
    int hours = minutes / 60;
    minutes = minutes % 60;
    int days = hours / 24;
    hours = hours % 24;

    Serial.print(days); Serial.print(":");
    Serial.print(hours); Serial.print(":");
    Serial.print(minutes); Serial.print(":");
    Serial.print(seconds); Serial.print(" (");
    Serial.print(millisec); Serial.print(")");
}

void printMessage() {
    Serial.print(" magic: ");
    Serial.print(message.magic, HEX);
    Serial.print(" version: ");
    Serial.print(message.version);
    Serial.print(" node_id: ");
    Serial.print(message.node_id);
    Serial.print(" msg_count: ");
    Serial.print(message.msg_count);
    Serial.print(" battery_mv: ");
    Serial.print(message.battery_mv);
    Serial.print(" RMS Gauss: ");
    Serial.print(message.rms_gauss);
    Serial.print(" RMS Adjust: ");
    Serial.print(message.rms_adjust);
    Serial.print(" threshold: ");
    Serial.print(message.threshold);
    Serial.print(" Byte Count: ");
    Serial.print(result);
    Serial.println();
}

void setup() {
    pinMode(13, OUTPUT);
    Serial.begin(115200);
    VWI.begin(RXTXBAUD);
    RX.begin();
    Serial.println("Listening for a singal...");
}

void loop() {
    if (RX.available()) {
        result = RX.recv((void*) &message, sizeof(message));
        if (validMessage(&message, &last_msg_cnt, result)) {
            last_msg_cnt = message.msg_count;
            printTime();
            printMessage();
        } else if (result > 0) {
            printTime();
            Serial.print(" INVALID MESSAGE ");
            printMessage();
        }
    }
}
