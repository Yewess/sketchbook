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


#include "nodeinfo.h"

/* Functions */

void updateNodes(unsigned long currentTime) {
    // keep receive_count updated for all nodes

    // for each node, update receive_count by new_count w/in THRESHOLD
    for (int nodeCount=0; nodeCount < MAXNODES; nodeCount++) {
        unsigned long elapsedTime = currentTime - nodeInfo[nodeCount].last_heard;
        unsigned char max_count = THRESHOLD/TXINTERVAL;
        unsigned char expired_count = elapsedTime % (THRESHOLD/GOODMSGMIN);

        // increase rate is capped by max transmit rate during threshold
        nodeInfo[nodeCount].receive_count += nodeInfo[nodeCount].new_count;
        if (nodeInfo[nodeCount].receive_count > max_count) {
            nodeInfo[nodeCount].receive_count = max_count;
#ifdef DEBUG
            PRINTTIME(currentTime);
            Serial.print("Clipped node_id "); Serial.print(nodeInfo[nodeCount].node_id);
            Serial.print(" receive_count to "); Serial.print(max_count);
            Serial.println();
#endif // DEBUG
        }

        // for every (THRESHOLD/GOODMSGMIN) over, reduce count by one
        if (nodeInfo[nodeCount].receive_count > expired_count) {
            nodeInfo[nodeCount].receive_count -= expired_count;
#ifdef DEBUG
            PRINTTIME(currentTime);
            Serial.print("Reduced node_id "); Serial.print(nodeInfo[nodeCount].node_id);
            Serial.print(" receive_count by "); Serial.print(expired_count);
            Serial.println();
#endif // DEBUG

        } else {
            nodeInfo[nodeCount].receive_count = 0; // expired_count was bigger
#ifdef DEBUG
            PRINTTIME(currentTime);
            Serial.print("Reduced node_id "); Serial.print(nodeInfo[nodeCount].node_id);
            Serial.print(" receive_count to 0");
            Serial.println();
#endif // DEBUG
        }

        // Any nodes w/o a receive_count loose last_heard
        if (nodeInfo[nodeCount].receive_count == 0) {
            nodeInfo[nodeCount].last_heard = 0;
#ifdef DEBUG
            PRINTTIME(currentTime);
            Serial.print("Clipped node_id "); Serial.print(nodeInfo[nodeCount].node_id);
            Serial.print(" last_heard to 0");
            Serial.println();
#endif // DEBUG
        }
    }
}

nodeInfo_t *activeNode(unsigned long currentTime) {
    nodeInfo_t *result = NULL;
    // return most recent active node that meets >= GOODMSGMIN in THRESHOLD
    // or NULL if none meet the criteria

    for (int nodeCount=0; nodeCount < MAXNODES; nodeCount++) {
        if (nodeInfo[nodeCount].receive_count >= GOODMSGMIN) {
            if (result != NULL) { // most recent wins
                if (nodeInfo[nodeCount].last_heard > result->last_heard) {
#ifdef DEBUG
                    printMessage(currentTime);
                    PRINTTIME(currentTime);
                    Serial.print("Warning: Node ");
                    Serial.print(nodeInfo[nodeCount].node_id);
                    Serial.print(" is competing with node ");
                    Serial.print(result->node_id);
                    Serial.println();
#endif // DEBUG
                    result = &nodeInfo[nodeCount];
                }
            } else { // first match
                result = &nodeInfo[nodeCount];
            }
        }
    }
    return result;
}

nodeInfo_t *findNode(byte node_id) {
    nodeInfo_t *result = NULL;

    // filter out invalid IDs
    if ((node_id != 255) && (node_id != 0)) {
        for (int nodeCount=0; nodeCount < MAXNODES; nodeCount++) {
            if (nodeInfo[nodeCount].node_id == node_id) {
                result == &nodeInfo[nodeCount];
            }
        }
    }
    return result;
}

void setupNodeInfo(void) {
    for (int nodeCount=0; nodeCount < MAXNODES; nodeCount++) {
        nodeInfo[nodeCount].node_id = 0;
        nodeInfo[nodeCount].port_id = 0;
        nodeInfo[nodeCount].servo_min = servoCenterPW;
        nodeInfo[nodeCount].servo_max = servoCenterPW;
        nodeInfo[nodeCount].receive_count = 0;
        nodeInfo[nodeCount].new_count = 0;
        nodeInfo[nodeCount].last_heard =0;
        for (int nameChar=0; nameChar < (NODENAMEMAX); nameChar++) {
            nodeInfo[nodeCount].node_name[nameChar] = '\0';
        }
    }
}

