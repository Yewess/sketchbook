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

#ifndef ROBOVAC_H
#define ROBOVAC_H

#include <Arduino.h>

#define MESSAGEMAGIC 0x9876
#define MESSAGEVERSION 0x01
#define MESSAGESIZE sizeof(message_t)

typedef struct message_s message_t;

void printMessage(const message_t *message);

void makeMessage(message_t *message, byte nodeID);

void copyMessage(message_t *destination, const message_t *source);

boolean validMessage(const message_t *message);

#endif // ROBOVAC_H
