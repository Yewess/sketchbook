#ifndef NODEINFO_H
#define NODEINFO_H

#include <RoboVac.h>

// keep receive_count updated for all nodes
void updateNodes(unsigned long *currentTime) {
    const unsigned long *elapsedTime;
    int expired_count;
    int max_count = (THRESHOLD/TXINTERVAL);
    nodeInfo_t *node;

    // for each node, update receive_count to last max_count w/in THRESHOLD
    for (byte nodeCount=0; nodeCount < MAXNODES; nodeCount++) {

        node = &(nodeInfo[nodeCount]);

        // don't bother with nodes having no messages or last_heard
        if ( (node->receive_count == 0) ||
             (currentTime == NULL) ||
             (node->last_heard == 0 )) {
            node->receive_count = 0;
            node->last_heard = 0;
            continue;
        }

        // Obtain elapsed time since last_heard
        elapsedTime = timerExpired(currentTime, &node->last_heard, 1);

        if ((elapsedTime) && (*elapsedTime > THRESHOLD)) {
            // For every ms over THRESHOLD taken to receive minimum message
            // not less than 0
            expired_count = constrain(
                (*elapsedTime - THRESHOLD) / (THRESHOLD / GOODMSGMIN),
                0,
                max_count);
            if (node->receive_count >= expired_count) {
                node->receive_count = max_count - expired_count;
            } else {
                node->receive_count = 0;
            }
        }

        // Clip top
        node->receive_count = constrain(
                        node->receive_count,
                        0,
                        int(THRESHOLD)/int(TXINTERVAL));

        // Nodes not heard from in 10x threshold get zerod last_heard
        // Any nodes w/o a receive_count lose last_heard
        if ((elapsedTime) && (*elapsedTime > (THRESHOLD * 10))) {
            node->last_heard = 0;
            node->receive_count = 0;
        }
    }
}

nodeInfo_t *activeNode(unsigned long *currentTime, boolean ignore_receive_count) {
    nodeInfo_t *result = NULL;
    // return most recent active node
    // or NULL if none meet the criteria

    for (int nodeCount=0; nodeCount < MAXNODES; nodeCount++) {
        if (nodeInfo[nodeCount].last_heard == 0) {
            continue;
        }
        if (ignore_receive_count || (nodeInfo[nodeCount].receive_count >= GOODMSGMIN)) {
            if (result) { // most recent wins
                if (nodeInfo[nodeCount].last_heard < result->last_heard) {
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
    // filter out invalid IDs
    if ((node_id != 255) && (node_id != 0)) {
        for (byte nodeCount=0; nodeCount < MAXNODES; nodeCount++) {
            if (nodeInfo[nodeCount].node_id == node_id) {
                return &(nodeInfo[nodeCount]);
            }
        }
    }
    return NULL;
}

void setupNodeInfo(void) {
    for (int nodeCount=0; nodeCount < MAXNODES; nodeCount++) {
        nodeInfo[nodeCount].node_id = 0;
        nodeInfo[nodeCount].port_id = 0;
        nodeInfo[nodeCount].servo_min = servoCenterPW;
        nodeInfo[nodeCount].servo_max = servoCenterPW;
        nodeInfo[nodeCount].receive_count = 0;
        nodeInfo[nodeCount].last_heard = 0;
        for (int nameChar=0; nameChar < (NODENAMEMAX); nameChar++) {
            nodeInfo[nodeCount].node_name[nameChar] = '\0';
        }
    }
}


void printNodeInfo(nodeInfo_t *node) {
        D(node->node_name);
        D(" #: "); D(node->node_id);
        D(" Port: "); D(node->port_id);
        D(" Min: "); D(node->servo_min);
        D(" Max: "); D(node->servo_max);
        D(" Msgs: "); D(node->receive_count);
        D(" last ms: "); D(node->last_heard);
        D("\n");
}

void printNodes(void) {
    for (byte nodeCount=0; nodeCount < MAXNODES; nodeCount++) {
        if (nodeInfo[nodeCount].last_heard > 0) {
            printNodeInfo(&(nodeInfo[nodeCount]));
        }
    }
}

void readNodeIDServoMap(void) {
    int address = 0;

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
        if ((nodeInfo[nodeCount].servo_min <= servoMinPW) ||
            (nodeInfo[nodeCount].servo_min >= servoMaxPW) ) { // uninitialized EEPROM
            nodeInfo[nodeCount].servo_min = servoCenterPW;
        }
        D(".");

        // servo_max (word)
        nodeInfo[nodeCount].servo_max = EEPROM.read(address) << 8;
        address++;
        nodeInfo[nodeCount].servo_min |= EEPROM.read(address);
        address++;
        if ((nodeInfo[nodeCount].servo_max <= servoMinPW) ||
            (nodeInfo[nodeCount].servo_max >= servoMaxPW) ) { // uninitialized EEPROM
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
