#ifndef CONTROL_H
#define CONTROL_H

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

void vacControl(boolean turnOn, boolean ignoreMonitorMode) {
    if (ignoreMonitorMode || !monitorMode) {
        if (turnOn == true) {
            digitalWrite(vacPowerControlPin, HIGH);
        } else {
            digitalWrite(vacPowerControlPin, LOW);
        }
    }
}

void openPort(byte port_id, word servo_max) {
    pwm.setPWM(port_id, 0, servo_max);
}

void closePort(byte port_id, word servo_min) {
    pwm.setPWM(port_id, 0, servo_min);
}

void idlePort(byte port_id) {
    pwm.setPWM(port_id, 0, 0);
}

void openAllPorts(void) {
    for (nodeCount=0; nodeCount < MAXNODES; nodeCount++) {
        nodeInfo_t *node = &nodeInfo[nodeCount];
        openPort(node->port_id, node->servo_max);
    }
}

void closeAllPorts(void) {
    for (nodeCount=0; nodeCount < MAXNODES; nodeCount++) {
        nodeInfo_t *node = &nodeInfo[nodeCount];
        openPort(node->port_id, node->servo_min);
    }
}

void idleAllPorts(void) {
    for (int portCount=0; portCount < MAXPORTS; portCount++) {
        idlePort(portCount);
    }
}

void servoControl(boolean turnOn) {
    if (!monitorMode) {
        if (turnOn == false) {
            idleAllPorts();
        } else {
            openAllPorts();
        }
    }
}

#endif // CONTROL_H
