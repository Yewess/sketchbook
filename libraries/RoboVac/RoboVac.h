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
#define SERIALBAUD 115200
#define NODENAMEMAX 27 // name characters + 1
#define MAXNODES 6 // number of nodes to keep track of
#define NODENAMEMAX (lcdCols + 1) // name characters + 1
#define SERVOPOWERTIME ((unsigned int) 250) // ms to wait for servo's to power up/down
#define SERVOMOVETIME ((unsigned int) 1000) // ms to wait for servo's to move
#define VACPOWERTIME ((unsigned int) 2000) // ms to wait for vac to power on
#define LCDSLEEPTIME ((unsigned int) 180000) // ms to sleep if no activity
#define LCDMENUTIME ((unsigned int) 30000) // ms before menu activity times out
#define LCDBLINKTIME ((unsigned int) 100) // ms to turn backlight off during blink
#define LCDRARROW (B01111110) //  right arrow character
#define LCDLARROW (B01111111) // left arrow character

// constants
const int lcdRows = 2;
const int lcdCols = 16;

// Macros
#define STATE2CASE(VAR, STATE) case STATE: VAR = #STATE; break;

#define STATE2STRING(VAR, STATE) {\
    switch (STATE) {\
        STATE2CASE(VAR, VAC_LISTENING)\
        STATE2CASE(VAR, VAC_VACPOWERUP)\
        STATE2CASE(VAR, VAC_SERVOPOWERUP)\
        STATE2CASE(VAR, VAC_SERVOACTION)\
        STATE2CASE(VAR, VAC_SERVOPOWERDN)\
        STATE2CASE(VAR, VAC_VACUUMING)\
        STATE2CASE(VAR, VAC_VACPOWERDN)\
        STATE2CASE(VAR, VAC_SERVOPOSTPOWERUP)\
        STATE2CASE(VAR, VAC_SERVOSTANDBY)\
        STATE2CASE(VAR, VAC_SERVOPOSTPOWERDN)\
        STATE2CASE(VAR, VAC_ENDSTATE)\
        default: VAR = "MISSING STATE!!!\0"; break;\
    }\
}

#define LCDSTATE2STRING(VAR, STATE) {\
    switch (STATE) {\
        STATE2CASE(VAR, LCD_ACTIVEWAIT)\
        STATE2CASE(VAR, LCD_SLEEPWAIT)\
        STATE2CASE(VAR, LCD_INMENU)\
        STATE2CASE(VAR, LCD_RUNNING)\
        STATE2CASE(VAR, LCD_ENDSTATE)\
        default: VAR = "MISSING STATE!!!\0"; break;\
    }\
}

#ifdef DEBUG
#define D(...) Serial.print(__VA_ARGS__)
#else
#define D(...) {;;}
#endif // DEBUG

#define SP(...) Serial.print(__VA_ARGS__)
#define STOP while (1) {}

#ifdef DEBUG
#define PRINTTIME(MILLIS) {\
    int seconds = (MILLIS / 1000);\
    int millisec = MILLIS % 1000;\
    int minutes = seconds / 60;\
    seconds = seconds % 60;\
    int hours = minutes / 60;\
    minutes = minutes % 60;\
    int days = hours / 24;\
    hours = hours % 24;\
    \
    D(days); D(":");\
    D(hours); D(":");\
    D(minutes); D(":");\
    D(seconds); D(" (");\
    D(MILLIS); D(") ");\
}
#else
#define PRINTTIME(CURRENTTIME) {;;}
#endif // DEBUG

#ifdef DEBUG
#define PRINTMESSAGE(CURRENTTIME, MESSAGE, SIGSTREN) {\
    PRINTTIME(CURRENTTIME);\
    D("Msg:");\
    D(" node ID: "); D(message.node_id);\
    D(" Uptime: ");\
    D(message.up_time); D("ms ");\
    D("Sig. Stren: "); D(SIGSTREN);\
    D("\n");\
}
#else
#define PRINTMESSAGE(CURRENTTIME, MESSAGE, SIGSTREN) {;;}
#endif // DEBUG

typedef struct nodeInfo_s {
    byte node_id; // node ID
    byte port_id; // servo node_id is mapped to
    word servo_min; // minimum limit of travel in 4096ths of 60hz PWM(?)
    word servo_max; // maximum limit of travel
    unsigned int receive_count; // number of messages received in THRESHOLD
    unsigned long last_heard; // timestamp last message was received
    char node_name[NODENAMEMAX]; // name of the node
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

typedef enum lcdState_e {
    LCD_ACTIVEWAIT, // Backlight on, waiting for buttons
    LCD_SLEEPWAIT, // Backlight off, waiting for buttons
    LCD_INMENU, // Backlight on, keep out of running menu
    LCD_RUNNING, // Backlight on, vacuuming, waiting for buttons
    LCD_ENDSTATE, // Go back to LCD_ACTIVEWAIT
} lcdState_t;

typedef struct message_s {
    word magic; // constant 0x42
    byte version; // protocol version
    byte node_id; // node ID
    unsigned long up_time; // number of miliseconds running
} message_t;


// Returns true when finished, false if still running
typedef boolean (*menuEntryCallback_t)(unsigned long *currentTime);

struct menuEntry_s {
    const char name[lcdCols+1]; // Extra space for NULL
    struct menuEntry_s *child;
    struct menuEntry_s *prev_sibling;
    struct menuEntry_s *next_sibling;
    menuEntryCallback_t callback;
};

typedef struct menuEntry_s menuEntry_t;

void makeMessage(message_t *message, byte nodeID);

void copyMessage(message_t *destination, const message_t *source);

boolean validMessage(const message_t *message);

const unsigned long *timerExpired(const unsigned long *currentTime,
                                  const unsigned long *lastTime,
                                  unsigned int interval);

#endif // ROBOVAC_H
