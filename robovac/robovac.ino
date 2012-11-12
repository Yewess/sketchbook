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

/*
    Dependencies:
    ebl-arduino: http://code.google.com/p/ebl-arduino/
    virtual wire: http://www.pjrc.com/teensy/td_libs_VirtualWire.html

    Pitfalls: virtual wire uses Timer1 (can't use PWM on pin 9 & 10
*/

#include <EEPROM.h>
#include <TimedEvent.h>
#include <VirtualWire.h>
#include <RoboVac.h>

/* Externals */

/* Definitions */
#define DEBUG

// Constants used once (to save space)
#define POLLINTERVAL 25 // @ 300 baud, takes 100ms to receive 30 bytes
#define STATEINTERVAL (POLLINTERVAL+3) // update state almost as quickly
#define STATUSINTERVAL 5003 // only used for debugging
#define GOODMSGMIN 3 // Minimum number of good messages in...
#define THRESHOLD 10000 // ..10 second reception threshold, to signal start
#define MAXNODES 10 // number of nodes to keep track of
#define SERVOPOWERTIME 250 // ms to wait for servo's to power up/down
#define SERVOMOVETIME 1000 // ms to wait for servo's to move
#define VACPOWERTIME 2000 // ms to wait for vac to power on

/* I/O constants */
const int statusLEDPin = 13;
const int rxDataPin = 12;
const int signalStrengthPin = A0;
const int servoPowerControlPin = 8;
const int vacPowerControlPin = 9;
/* Global Variables */
message_t message;
nodeInfo_t nodeInfo[MAXNODES];
nodeInfo_t *currentActive = NULL;
nodeInfo_t *lastActive = NULL;
boolean blankMessage = true; // Signal not to read from message
unsigned long lastReception = 0; // millis() a message was last received
unsigned long lastStateChange = 0; // last time state was changed
int signalStrength = 0;
vacstate_t actionState = VAC_SERVOPOSTPOWERUP; // make double-sure all ports open
const char *statusMessage = "\0";

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

#ifdef DEBUG
    Serial.print("Looking for node_id "); Serial.print(node_id);
#endif // DEBUG

    // filter out invalid IDs
    if ((node_id != 255) && (node_id != 0)) {
        for (int nodeCount=0; nodeCount < MAXNODES; nodeCount++) {
            if (nodeInfo[nodeCount].node_id == node_id) {
                result == &nodeInfo[nodeCount];
            }
        }
    }

#ifdef DEBUG
    if (result != NULL) {
        Serial.println(" - found it!");
    } else {
        Serial.println(" - not found!");
    }
#endif // DEBUG

    return result;
}

void setupNodeInfo(void) {
    for (int nodeCount=0; nodeCount < MAXNODES; nodeCount++) {
        nodeInfo[nodeCount].node_id = 0;
        for (int nameChar=0; nameChar < (NODENAMEMAX); nameChar++) {
            nodeInfo[nodeCount].node_name[nameChar] = '\0';
        }
        nodeInfo[nodeCount].port_id = 0;
        nodeInfo[nodeCount].receive_count = 0;
        nodeInfo[nodeCount].new_count = 0;
        nodeInfo[nodeCount].last_heard =0;
    }
}

