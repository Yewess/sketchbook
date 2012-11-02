/*
  Analog current sensing and encapsulation w/in message packet
  
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
*/

#include <TimedEvent.h>
#include <VirtualWire.h>

/* Externals */

/* Definitions */

#define NODEID (0x01) // unique to this node
#define ACHERTZ (60)
#define DATABUFLEN (14) // words
#define SAMPLESPERINTERVAL 10 // number of peak detection measurements
#define TXEVENTID 1 //id number of tx timer

/* Measurement Constants */
const unsigned long acHalfWave = (1000000/(ACHERTZ*2)); // microseconds
/* Offset after all reads so subsequent measurements 
   capture a slightly different waveform. This will
   help highlight flux changes constrained to a specific
   phase of the wave */
const unsigned long asyncOffset = acHalfWave / (SAMPLESPERINTERVAL + 1);

/* I/O constants */
const int statusLEDPin = 13;
const int txEnablePin = 7;
const int txDataPin = 8;
const int currentSensePin = A0;
const int thresholdPin = A1;
const int VCCRefPin = A2;

/* comm. constants */
const unsigned int senseInterval = 200;
const unsigned int txInterval = 501;
const unsigned int serialBaud = 9600;
const unsigned int txBaud = 300;

/* Types */

/* Needs to be exactly 30 bytes*/
struct MessageStruct {
    char nodeID; // ID number of node (byte)
    char messageCount; // # messages sent 
    int data[DATABUFLEN]; /* Must be 30 bytes total*/
};

/* Global Variables */
int threshold = 0; // range: -512 - 512
int VCCRef = 0; // range 0 - 512
volatile struct MessageStruct messageBuf = {0};
unsigned long analogReadMicroseconds = 0;

/* Functions */

void printStatus(void) {
    unsigned long currentTime = millis();
    int seconds = (currentTime / 1000);
    int minutes = seconds / 60;
    seconds = seconds % 60;
    int hours = minutes / 60;
    minutes = minutes % 60;
    int days = hours / 24;
    hours = hours % 24;

    Serial.print(days); Serial.print(":");
    Serial.print(hours); Serial.print(":");
    Serial.print(minutes); Serial.print(":");
    Serial.print(seconds);
    Serial.print("  nodeID: "); Serial.print(messageBuf.nodeID);
    Serial.print("  msg #: "); Serial.print(messageBuf.messageCount);
    Serial.print("  VCCRef: "); Serial.print(VCCRef);
    Serial.print("  Thrsh: "); Serial.print(threshold);
    Serial.print("  Data: ");
    for (char dataCount=DATABUFLEN; dataCount--; dataCount > 0) {
        Serial.print(messageBuf.data[dataCount-1]);
        Serial.print(" ");
    }
    Serial.println("");
}

boolean thresholdBreached() {
    int breachCount=0;

    for (char dataCount=DATABUFLEN; dataCount--; dataCount > 0) {
        if (abs(messageBuf.data[dataCount-1]) > threshold) {
            breachCount++;
        }
    }

    printStatus();

    // One quarter of samples must breach
    if (breachCount > (DATABUFLEN/4)) {
        return true;
    } else {
        return false;
    }
}

