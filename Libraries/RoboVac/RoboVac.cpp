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
#include "RoboVac.h"

void makeMessage(message_t *message, byte nodeID) {
    message->magic = MESSAGEMAGIC;
    message->version = MESSAGEVERSION;
    message->node_id = nodeID;
    message->up_time = millis();
}

void copyMessage(message_t *destination, const message_t *source) {
    destination->magic = source->magic;
    destination->version = source->version;
    destination->node_id = source->node_id;
    destination->up_time = source->up_time;
}

boolean validMessage(const message_t *message, unsigned long listen_up_time) {
    if (     (message->magic == MESSAGEMAGIC) &&
             (message->version == MESSAGEVERSION) &&
             (message->node_id > 0) &&
             (message->node_id < 255) &&
             (message->up_time < listen_up_time) ) {
        return true;
    } else {
        return false;
    }
}
