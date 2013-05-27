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

boolean validMessage(const message_t *message,
                     const unsigned long *last_msg_cnt,
                     byte recv_result) {
    if (     (recv_result == sizeof(message_t)) &&
             (message->magic == MESSAGEMAGIC) &&
             (message->version == MESSAGEVERSION) &&
             (message->node_id >= 0) &&
             (message->node_id < 255) &&
             (message->battery_mv <= HIGH_BATTERY_MV) &&
             (message->battery_mv >= DEAD_BATTERY_MV) &&
             (message->msg_count != *last_msg_cnt) &&
             (message->msg_count >= 6) &&
             (message->threshold >= TRIGGER_DEFAULT) &&
             ( (message->rms_gauss >= message->threshold) ||
               (message->rms_gauss < 0) )) {
        return true;
    } else {
        return false;
    }
}

const unsigned long *timerExpired(const unsigned long *currentTime,
                                  const unsigned long *lastTime,
                                  // sometimes this is a macro
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