void updateCurrentEvent(TimerInformation *Sender) {
    int highRange=0;
    int lowRange=0;
    int currentSample=0;
    int maxSample=-0;

    VCCRef = analogRead(VCCRefPin);
    highRange = VCCRef / 2; // Hall effect sensor reports from -1/2vcc to 1/2vcc
    lowRange = -1 * highRange;
    VCCRef = map(VCCRef, 0, 1023, lowRange, highRange);
    threshold = map(analogRead(thresholdPin), 0, 1023, 0, highRange);

    // Fill data elements
    for (char dataCount=DATABUFLEN; dataCount--; dataCount > 0) {

        // Find peak value
        for (char sampleCount=SAMPLESPERINTERVAL; sampleCount--; sampleCount > 0) {
            currentSample = map(analogRead(currentSensePin),
                                0, 1023,
                                lowRange, highRange);
            // Peak detection
            if (abs(currentSample) > abs(maxSample)) {
                maxSample = currentSample;
            }
            /* delay for one ac half-wave plus small offset so
               same part of wave isn't measured repeatidly. Reduce
               by amount of time it takes to perform an analogRead */
            delayMicroseconds((acHalfWave + asyncOffset) - analogReadMicroseconds);
        }
        messageBuf.data[dataCount-1] = maxSample;
    }

    if (thresholdBreached()) {
        digitalWrite(txEnablePin, HIGH); // turn on transmitter
        digitalWrite(statusLEDPin, HIGH);
        TimedEvent.start(TXEVENTID);
    } else {
        TimedEvent.stop(TXEVENTID);
        digitalWrite(txEnablePin, LOW); // turn off transmitter
        digitalWrite(statusLEDPin, LOW);
    }
}

void txEvent(TimerInformation *Sender) {
    messageBuf.messageCount += 1;
    // Transmit new data only while not transmitting old data
    if ( ! vw_tx_active() ) {
        vw_send((uint8_t *) &messageBuf, sizeof(messageBuf));
    }
}

/* Main Program */

void setup() {
    Serial.begin(serialBaud);
    int outReadCount=100;
    int inrReadCount=100;

    // debugging info
    Serial.println("setup()");
    Serial.print("  txDataPin: "); Serial.print(txDataPin);
    pinMode(txDataPin, OUTPUT);
    vw_set_tx_pin(txDataPin);
    Serial.print("  txEnablePin: "); Serial.print(txEnablePin);
    pinMode(txEnablePin, OUTPUT);
    vw_set_ptt_pin(statusLEDPin);
    Serial.print("  statusLEDPin: "); Serial.print(statusLEDPin);
    pinMode(statusLEDPin, OUTPUT);
    digitalWrite(statusLEDPin, LOW);
    Serial.print("  currentSensePin: "); Serial.print(currentSensePin);
    pinMode(currentSensePin, INPUT);
    Serial.print("  thresholdPin: "); Serial.print(thresholdPin);
    pinMode(thresholdPin, INPUT);
    Serial.print("  VCCRefPin: "); Serial.println(VCCRefPin);
    pinMode(VCCRefPin, INPUT);
    Serial.print("serialBaud: "); Serial.print(serialBaud);
    Serial.print("  txBaud-"); Serial.print(txBaud);
    vw_setup(txBaud);
    Serial.print("  Smp. Per. Intvl: "); Serial.print(SAMPLESPERINTERVAL);
    Serial.print("  senseInterval: "); Serial.print(senseInterval);
    Serial.println("ms");

    Serial.print("analogRead: ");
    // Let analog pins settle while measuring read speed
    analogReadMicroseconds = (millis() * 1000) + micros();
    for (char outer=outReadCount; outer--; outer > 0) {
        for (char inner=inrReadCount; inner--; inner > 0) {
            analogRead(VCCRefPin);
            analogRead(thresholdPin);
            analogRead(currentSensePin);
        }
    }
    analogReadMicroseconds = ((millis() * 1000) + micros()) - analogReadMicroseconds;
    // number of analogReads times number of loops
    analogReadMicroseconds /= (3 * outReadCount * inrReadCount);
    Serial.print(analogReadMicroseconds);
    Serial.print("us  AC 1/2 wave: "); Serial.print(acHalfWave);
    Serial.print("us  asyncOffset: "); Serial.print(asyncOffset);

    // Setup events
    TimedEvent.addTimer(senseInterval, updateCurrentEvent);
    TimedEvent.addTimer(TXEVENTID, txInterval, txEvent);

    // Initialize buffer
    messageBuf.nodeID = NODEID;    

    // Signal completion
    Serial.println("us");
}

void loop() {
    TimedEvent.loop(); // blocking
}
