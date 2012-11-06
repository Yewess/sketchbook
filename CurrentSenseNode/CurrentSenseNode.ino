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
#define DATABUFLEN (14) // words
#define TXEVENTID 1 //id number of tx timer

/* Measurement Constants */

// AC Frequency
#define ACHERTZ (60)
// Number of measurements to capture per wavelength
#define SAMPLESPERWAVE (20)
// Wavelength in microseconds
#define ACWAVELENGTH (1000000.0/float(ACHERTZ))
// Number of microseconds per sample (MUST be larger than AnalogRead)
#define MICROSPERSAMPLE (float(ACWAVELENGTH) / float(SAMPLESPERWAVE))
// Threshold Sensitivity
#define THRESHOLDLIMIT (127)

/* I/O constants */
const int thresholdPin = A1;
const int statusLEDPin = 13;
const int txEnablePin = 7;
const int txDataPin = 8;
const int currentSensePin = A0;

/* comm. constants */
const unsigned int senseInterval = 101;
const unsigned int txInterval = 202;
const unsigned int statusInterval = 5003;
const unsigned int serialBaud = 9600;
const unsigned int txBaud = 300;

/* Global Variables */
unsigned long analogReadMicroseconds = 0; // measured in setup()
int sampleLow = 0;
int sampleHigh = 0;
int sampleRange = 0;
int threshold = 0;
uint8_t nodeID = NODEID;

/* Functions */

void updateCurrentEvent(TimerInformation *Sender) {
    char index=0;
    char sample=0;

    // reset data
    sampleHigh = 0;
    sampleLow = 0;
    sampleRange = 0;

    threshold = map( analogRead(thresholdPin), 0, 1023, 0, THRESHOLDLIMIT);
    // Fill data elements
    for (index=0; index< SAMPLESPERWAVE; index++) {
        sample= map( analogRead(currentSensePin),
                     0, 1023,
                     -512, 512);
        if (sample > sampleHigh) {
            sampleHigh = sample;
        } else if (sample < sampleLow) {
            sampleLow = sample;
        }
        // wait difference between MICROSPERSAMPLE and analogReadMicroseconds
        delayMicroseconds(MICROSPERSAMPLE - analogReadMicroseconds);
    }
    sampleRange = sampleHigh - sampleLow;
}

boolean thresholdRMSBreached() {
    if (sampleRange >= threshold) {
        return true;
    } else {
        return false;
    }
}

void txEvent(TimerInformation *Sender) {
    if (thresholdRMSBreached()) {
        digitalWrite(txEnablePin, HIGH);
        vw_wait_tx(); // Wait for previous tx (not likely)
        vw_send(&nodeID, sizeof(nodeID));
    } else {
        digitalWrite(txEnablePin, LOW);
    }
}


void printStatusEvent(TimerInformation *Sender) {
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
    Serial.print(" \tNode ID: "); Serial.print(nodeID);
    Serial.print("  threshold: "); Serial.print(threshold);
    Serial.print("  SampleHigh: "); Serial.print(sampleHigh);
    Serial.print("  SampleLow: "); Serial.print(sampleLow);
    Serial.print("  SampleRange: "); Serial.print(sampleRange);
    Serial.println("");
    // for debugging receiver
    nodeID++; // for debugging receiver
    // for debugging receiver
}

/* Main Program */

void setup() {
    double startTime = 0;
    double stopTime = 0;
    double duration = 0;
  
    // debugging info
    Serial.begin(serialBaud);
    Serial.println("setup()");
    Serial.print("txDataPin: "); Serial.print(txDataPin);
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
    Serial.print("  txBaud-"); Serial.print(txBaud);
    vw_setup(txBaud);
    Serial.print("\n");
    Serial.print("Threshold Limit: "); Serial.print(THRESHOLDLIMIT);
    Serial.print("  Smp. Per. Intvl: "); Serial.print(SAMPLESPERWAVE);
    Serial.print("  Smp. Duration: "); Serial.print(MICROSPERSAMPLE);
    Serial.print("us  analogRead: ");
    
    // Capture the start time in microseconds
    startTime = (millis() * 1000) + micros();
    for (int counter = 0; counter < 10000; counter++) { // 10,000 analog reads for average (below)
        analogRead(currentSensePin);
    }
    stopTime = (millis() * 1000) + micros();
    // Calculate duration in microseconds
    duration = stopTime - startTime;
    // divide the duration by number of reads
    analogReadMicroseconds = duration / 10000.0;
    Serial.print(analogReadMicroseconds);

    // Check Assumptions
    if (analogReadMicroseconds > MICROSPERSAMPLE) {
        while (1) {
            Serial.print("Error: AnalogRead speed too slow!");
        }
    }

    // Setup events
    TimedEvent.addTimer(senseInterval, updateCurrentEvent);
    TimedEvent.addTimer(statusInterval, printStatusEvent);
    TimedEvent.addTimer(txInterval, txEvent);

    // Signal completion
    Serial.println("us");
}

void loop() {
    TimedEvent.loop(); // blocking
}
