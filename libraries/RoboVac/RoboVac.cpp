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

void makeMessage(message_t *message, byte nodeID,
                 msgType_t msgType, int batteryMiliVolts) {
    message->magic = MESSAGEMAGIC;
    message->version = MESSAGEVERSION;
    message->node_id = nodeID;
    message->msgType = (byte)msgType;
    message->up_time = millis();
    message->batteryMiliVolts = batteryMiliVolts;
}

void copyMessage(message_t *destination, const message_t *source) {
    destination->magic = source->magic;
    destination->version = source->version;
    destination->node_id = source->node_id;
    destination->msgType = source->msgType;
    destination->up_time = source->up_time;
    destination->batteryMiliVolts = source->batteryMiliVolts;
}

boolean validMessage(const message_t *message) {
    if (     (message->magic == MESSAGEMAGIC) &&
             (message->version == MESSAGEVERSION) &&
             (message->msgType >= (byte)MSG_HELLO) &&
             (message->msgType < (byte)MSG_MAXTYPE) &&
             (message->node_id > 0) &&
             (message->node_id < 255) &&
             (message->batteryMiliVolts <= 5000) ) {
        return true;
    } else {
        return false;
    }
}

const unsigned long *timerExpired(const unsigned long *currentTime,
                                  const unsigned long *lastTime,
                                  unsigned long interval) {
    static unsigned long elapsedTime=0;

    if (*currentTime < *lastTime) { // currentTime has wrapped
        elapsedTime = ((unsigned long)-1) - *lastTime;
        elapsedTime += *currentTime;
    } else { // no wrap
        elapsedTime = *currentTime - *lastTime;
    }
    if (elapsedTime >= interval) {
        return &elapsedTime;
    } else {
        return (unsigned long *) NULL;
    }
}

char nibbleToHexChar(byte nibble) {
    // range check
    if (nibble < 16) {
        if (nibble > 9) {
            return char((nibble - 10) + 97); // ASCII 'a'
        } else { // hex number
            return char(nibble + 48); // ASCII '0'
        }
    } else {
        return 'X';
    }
}

const char *byteHexString(byte thebyte) {
    static char buf[3] = {'\0'};

    // Convert a node's port ID to hexidecimal 2-char string & return static buf
    buf[0] = nibbleToHexChar((thebyte & 0xF0) >> 4);
    buf[1] = nibbleToHexChar(thebyte & 0x0F);
    return buf;
}

