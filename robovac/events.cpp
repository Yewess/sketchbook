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

#include "events.h"

/* functions */

void actionStateEvent(TimerInformation *Sender) {
    handleActionState();
}

void pollRxEvent(TimerInformation *Sender) {
    boolean CRCGood = false;
    uint8_t buffLen = sizeof(message_t);
    uint8_t *messageBuff = (uint8_t *) &message;

    signalStrength = analogRead(signalStrengthPin);
    if (vw_have_message()) {
        digitalWrite(statusLEDPin, HIGH);
        CRCGood = vw_get_message(messageBuff, &buffLen);
        if ( (CRCGood == false) || (validMessage(&message, currentTime) == false)
                                || (findNode(message.node_id) == NULL) ) {
                blankMessage = true;
#ifdef DEBUG
                PRINTMESSAGE(millis(), message, signalStrength);
                if (CRCGood == false) {
                    Serial.print("Bad CRC ");
                }
                if (validMessage(&message) == false) {
                    Serial.print("Invalid message ");
                }
                if (findNode(message.node_id) == NULL) {
                    Serial.print("Invalid node_id ");
                    Serial.print(message.node_id)
                }
                Serial.println();
#endif // DEBUG
        } else {
            blankMessage = false;
        }
    }
}

void printStatusEvent(TimerInformation *Sender) {
#ifdef DEBUG
    PRINTMESSAGE(millis(), message, signalStrength);
#endif // DEBUG
    digitalWrite(statusLEDPin, LOW);
}

