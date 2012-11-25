#ifndef NODEINFO_H
#define NODEINFO_H

#include <RoboVac.h>

void updateNodes(unsigned long *currentTime) {
    // keep receive_count updated for all nodes

    // for each node, update receive_count by new_count w/in THRESHOLD
    for (int nodeCount=0; nodeCount < MAXNODES; nodeCount++) {
        const unsigned long *elapsedTime = timerExpired(currentTime,
                                                        &nodeInfo[nodeCount].last_heard,
                                                        1);
        unsigned int max_count = int(THRESHOLD)/int(TXINTERVAL);
        unsigned int expired_count = *elapsedTime % (int(THRESHOLD)/int(GOODMSGMIN));

        // increase rate is capped by max transmit rate during threshold
        nodeInfo[nodeCount].receive_count += nodeInfo[nodeCount].new_count;
        if (nodeInfo[nodeCount].receive_count >= max_count) {
            nodeInfo[nodeCount].receive_count = max_count;
            PRINTTIME(currentTime);
            D("Clipped node_id "); D(nodeInfo[nodeCount].node_id);
            D(" receive_count to "); D(max_count);
            D("\n");
        }

        // for every (THRESHOLD/GOODMSGMIN) over, reduce count by one
        if (nodeInfo[nodeCount].receive_count > expired_count) {
            nodeInfo[nodeCount].receive_count -= expired_count;
            PRINTTIME(currentTime);
            D("Reduced node_id "); D(nodeInfo[nodeCount].node_id);
            D(" receive_count by "); D(expired_count);
            D("\n");

        } else {
            nodeInfo[nodeCount].receive_count = 0; // expired_count was bigger
            PRINTTIME(currentTime);
            D("Reduced node_id "); D(nodeInfo[nodeCount].node_id);
            D(" receive_count to 0");
            D("\n");
        }

        // Any nodes w/o a receive_count loose last_heard
        if (nodeInfo[nodeCount].receive_count == 0) {
            nodeInfo[nodeCount].last_heard = 0;
            PRINTTIME(currentTime);
            D("Clipped node_id "); D(nodeInfo[nodeCount].node_id);
            D(" last_heard to 0");
            D("\n");
        }
    }
}

nodeInfo_t *activeNode(unsigned long *currentTime) {
    nodeInfo_t *result = NULL;
    // return most recent active node that meets >= GOODMSGMIN in THRESHOLD
    // or NULL if none meet the criteria

    for (int nodeCount=0; nodeCount < MAXNODES; nodeCount++) {
        if (nodeInfo[nodeCount].receive_count >= GOODMSGMIN) {
            if (result != NULL) { // most recent wins
                if (nodeInfo[nodeCount].last_heard > result->last_heard) {
                    PRINTMESSAGE(*currentTime, message, signalStrength);
                    PRINTTIME(*currentTime);
                    D("Warning: Node ");
                    D(nodeInfo[nodeCount].node_id);
                    D(" is competing with node ");
                    D(result->node_id);
                    D("\n");
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
        D("Node Name: '"); D(nodeInfo[index].node_name);
        D("'");
        D(" Node ID: "); D(nodeInfo[index].node_id);
        D(" Port ID: "); D(nodeInfo[index].port_id);
        D(" Servo Min: "); D(nodeInfo[index].servo_min);
        D(" Servo Max: "); D(nodeInfo[index].servo_max);
        D(" Messags: "); D(nodeInfo[index].receive_count);
        D(" + Mesgs: "); D(nodeInfo[index].new_count);
        D(" last ms: "); D(nodeInfo[index].last_heard);
}

void printNodes(void) {
#ifdef DEBUG
    for (int nodeCount=0; nodeCount < MAXNODES; nodeCount++) {
        printNodeInfo(nodeCount);
        D("^@");
    }
#endif
}

void readNodeIDServoMap(void) {
    int address = 0;

    D("Reading nodeInfo from EEPROM");

    for (int nodeCount=0; nodeCount < MAXNODES; nodeCount++) {

        // node_id
        nodeInfo[nodeCount].node_id = EEPROM.read(address);
        address++;
        if (nodeInfo[nodeCount].node_id == 255) { // uninitialized EEPROM
            nodeInfo[nodeCount].node_id = 0;
        }
        D(".");

        // port_id
        nodeInfo[nodeCount].port_id = EEPROM.read(address);
        address++;
        if (nodeInfo[nodeCount].port_id == 255) { // uninitialized EEPROM
            nodeInfo[nodeCount].port_id = 0;
        }
        D(".");

        // servo_min (word)
        nodeInfo[nodeCount].servo_min = EEPROM.read(address) << 8;
        address++;
        nodeInfo[nodeCount].servo_min |= EEPROM.read(address);
        address++;
        if (nodeInfo[nodeCount].servo_min == 65535) { // uninitialized EEPROM
            nodeInfo[nodeCount].servo_min = servoCenterPW;
        }
        D(".");

        // servo_max (word)
        nodeInfo[nodeCount].servo_max = EEPROM.read(address) << 8;
        address++;
        nodeInfo[nodeCount].servo_min |= EEPROM.read(address);
        address++;
        if (nodeInfo[nodeCount].servo_max == 65535) { // uninitialized EEPROM
            nodeInfo[nodeCount].servo_max = servoCenterPW;
        }
        D(".");

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
        D(".");
    }
    D("\n");
}

void writeNodeIDServoMap(void) {
    int address = 0;
    byte currentByte = 0;
    word currentWord = 0;

    D("Writing nodeInfo to EEPROM");

    for (int nodeCount=0; nodeCount < MAXNODES; nodeCount++) {

        // Only write nodeID if it changed
        currentByte = EEPROM.read(address);
        if (currentByte != nodeInfo[nodeCount].node_id) {
            EEPROM.write(address, nodeInfo[nodeCount].node_id);
            D(".");
        }
        address++;

        // Only write port_id if it changed
        currentByte = EEPROM.read(address);
        if (currentByte != nodeInfo[nodeCount].port_id) {
            EEPROM.write(address, nodeInfo[nodeCount].port_id);
            D(".");
        }
        address++;

        // Only write servo min if it changed
        currentWord = EEPROM.read(address) << 8;
        currentWord |= EEPROM.read(address + 1);
        if (currentWord != nodeInfo[nodeCount].servo_min) {
            EEPROM.write(address, highByte(nodeInfo[nodeCount].servo_min));
            EEPROM.write(address + 1, lowByte(nodeInfo[nodeCount].servo_min));
            D(".");
        }
        address += 2;

        // Only write Servo max if it changed
        currentWord = EEPROM.read(address) << 8;
        currentWord |= EEPROM.read(address + 1);
        if (currentWord != nodeInfo[nodeCount].servo_max) {
            EEPROM.write(address, highByte(nodeInfo[nodeCount].servo_max));
            EEPROM.write(address, lowByte(nodeInfo[nodeCount].servo_max));
            D(".");
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
    D("\n");
}

#endif // NODEINFO_H
