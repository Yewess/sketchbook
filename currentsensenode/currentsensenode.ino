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
    virtual wire: http://www.open.com.au/mikem/arduino/
*/

#include <VirtualWire.h>
#include <RoboVac.h>

/* Definitions */
#define NODEID 0x01
#undef DEBUG

// Constants used once (to save space)
#define SENSEINTERVAL 101
#define STATUSINTERVAL 6003
#define OVERRIDEAMOUNT (60000 * 3)

// AC Frequency
#define ACHERTZ (60)
// Number of measurements to capture per wavelength
#define SAMPLESPERWAVE (20)
// Wavelength in microseconds
#define ACWAVELENGTH (1000000.0/(ACHERTZ))
// Number of microseconds per sample (MUST be larger than AnalogRead)
#define MICROSPERSAMPLE ((ACWAVELENGTH) / (SAMPLESPERWAVE))

/* Types */

/* I/O constants */
const int txEnablePin = 1;
const int txDataPin = 0;
const int currentSensePin = 5;
const int overridePin = 2;

/* Global Variables */
message_t message;
unsigned long analogReadMicroseconds = 0; // measured in setup()
int sampleLow = 0;
int sampleHigh = 0;
int sampleRange = 0;
int threshold = 0;
unsigned long txInterval = long(TXINTERVAL);
unsigned long currentTime = 0; // current ms
unsigned long overrideTime = 0; // current Interval of override mode
unsigned long lastOvertide = 0; // first time override button pressed
unsigned long lastTXEvent = 0; // ms since last entered txEvent()
unsigned long lastCurrentEvent = 0; // ms since last entered updateCurrentEvent()

/* Functions */

void txEvent(void) {
    if (thresholdBreached()) {
        digitalWrite(txEnablePin, HIGH);
        makeMessage(&message, byte(NODEID));
        vw_send((uint8_t *) &message, MESSAGESIZE);
        PRINTMESSAGE(millis(), message, 0);
    } else {
        digitalWrite(txEnablePin, LOW);
    }

    // Check override button during slow event
    if (digitalRead(overridePin) == HIGH) {
        incOverrideTime();
    }
}

void updateCurrentEvent(void) {
    char index=0;
    char sample=0;

    // reset data
    sampleHigh = 0;
    sampleLow = 0;
    sampleRange = 0;

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
        overrideTime = 0; // Cancel override mode
        lastOvertide = 0;
        return true;
    } else { // under threshold
        if (overrideTime > 0) { // in override
            // update override timer
            if (timerExpired(&currentTime, &lastOvertide, overrideTime)) {
                // override mode finished
                overrideTime = 0;
                lastOvertide = 0;
                return false;
            } else {
                // in override mode
                return true;
            }
        } else { // not in override
            overrideTime = 0;
            lastOvertide = 0;
            return false;
        }
    }
}

void incOverrideTime(void) {
    // Check if already in override mode or not
    if (overrideTime > 0) {
        // In overrdie, add more.
        overrideTime += OVERRIDEAMOUNT;
    } else {
        // not in override
        overrideTime = OVERRIDEAMOUNT; // enter override
        // Remember when this happened
        lastOvertide = currentTime;
    }
    // Signal to user time incremented
    digitalWrite(txEnablePin, HIGH);
    delay(txInterval / 2);
    digitalWrite(txEnablePin, LOW);
}

/* Main Program */

void setup() {
    double startTime = 0;
    double stopTime = 0;
    double duration = 0;

    pinMode(txDataPin, OUTPUT);
    digitalWrite(txDataPin, LOW);
    vw_set_tx_pin(txDataPin);

    pinMode(txEnablePin, OUTPUT);
    digitalWrite(txEnablePin, LOW);
    vw_set_ptt_pin(txEnablePin);

    pinMode(overridePin, INPUT);

    pinMode(currentSensePin, INPUT);

    vw_setup(RXTXBAUD);

    // Calibrate read speed
    // Capture the start time in microseconds
    startTime = (millis() * 1000) + micros();
    for (int counter = 0; counter < 10000; counter++) { // 10,000 analog reads for average (below)
        threshold = analogRead(currentSensePin);
        if (threshold > sampleHigh) {
            sampleHigh = constrain(threshold, 0, 127);
        }
    }
    stopTime = (millis() * 1000) + micros();
    // Calculate duration in microseconds
    duration = stopTime - startTime;
    // divide the duration by number of reads
    analogReadMicroseconds = duration / 10000.0;

    // set threshold to 10x highest value read (min of 10)
    threshold = constrain(sampleHigh * 10, 10, 256);

    // Check Assumptions
    if (analogReadMicroseconds > MICROSPERSAMPLE) {
        while (1) {
            // signal error, MICROSPERSAMPLE is too small!
            digitalWrite(txEnablePin, HIGH);
            delay(250);
            digitalWrite(txEnablePin, LOW);
            delay(250);
            digitalWrite(txEnablePin, HIGH);
            delay(250);
            digitalWrite(txEnablePin, LOW);
            delay(1000);
        }
    }
}

void loop() {
    currentTime = millis();
    // Jiggle txInterval slightly to help avoid collisions
    if (currentTime % 2) {
        txInterval += TXINTERVAL / 2; // half as long more
    } else {
        txInterval = TXINTERVAL;
    }
    if (timerExpired(&currentTime, &lastCurrentEvent, SENSEINTERVAL)) {
        updateCurrentEvent();
    }
    if (timerExpired(&currentTime, &lastTXEvent, txInterval)) {
        txEvent();
    }
}
