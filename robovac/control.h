#ifndef CONTROL_H
#define CONTROL_H

void servoControl(boolean turnOn) {
    if (!monitorMode) {
        if (turnOn == true) {
            digitalWrite(servoPowerControlPin, HIGH);
        } else {
            digitalWrite(servoPowerControlPin, LOW);
        }
    }
}

void vacControl(boolean turnOn) {
    if (!monitorMode) {
        if (turnOn == true) {
            digitalWrite(vacPowerControlPin, HIGH);
        } else {
            digitalWrite(servoPowerControlPin, LOW);
        }
    }
}

void moveServos(unsigned char port_id) {
    // will be called repeatidly until SERVOMOVETIME expires
    // port_id could change on any call
    int nodeCount=0;

    PRINTTIME(millis());
    if (port_id == 0) {
        D("Opening all port doors\n");
    } else { // Close all ports except port_id
        for (nodeCount=0; nodeCount < MAXNODES; nodeCount++) {
            if (nodeInfo[nodeCount].port_id != port_id) {
                D("Closing node_id ");
                D(nodeInfo[nodeCount].node_id);
                D("'s port ");
                D(nodeInfo[nodeCount].port_id);
            } else {
                D("Leaving node_id ");
                D(nodeInfo[nodeCount].node_id);
                D("'s port ");
                D(nodeInfo[nodeCount].port_id);
                D(" open!!!!!!!!\n");
            }
        }
    }
    D("\n");
}

#endif // CONTROL_H
