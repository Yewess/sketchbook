/*
  Library header for wireless sensor controlled vacuum port controller

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

#include <Arduino.h>
#include <RoboVac.h>

struct message_s {
    word magic; // constant 0x42
    byte version; // protocol version
    byte node_id; // node ID
    word up_days; // number of days running
    byte up_hours; // number of hours running
    byte up_minutes; // number of minutes running
    byte up_seconds; // number of seconds running
    word up_millis; // number of miliseconds running
};

const word message_size = sizeof(message_t);

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

void makeMessage(message_t *message, byte nodeID) {
    unsigned long currentTime = millis();
    int seconds = (currentTime / 1000);
    int millisec = currentTime % 1000;
    int minutes = seconds / 60;
    seconds = seconds % 60;
    int hours = minutes / 60;
    minutes = minutes % 60;
    int days = hours / 24;
    hours = hours % 24;

    message->magic = MESSAGEMAGIC;
    message->version = MESSAGEVERSION;
    message->node_id = nodeID;
    message->up_millis = word(millisec);
    message->up_seconds = byte(seconds);
    message->up_minutes = byte(minutes);
    message->up_hours = byte(seconds);
    message->up_days = byte(days);
}

void copyMessage(message_t *destination, const message_t *source) {
    destination->magic = source->magic;
    destination->version = source->version;
    destination->node_id = source->node_id;
    destination->up_millis = source->up_millis;
    destination->up_seconds = source->up_seconds;
    destination->up_minutes = source->up_minutes;
    destination->up_hours = source->up_hours;
    destination->up_days = source->up_days;
}

boolean validMessage(const message_t *message) {
    if (     (message->magic == MESSAGEMAGIC) &&
             (message->version == MESSAGEVERSION) &&
             (message->node_id > 0) &&
             (message->up_millis < 1000) &&
             (message->up_seconds < 60) &&
             (message->up_minutes < 60) &&
             (message->up_hours < 24) &&
             (message->up_days < 50) ) {
        return true;
    } else {
        return false;
    }
}
