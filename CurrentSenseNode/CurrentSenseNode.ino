/*
  Analog current sensing and encapsulation w/in message packet
  
    Copyright (C) 2012 Chris Evich <chris-arduino@anonomail.me>

    This program is free software; you can redistribute it and/or modify
    it under the te of the GNU General Public License as published by
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
#define DEBUG
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
const int overridePin = 12;

/* comm. constants */
// VW_MAX_PAYLOAD-1 char max  "56789012345678901234567890"
char nodeID[VW_MAX_PAYLOAD] = "56789012345678901234567890";
const unsigned int senseInterval = 101;
const unsigned int txInterval = 502;
const unsigned long overrideAddition = (60000 * 3); // millis added for each press
const unsigned int txBaud = 300;
#ifdef DEBUG
const unsigned int statusInterval = 5003;
const unsigned int serialBaud = 9600;
#endif // DEBUG

/* Global Variables */
unsigned long analogReadMicroseconds = 0; // measured in setup()
int sampleLow = 0;
int sampleHigh = 0;
int sampleRange = 0;
int threshold = 0;
unsigned long overrideTime = 0; // millis when manual override expires

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

boolean thresholdBreached() {
    if (sampleRange >= threshold) {
        overrideTime = 0;
        return true;
    } else { // under threshold
        if (overrideTime > millis()) {
            return true; // still in override
        } else {
            overrideTime = 0;
            return false; // not in override
        }
    }
}

void incOverrideTime(void) {
    unsigned long currentTime = millis();
    // FIXME: This will break if currentMillies wraps during override    
    if (overrideTime > currentTime) {
        // Still override time in timer
        overrideTime += (overrideAddition);
#ifdef DEBUG        
        Serial.print("Override +: ");Serial.println(overrideAddition);
#endif // DEBUG        
    } else if (overrideTime < currentTime) { // less than currentMillis
        // First press
        overrideTime = currentTime + (overrideAddition);
#ifdef DEBUG
        Serial.print("Override by: ");Serial.println(overrideAddition);
#endif // DEBUG
    }
}

void txEvent(TimerInformation *Sender) {
    if (thresholdBreached()) {
        digitalWrite(txEnablePin, HIGH);
        vw_wait_tx(); // Wait for previous tx (not likely)
        vw_send((uint8_t *) nodeID, VW_MAX_PAYLOAD);
    } else {
        digitalWrite(txEnablePin, LOW);
    }
    
    // Check override button during slow event
    if (digitalRead(overridePin) == HIGH) {
        incOverrideTime();
    }
}

#ifdef DEBUG
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
    Serial.print(seconds); Serial.print(" (");
    Serial.print(currentTime); Serial.print(")");
    Serial.print(" \t threshold: "); Serial.print(threshold);
    Serial.print("  SampleHigh: "); Serial.print(sampleHigh);
    Serial.print("  SampleLow: "); Serial.print(sampleLow);
    Serial.print("  SampleRange: "); Serial.print(sampleRange);
    if (overrideTime > currentTime) {
        Serial.print("  Override: "); Serial.print(overrideTime - currentTime);
    }
    Serial.println("");
}
#endif // DEBUG

/* Main Program */

void setup() {
    double startTime = 0;
    double stopTime = 0;
    double duration = 0;
  
    pinMode(txDataPin, OUTPUT);
    vw_set_tx_pin(txDataPin);
    pinMode(txEnablePin, OUTPUT);
    vw_set_ptt_pin(statusLEDPin);
    pinMode(statusLEDPin, OUTPUT);
    digitalWrite(statusLEDPin, LOW);
    pinMode(currentSensePin, INPUT);
    pinMode(overridePin, INPUT);
    vw_setup(txBaud);
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

    // Check Assumptions
    if (analogReadMicroseconds > MICROSPERSAMPLE) {
        while (1) {
#ifdef DEBUG
            Serial.print("Error: AnalogRead speed too slow!");
#else
            digitalWrite(statusLEDPin, HIGH);
            delay(250);
            digitalWrite(statusLEDPin, LOW);
            delay(250);
            digitalWrite(statusLEDPin, HIGH);
            delay(250);
            digitalWrite(statusLEDPin, LOW);
            delay(1000);
#endif // DEBUG
        }
    }

    // Setup events
    TimedEvent.addTimer(senseInterval, updateCurrentEvent);
    TimedEvent.addTimer(txInterval, txEvent);

#ifdef DEBUG
    TimedEvent.addTimer(statusInterval, printStatusEvent);
    Serial.begin(serialBaud);
    Serial.print("setup()"); Serial.print(" Node ID: "); Serial.println(nodeID);
    Serial.print("txDataPin: "); Serial.print(txDataPin);
    Serial.print("  txEnablePin: "); Serial.print(txEnablePin);
    Serial.print("  statusLEDPin: "); Serial.print(statusLEDPin);
    Serial.print("  currentSensePin: "); Serial.print(currentSensePin);
    Serial.print("  overridePin: "); Serial.print(overridePin);
    Serial.print("  txBaud: "); Serial.print(txBaud);
    Serial.println();
    Serial.print("Threshold Limit: "); Serial.print(THRESHOLDLIMIT);
    Serial.print("  Smp. Per. Intvl: "); Serial.print(SAMPLESPERWAVE);
    Serial.print("  Smp. Duration: "); Serial.print(MICROSPERSAMPLE);
    Serial.print("us  analogRead: ");
    Serial.print(analogReadMicroseconds);
    Serial.println("us");
#endif // DEBUG
}

void loop() {
    TimedEvent.loop(); // blocking
}
