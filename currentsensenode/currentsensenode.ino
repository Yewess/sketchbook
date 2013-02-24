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

#include <EEPROM.h>
#include <VirtualWire.h>
#include <RoboVac.h>

/* Definitions */
#undef DEBUG

// Constants used once (to save space)
#define SENSEINTERVAL 101
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
const int overridePin = 3; // has 1.5k pull up allready
const int batteryPin = 2;
const int floatingPin = 4; // for random seed

/* Global Variables */
byte nodeID = 255;
message_t message;
unsigned long analogReadMicroseconds = 0; // measured in setup()
int sampleLow = 0;
int sampleHigh = 0;
int sampleRange = 0;
int threshold = 0;
unsigned long txInterval = long(TXINTERVAL);
unsigned long currentTime = 0; // current ms
unsigned long overrideEnter = 0; // Time when override mode first set
unsigned long lastOvertide = 0; // first time override button pressed
unsigned long overrideInterval = 0;
unsigned long lastTXEvent = 0; // ms since last entered txEvent()
unsigned long lastCurrentEvent = 0; // ms since last entered updateCurrentEvent()
boolean randomSeeded = false;
boolean overrideMode = false;

/* Functions */

// logic is inverted b/c internal pull-up is used
boolean overridePressed(void) {
    if (digitalRead(overridePin) == HIGH) {
        return false;
    } else {
        return true;
    }
}

byte getRandomByte(void) {
    if (!randomSeeded) {
        delay(1);
        randomSeed(analogRead(floatingPin) + ((millis() * 1000) + micros()));
    }
    return random(256);
}

void txEvent(void) {
    if (thresholdBreached()) {
        digitalWrite(txEnablePin, HIGH);
        makeMessage(&message, nodeID);
        vw_send((uint8_t *) &message, MESSAGESIZE);
        PRINTMESSAGE(millis(), message, 0);
    } else {
        digitalWrite(txEnablePin, LOW);
    }

    // Check override button during slow event
    if (overridePressed) {
        incOverrideTime(); // event timer does debounce
    }
}

int updateCurrentEvent(void) {
    int sample=0;

    // Fill data elements
    for (byte index=0; index< SAMPLESPERWAVE; index++) {
        sample = analogRead(currentSensePin); // 512 == 0 volts
        if (sample >= sampleHigh) {
            sampleHigh = sample;
        } else if (sample <= sampleLow) {
            sampleLow = sample;
        }
        // wait difference between MICROSPERSAMPLE and analogReadMicroseconds
        delayMicroseconds(MICROSPERSAMPLE - analogReadMicroseconds);
    }
    return sampleHigh - sampleLow;
}

void overrideCancel(void) {
    overrideMode = false;
    overrideEnter = 0;
    overrideInterval = 0;
}

boolean thresholdBreached() {
    if (sampleRange >= threshold) {
        overrideCancel();
        return true;
    } else { // under threshold
        if (overrideMode) {
            return true;
        } else { // not in override
            return false;
        }
    }
}

void incOverrideTime() {
    // Check if already in override mode or not
    if  ((overrideMode == false) &&
         (overrideEnter == 0) &&
         (overrideInterval == 0)) {
        // Entering override mode
        overrideEnter = millis();
        overrideInterval = OVERRIDEAMOUNT;
        overrideMode = true;
    } else {
        // already in overrde, add more time
        overrideInterval += OVERRIDEAMOUNT;
    }
    // signal to user
    oneBlink();
}

void calibrateThreshold(void) {
    for(int count=0; count < 100; count++) {
        threshold = updateCurrentEvent();
    }
    // 256 signals a calibration error
    threshold = (int) constrain((unsigned long) threshold * 5,
                                (unsigned long) 5, // force function
                                (unsigned long) 256);
    // reset reading for next time
    sampleHigh = 0;
    sampleLow = 0;
}