void readNodeIDServoMap(void) {
    int address = 0;

#ifdef DEBUG
    Serial.print("Reading nodeInfo from EEPROM");
#endif // DEBUG

    for (int nodeCount=0; nodeCount < MAXNODES; nodeCount++) {
        nodeInfo[nodeCount].node_id = EEPROM.read(address);
        if (nodeInfo[nodeCount].node_id == 255) { // uninitialized EEPROM
            nodeInfo[nodeCount].node_id = 0;
        }
        address++;
#ifdef DEBUG
        Serial.print(".");
#endif // DEBUG
        nodeInfo[nodeCount].port_id = EEPROM.read(address);
        if (nodeInfo[nodeCount].port_id == 255) { // uninitialized EEPROM
            nodeInfo[nodeCount].port_id = 0;
        }
        address++;
#ifdef DEBUG
        Serial.print(".");
#endif // DEBUG
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

void printNodeInfo(void) {
    for (int nodeCount=0; nodeCount < MAXNODES; nodeCount++) {
        Serial.print("Node ID: "); Serial.print(nodeInfo[nodeCount].node_id);
        Serial.print(" Port ID: "); Serial.print(nodeInfo[nodeCount].port_id);
        Serial.print(" Messags: "); Serial.print(nodeInfo[nodeCount].receive_count);
        Serial.print(" + Mesgs: "); Serial.print(nodeInfo[nodeCount].new_count);
        Serial.print(" last ms: "); Serial.print(nodeInfo[nodeCount].last_heard);
        Serial.print(" Node Name: '"); Serial.print(nodeInfo[nodeCount].node_name);
        Serial.println("'");
    }
}


void writeNodeIDServoMap(void) {
    int address = 0;

#ifdef DEBUG
    Serial.print("Writing nodeInfo to EEPROM");
#endif // DEBUG

    for (int nodeCount=0; nodeCount < MAXNODES; nodeCount++) {

        // Only write nodeID if it changed
        int currentValue = EEPROM.read(address);
        if (currentValue != nodeInfo[nodeCount].node_id) {
            EEPROM.write(address, nodeInfo[nodeCount].node_id);
            address++;
            delay(4);
#ifdef DEBUG
    Serial.print(".");
#endif // DEBUG
        } else {
            address++;
        }

        // Only write ServoNr if it changed
        currentValue = EEPROM.read(address);
        if (currentValue != nodeInfo[nodeCount].port_id) {
            EEPROM.write(address, nodeInfo[nodeCount].port_id);
            address++;
            delay(4);
#ifdef DEBUG
    Serial.print(".");
#endif // DEBUG
        } else {
            address++;
        }

        // Only write name bytes that changed
        for (int nameChar=0; nameChar < (NODENAMEMAX-1); nameChar++) {
            currentValue = EEPROM.read(address);
            if (currentValue != nodeInfo[nodeCount].node_name[nameChar]) {
                EEPROM.write(address, nodeInfo[nodeCount].node_name[nameChar]);
                address++;
                delay(4);
            } else {
                address++;
            }
        }
        // nodeInfo[nodeCount].node_name[NODENAMEMAX-1] always init to '\0'
#ifdef DEBUG
    Serial.print(".");
#endif // DEBUG
    }
#ifdef DEBUG
    Serial.println();
#endif // DEBUG
}

void servoControl(boolean turnOn) {
    if (turnOn == true) {
        digitalWrite(servoPowerControlPin, HIGH);
    } else {
        digitalWrite(servoPowerControlPin, LOW);
    }
}

void vacControl(boolean turnOn) {
    if (turnOn == true) {
        digitalWrite(vacPowerControlPin, HIGH);
    } else {
        digitalWrite(servoPowerControlPin, LOW);
    }
}

void updateState(vacstate_t newState, unsigned long currentTime) {
    if (newState > VAC_ENDSTATE) {
        actionState = VAC_ENDSTATE;
    }
#ifdef DEBUG
    const char *stateStr;

    printMessage(currentTime);
    PRINTTIME(currentTime);
    STATE2STRING(actionState);
    Serial.print("State Change: "); Serial.print(stateStr);
    STATE2STRING(newState);
    Serial.print(" -> "); Serial.print(stateStr);
    Serial.println();
#endif
    actionState = newState;
    lastStateChange = currentTime;
}


void moveServos(unsigned char port_id) {
    // will be called repeatidly until SERVOMOVETIME expires
    // port_id could change on any call

#ifdef DEBUG
        PRINTTIME(millis());
#endif // DEBUG
    if (port_id == 0) {
#ifdef DEBUG
        Serial.print("Opening all port doors");
#endif // DEBUG
    } else { // Close all ports except port_id
        for (int nodeCount=0; nodeCount < MAXNODES; nodeCount++) {
            if (nodeInfo[nodeCount].port_id != port_id) {
#ifdef DEBUG
                Serial.print("Closing node_id ");
                Serial.print(nodeInfo[nodeCount].node_id);
                Serial.print("'s port ");
                Serial.print(nodeInfo[nodeCount].port_id);
#endif // DEBUG
            } else {
#ifdef DEBUG
                Serial.print("Leaving node_id ");
                Serial.print(nodeInfo[nodeCount].node_id);
                Serial.print("'s port ");
                Serial.print(nodeInfo[nodeCount].port_id);
                Serial.print(" open!!!!!!!!");
#endif // DEBUG
            }
        }
    }
#ifdef DEBUG
    Serial.println();
#endif // DEBUG
}

void handleActionState(TimerInformation *Sender) {
    unsigned long currentTime = millis();

    // Handle overall state
    switch (actionState) {

        case VAC_LISTENING:
            lastActive = currentActive = activeNode(currentTime);
            if (currentActive != NULL) {
                updateState(VAC_VACPOWERUP, currentTime);
            }
            break;

        case VAC_VACPOWERUP:
            currentActive = activeNode(currentTime);
            if (currentActive != NULL) { // node remained active
                vacControl(true); // Power ON
                if (currentTime > (lastStateChange + VACPOWERTIME)) {
                    // powerup finished
                    updateState(VAC_SERVOPOWERUP, currentTime);
                } // else wait some more
            } else { // node shut down during power up
                updateState(VAC_VACPOWERDN, currentTime); // power down
            }
            break;

        case VAC_SERVOPOWERUP:
            currentActive = activeNode(currentTime);
            if (currentActive != NULL) { // node remained active
                servoControl(true); // Power on
                if (currentTime > (lastStateChange + SERVOPOWERTIME)) {
                    // Servo powerup finished
                    updateState(VAC_SERVOACTION, currentTime);
                } // else wait some more
            } else { // node shutdown during servo power up
                updateState(VAC_SERVOPOSTPOWERDN, currentTime);
            }
            break;

        case VAC_SERVOACTION:
            currentActive = activeNode(currentTime);
            if (currentActive != NULL) { // node remained active
                moveServos(currentActive->port_id);
                if ( (lastActive == currentActive) && // node didn't change
                     (currentTime > (lastStateChange + SERVOMOVETIME))) {
                    updateState(VAC_SERVOPOWERDN, currentTime);
                } // not enough time and/or node id changed
            } else { // node shutdown
                updateState(VAC_SERVOSTANDBY, currentTime);
            }
            break;

        case VAC_SERVOPOWERDN:
            currentActive = activeNode(currentTime);
            if (currentActive != NULL) { // node remained active
                servoControl(false); // Power off
                if ( (lastActive == currentActive) && // node didn't change
                     (currentTime > (lastStateChange + SERVOPOWERTIME))) {
                    updateState(VAC_VACUUMING, currentTime);
                } // not enough time
            } else { // node shutdown
                updateState(VAC_SERVOPOSTPOWERUP, currentTime);
            }
            break;

        case VAC_VACUUMING:
            currentActive = activeNode(currentTime);
            if (currentActive != NULL) { // node remained active
                if (lastActive != currentActive) { // node changed!
                    updateState(VAC_SERVOPOWERUP, currentTime);
                } // else keep vaccuuming
            } else { // node shutdown
                updateState(VAC_VACPOWERDN, currentTime);
            }
            break;

        case VAC_VACPOWERDN:
            // ignore any nodes comming online, must go through VAC_SERVOSTANDBY
            // before VAC_VACPOWERUP
            vacControl(false); // Power OFF
            if (currentTime > (lastStateChange + VACPOWERTIME)) {
                // waited long enough
                updateState(VAC_SERVOPOSTPOWERUP, currentTime);
            } // wait longer
            break;

        case VAC_SERVOPOSTPOWERUP:
            // ignore any nodes comming online, must go through VAC_SERVOSTANDBY
            servoControl(true); // Power on
            if (currentTime > (lastStateChange + SERVOPOWERTIME)) {
                // Servo powerup finished
                updateState(VAC_SERVOSTANDBY, currentTime);
            } // else wait some more
            break;

        case VAC_SERVOSTANDBY:
            // ignore any nodes comming online, must go through VAC_SERVOSTANDBY
            moveServos(0); // open all ports
            if (currentTime > (lastStateChange + SERVOMOVETIME)) {
                updateState(VAC_SERVOPOSTPOWERDN, currentTime);
            } // else wait longer
            break;

        case VAC_SERVOPOSTPOWERDN:
            servoControl(false); // Power off
            if (currentTime > (lastStateChange + SERVOPOWERTIME)) {
                updateState(VAC_ENDSTATE, currentTime);
            } // not enough time
            break;

        case VAC_ENDSTATE:
        default:
            // Make sure everything powered off
            servoControl(false); // Power off
            vacControl(false); // Power OFF
            updateState(VAC_LISTENING, currentTime);
            break;
    }
}


void pollRxEvent(TimerInformation *Sender) {
    unsigned long currentTime = millis();
    boolean CRCGood = false;
    uint8_t buffLen = sizeof(message_t);
    uint8_t *messageBuff = (uint8_t *) &message;

    signalStrength = analogRead(signalStrengthPin);
    signalStrength = abs(map(signalStrength, 0, 1023, 100, -100));
    if (vw_have_message()) {
        digitalWrite(statusLEDPin, HIGH);
        CRCGood = vw_get_message(messageBuff, &buffLen);
        if ((CRCGood == false) || (validMessage(&message, currentTime) == false)
            || (findNode(message.node_id) == NULL)) {
                blankMessage = true;
#ifdef DEBUG
            PRINTTIME(currentTime);
            if (CRCGood == false) {
                Serial.print("Bad CRC ");
            }
            if (validMessage(&message, currentTime) == false) {
                Serial.print("Invalid message ");
            }
            if (findNode(message.node_id) == NULL) {
                Serial.print("Node_ID not found");
            }
            Serial.println();
#endif // DEBUG
        } else {
            blankMessage = false;
        }
    }
}


void printMessage(unsigned long currentTime) {
    PRINTTIME(currentTime);
    if (blankMessage == false) {
        Serial.print("Message:");
        Serial.print(" Magic: 0x"); Serial.print(message.magic, HEX);
        Serial.print(" Version: "); Serial.print(message.version);
        Serial.print(" node ID: "); Serial.print(message.node_id);
        Serial.print(" Uptime: ");
        Serial.print(message.up_time); Serial.print("ms ");
    }
}


void printStatusEvent(TimerInformation *Sender) {
#ifdef DEBUG
    unsigned long currentTime = millis();

    printMessage(currentTime);
    Serial.print("Sig. Stren: ");
    Serial.print(int(
        (float(signalStrength) / 1023.0)
        * 100.0
    ));
    Serial.print("% ("); Serial.print(signalStrength);
    Serial.print(")");
    Serial.println();
#endif // DEBUG
    digitalWrite(statusLEDPin, LOW);
}

/* Main Program */

void setup() {

    // debugging info
#ifdef DEBUG
    Serial.begin(SERIALBAUD);
    Serial.println("setup()");
#endif // DEBUG

    // Initialize array
    setupNodeInfo();
    // Load stored map data
    readNodeIDServoMap();
    printNodeInfo();
    // Make sure content matches
    writeNodeIDServoMap();
    printNodeInfo();

    pinMode(rxDataPin, INPUT);
    pinMode(signalStrengthPin, INPUT);
    vw_set_rx_pin(rxDataPin);
    vw_set_ptt_pin(statusLEDPin);
    pinMode(statusLEDPin, OUTPUT);
    digitalWrite(statusLEDPin, LOW);
    vw_setup(RXTXBAUD);
    vw_rx_start();

    // Setup events
    TimedEvent.addTimer(POLLINTERVAL, pollRxEvent);
    TimedEvent.addTimer(STATEINTERVAL, handleActionState);
    TimedEvent.addTimer(STATUSINTERVAL, printStatusEvent);

    // Setup state machine
    lastStateChange = millis();

    // debugging stuff
#ifdef DEBUG
    Serial.print("rxDataPin: "); Serial.print(rxDataPin);
    Serial.print("  statusLEDPin: "); Serial.print(statusLEDPin);
    Serial.print("  SERIALBAUD: "); Serial.print(SERIALBAUD);
    Serial.print("  RXTXBAUD: "); Serial.print(RXTXBAUD);
    Serial.print("\nStat. Int.: "); Serial.print(STATUSINTERVAL);
    Serial.print("ms  Poll Int.: "); Serial.print(POLLINTERVAL);
    Serial.println("ms"); Serial.println("loop()");
#endif // DEBUG
}

void loop() {
    TimedEvent.loop(); // blocking
}
