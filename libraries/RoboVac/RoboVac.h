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

// Definitions
#define MESSAGEMAGIC 0x9876
#define MESSAGEVERSION 0x01
#define MESSAGESIZE sizeof(message_t)
#define TXINTERVAL 1002 // Miliseconds between transmits
#define RXTXBAUD 300 // Rf Baud
#define SERIALBAUD 9600
#define NODENAMEMAX 27 // name characters + 1

// Macros
#define STATE2CASE(STATE) case STATE: stateStr = #STATE; break;
#define STATE2STRING(STATE) {\
    switch (STATE) {\
        STATE2CASE(VAC_LISTENING)\
        STATE2CASE(VAC_VACPOWERUP)\
        STATE2CASE(VAC_SERVOPOWERUP)\
        STATE2CASE(VAC_SERVOACTION)\
        STATE2CASE(VAC_SERVOPOWERDN)\
        STATE2CASE(VAC_VACUUMING)\
        STATE2CASE(VAC_VACPOWERDN)\
        STATE2CASE(VAC_SERVOPOSTPOWERUP)\
        STATE2CASE(VAC_SERVOSTANDBY)\
        STATE2CASE(VAC_SERVOPOSTPOWERDN)\
        STATE2CASE(VAC_ENDSTATE)\
        default: stateStr = "MISSING STATE!!!\0"; break;\
    }\
}
#define PRINTTIME(CURRENTTIME) {\
    int seconds = (CURRENTTIME / 1000);\
    int millisec = CURRENTTIME % 1000;\
    int minutes = seconds / 60;\
    seconds = seconds % 60;\
    int hours = minutes / 60;\
    minutes = minutes % 60;\
    int days = hours / 24;\
    hours = hours % 24;\
    \
    Serial.print(days); Serial.print(":");\
    Serial.print(hours); Serial.print(":");\
    Serial.print(minutes); Serial.print(":");\
    Serial.print(seconds); Serial.print(" (");\
    Serial.print(CURRENTTIME); Serial.print(") ");\
}

typedef struct message_s {
    word magic; // constant 0x42
    byte version; // protocol version
    byte node_id; // node ID
    unsigned long up_time; // number of miliseconds running
} message_t;

typedef struct nodeInfo_s {
    byte node_id; // node ID
    unsigned char port_id; // servo node_id is mapped to
    char node_name[NODENAMEMAX]; // name of the node
    unsigned char receive_count; // number of messages received in THRESHOLD
    unsigned char new_count; // messages just received
    unsigned long last_heard; // timestamp last message was received
} nodeInfo_t;

typedef enum vacstate_e {
    VAC_LISTENING, // Waiting for Signal
    VAC_VACPOWERUP, // Powering up vacuum
    VAC_SERVOPOWERUP, // Powering up servos
    VAC_SERVOACTION, // Moving Servos
    VAC_SERVOPOWERDN, // Powering down servos
    VAC_VACUUMING, // Waiting for down threshold
    VAC_VACPOWERDN, // Powering down vacuum
    VAC_SERVOPOSTPOWERUP, // Powering up servos again
    VAC_SERVOSTANDBY,     // open all ports
    VAC_SERVOPOSTPOWERDN, // Powering down servos again
    VAC_ENDSTATE, // Return to listening
} vacstate_t;

void makeMessage(message_t *message, byte nodeID);

void copyMessage(message_t *destination, const message_t *source);

boolean validMessage(const message_t *message, unsigned long listen_up_time);

#endif // ROBOVAC_H