void calibrateAnalogRead(void) {
    unsigned long startTime = 0;
    unsigned long stopTime = 0;

    // Calibrate read speed
    // Capture the start time in microseconds
    startTime = (millis() * 1000) + micros();
    // 10,000 analog reads for average (below)
    for (int counter = 0; counter < 10000; counter++) {
        threshold = analogRead(currentSensePin);
    }
    stopTime = (millis() * 1000) + micros();
    // Calculate duration in microseconds
    // divide the duration by number of reads
    analogReadMicroseconds = (stopTime - startTime) / 10000;
}

void fourBlinks(void) { // 4 blinks & 1 second delay
    for (byte count=0; count < 4; count++) {
        digitalWrite(txEnablePin, HIGH);
        delay(150);
        digitalWrite(txEnablePin, LOW);
        delay(100);
    }
}

void oneBlink(void) { // 1 blink & 1 second delay
    digitalWrite(txEnablePin, HIGH);
    delay(500);
    digitalWrite(txEnablePin, LOW);
    delay(500);
}

void checkReset(void) {
    // must hold button for ~4 seconds, remaining is entropy
    for(int count=0; count < 40; count++) {
        if (!overridePressed()) {
            break;
        } else {
            // blink light
            digitalWrite(txEnablePin, !digitalRead(txEnablePin));
        }
        delay(100);
    }
    // signal reset mode entered & add entropy
    while (overridePressed()) {
        nodeID = 0; // reset signal
        digitalWrite(txEnablePin, HIGH);
        delay(50);
    }
    digitalWrite(txEnablePin, LOW);
}

void setNodeID(void) {
    // get currently set value (255 is default)
    nodeID = EEPROM.read(0);

    checkReset(); // sets nodeID == 0 on reset

    while ((nodeID == 255) || (nodeID == 0)) {
        nodeID = getRandomByte();
    }
    // only write if changed
    if (nodeID != EEPROM.read(0)) {
        fourBlinks();
        EEPROM.write(0, nodeID);
    }
}

/* Main Program */

void setup() {
    // override button to ground (needs internal pull-up)
    pinMode(overridePin, OUTPUT);
    digitalWrite(overridePin,, HIGH);

    pinMode(floatingPin, INPUT);
    pinMode(currentSensePin, INPUT);

    pinMode(txDataPin, OUTPUT);
    digitalWrite(txDataPin, LOW);
    vw_set_tx_pin(txDataPin);

    pinMode(txEnablePin, OUTPUT);
    digitalWrite(txEnablePin, LOW);
    vw_set_ptt_pin(txEnablePin);

    setNodeID();

    calibrateAnalogRead();

    // Check Assumptions
    if ((threshold == 256) || (analogReadMicroseconds > MICROSPERSAMPLE)) {
        while (1) {
            // signal error, MICROSPERSAMPLE is too small!
            fourBlinks();
        }
    }

    calibrateThreshold();

    // installs ISR
    vw_setup(RXTXBAUD);
}

void loop() {
    currentTime = millis();
    if (timerExpired(&currentTime, &lastCurrentEvent, SENSEINTERVAL)) {
        // clear previous data
        sampleRange = updateCurrentEvent();
        lastCurrentEvent = currentTime;
    }
    if (timerExpired(&currentTime, &lastTXEvent, txInterval)) {
        txEvent(); // Also check override button press
        // jiggle lastTXEvent +/- 255ms to help with collisions
        lastTXEvent = getRandomByte(); // borrow this for a random even/odd
        if ((lastTXEvent % 2) == 0) { // add time
            lastTXEvent = currentTime - lastTXEvent;
        } else {
            lastTXEvent = currentTime + lastTXEvent;
        }
        // reset reading for next time
        sampleHigh = 0;
        sampleLow = 0;
    }
    if (timerExpired(&currentTime, &overrideEnter, overrideInterval)) {
        overrideCancel();
    }
}
