#ifndef CONTROL_H
#define CONTROL_H

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

void servoControl(boolean turnOn) {
    if (!monitorMode) {
        if (turnOn == true) {
            digitalWrite(servoPowerControlPin, HIGH);
        } else {
            digitalWrite(servoPowerControlPin, LOW);
        }
    }
}

void vacControl(boolean turnOn, boolean ignoreMonitorMode) {
    if (ignoreMonitorMode || !monitorMode) {
        if (turnOn == true) {
            digitalWrite(vacPowerControlPin, HIGH);
            D("The Vav is ON\nr");
        } else {
            digitalWrite(vacPowerControlPin, LOW);
            D("The Vac is OFF\nr");
        }
    }
}

void moveServos(unsigned char port_id) {
    // will be called repeatidly until SERVOMOVETIME expires
    // port_id could change on any call
    int nodeCount=0;
    nodeInfo_t *node;

    for (nodeCount=0; nodeCount < MAXNODES; nodeCount++) {
        node = &nodeInfo[nodeCount];
        if ((port_id == 0) || (node->port_id == port_id)) {
            // OPEN
            pwm.setPWM(node->port_id, 0, node->servo_max);
        } else {
            // CLOSE
            pwm.setPWM(node->port_id, 0, node->servo_min);
        }
    }
}

#endif // CONTROL_H
