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

#ifndef NODEINFO_H
#define NODEINFO_H

#include <Arduino.h>
#include <EEPROM.h>
#include <RoboVac.h>
#include "config.h"

/* types */

typedef struct nodeInfo_s {
    byte node_id; // node ID
    byte port_id; // servo node_id is mapped to
    word servo_min; // minimum limit of travel in 4096ths of 60hz PWM(?)
    word servo_max; // maximum limit of travel
    unsigned char receive_count; // number of messages received in THRESHOLD
    unsigned char new_count; // messages just received
    unsigned long last_heard; // timestamp last message was received
    char node_name[NODENAMEMAX]; // name of the node
} nodeInfo_t;

/* Function definitions */

void updateNodes(unsigned long currentTime);
nodeInfo_t *activeNode(unsigned long currentTime);
nodeInfo_t *findNode(byte node_id);
void setupNodeInfo(void);
void printNodeInfo(int index);
void printNodes(void);
void readNodeIDServoMap(void);
void writeNodeIDServoMap(void);

#endif // NODEINFO_H