void printNodeInfo(int index) {
        Serial.print("Node Name: '"); Serial.print(nodeInfo[index].node_name);
        Serial.print("'");
        Serial.print(" Node ID: "); Serial.print(nodeInfo[index].node_id);
        Serial.print(" Port ID: "); Serial.print(nodeInfo[index].port_id);
        Serial.print(" Servo Min: "); Serial.print(nodeInfo[index].servo_min);
        Serial.print(" Servo Max: "); Serial.print(nodeInfo[index].servo_max);
        Serial.print(" Messags: "); Serial.print(nodeInfo[index].receive_count);
        Serial.print(" + Mesgs: "); Serial.print(nodeInfo[index].new_count);
        Serial.print(" last ms: "); Serial.print(nodeInfo[index].last_heard);


void printNodes(void) {
    for (int nodeCount=0; nodeCount < MAXNODES; nodeCount++) {
        printNodeInfo(nodeCount);
        Serial.println();
    }
}

void readNodeIDServoMap(void) {
    int address = 0;

#ifdef DEBUG
    Serial.print("Reading nodeInfo from EEPROM");
#endif // DEBUG

    for (int nodeCount=0; nodeCount < MAXNODES; nodeCount++) {

        // node_id
        nodeInfo[nodeCount].node_id = EEPROM.read(address);
        address++;
        if (nodeInfo[nodeCount].node_id == 255) { // uninitialized EEPROM
            nodeInfo[nodeCount].node_id = 0;
        }
#ifdef DEBUG
        Serial.print(".");
#endif // DEBUG

        // port_id
        nodeInfo[nodeCount].port_id = EEPROM.read(address);
        address++;
        if (nodeInfo[nodeCount].port_id == 255) { // uninitialized EEPROM
            nodeInfo[nodeCount].port_id = 0;
        }
#ifdef DEBUG
        Serial.print(".");
#endif // DEBUG

        // servo_min (word)
        nodeInfo[nodeCount].servo_min = EEPROM.read(address) << 8;
        address++;
        nodeInfo[nodeCount].servo_min |= EEPROM.read(address);
        address++;
        if (nodeInfo[nodeCount].servo_min == 65535) { // uninitialized EEPROM
            nodeInfo[nodeCount].servo_min = servoCenterPW;
        }
#ifdef DEBUG
        Serial.print(".");
#endif // DEBUG

        // servo_max (word)
        nodeInfo[nodeCount].servo_max = EEPROM.read(address) << 8;
        address++;
        nodeInfo[nodeCount].servo_min |= EEPROM.read(address);
        address++;
        if (nodeInfo[nodeCount].servo_max == 65535) { // uninitialized EEPROM
            nodeInfo[nodeCount].servo_max = servoCenterPW;
        }
#ifdef DEBUG
        Serial.print(".");
#endif // DEBUG

        // node_name (NODENAMEMAX-1 bytes)
        for (int nameChar=0; nameChar < (NODENAMEMAX-1); nameChar++) {
            nodeInfo[nodeCount].node_name[nameChar] = EEPROM.read(address);
            address++;
            if (nodeInfo[nodeCount].node_name[nameChar] == 255) {
                // uninitialized EEPROM
                nodeInfo[nodeCount].node_name[nameChar] = '\0';
            }
        }
        nodeInfo[nodeCount].node_name[NODENAMEMAX-1] = '\0'; // not stored
#ifdef DEBUG
        Serial.print(".");
#endif // DEBUG
    }
#ifdef DEBUG
    Serial.println();
#endif // DEBUG
}

void writeNodeIDServoMap(void) {
    int address = 0;
    byte currentByte = 0;
    word currentWord = 0;

#ifdef DEBUG
    Serial.print("Writing nodeInfo to EEPROM");
#endif // DEBUG

    for (int nodeCount=0; nodeCount < MAXNODES; nodeCount++) {

        // Only write nodeID if it changed
        currentByte = EEPROM.read(address);
        if (currentByte != nodeInfo[nodeCount].node_id) {
            EEPROM.write(address, nodeInfo[nodeCount].node_id);
#ifdef DEBUG
            Serial.print(".");
#endif // DEBUG
        }
        address++;

        // Only write port_id if it changed
        currentByte = EEPROM.read(address);
        if (currentByte != nodeInfo[nodeCount].port_id) {
            EEPROM.write(address, nodeInfo[nodeCount].port_id);
#ifdef DEBUG
            Serial.print(".");
#endif // DEBUG
        }
        address++;

        // Only write servo min if it changed
        currentWord = EEPROM.read(address) << 8;
        currentWord |= EEPROM.read(address + 1);
        if (currentWord != nodeInfo[nodeCount].servo_min) {
            EEPROM.write(address, highByte(nodeInfo[nodeCount].servo_min));
            EEPROM.write(address + 1, lowByte(nodeInfo[nodeCount].servo_min));
#ifdef DEBUG
            Serial.print(".");
#endif // DEBUG
        }
        address += 2;

        // Only write Servo max if it changed
        currentWord = EEPROM.read(address) << 8;
        currentWord |= EEPROM.read(address + 1);
        if (currentWord != nodeInfo[nodeCount].servo_max) {
            EEPROM.write(address, highByte(nodeInfo[nodeCount].servo_max));
            EEPROM.write(address, lowByte(nodeInfo[nodeCount].servo_max));
#ifdef DEBUG
            Serial.print(".");
#endif // DEBUG
        }
        address += 2;

        // Only write name bytes that changed
        for (int nameChar=0; nameChar < (NODENAMEMAX-1); nameChar++) {
            currentByte = EEPROM.read(address);
            if (currentByte != nodeInfo[nodeCount].node_name[nameChar]) {
                EEPROM.write(address, nodeInfo[nodeCount].node_name[nameChar]);
                address++;
            }
            address++;
        }
        // nodeInfo[nodeCount].node_name[NODENAMEMAX-1] always init to '\0'
    }
#ifdef DEBUG
    Serial.println();
#endif // DEBUG
}
