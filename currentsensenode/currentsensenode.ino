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

#define DIGISPARK

#include <avr/power.h>
#include <EEPROM.h>
#include <VirtualWire.h>
#include <RoboVac.h>

/* Definitions */
//#define DEBUG
#undef DEBUG
#define OVERRIDEAMOUNT (unsigned long) (60000 * 5) // 5 minutes

// AC Frequency
#define ACHERTZ (60)
// Number of measurements to capture per wavelength
#define SAMPLESPERWAVE (10)
// Wavelength in microseconds
#define ACWAVELENGTH (1000000.0/(ACHERTZ))
// Number of microseconds per sample (MUST be larger than AnalogRead)
#define MICROSPERSAMPLE ((ACWAVELENGTH) / (SAMPLESPERWAVE))

// Alegro A1301EUA-T Linear Hall-effect Sensor output scale
#define MVPERGAUSS 2.5

/* I/O constants */
const int txEnablePin = 0;
const int txDataPin = 1;
const int currentSensePin = 0; // Pin 5 == Analog pin 0!!!!
const int overridePin = 3; // has 1.5k pull up allready
// 3 AAA new about 4.8 volts
// 3 AAA dead about 2 volts
const int batteryPin = 1; // Pin 2 == Analog pin 1!!!!
const int floatingPin = 4; // for random seed

/* Global Variables */
VirtualWire VWI;
VirtualWire::Transmitter TX(txDataPin);
message_t message;
// Measurements show 223microseconds are needed for an analog read
byte analogReadMicroseconds = 223; // measured in setup()
unsigned long currentTime = 0; // current ms
unsigned long overrideStart = 0; // Time when override mode first set
unsigned long overrideInterval = 0;
unsigned long lastTXEvent = 0; // ms since last entered txEvent()

/* Functions */

void ledBlink(unsigned int numberBlinks, unsigned int delayTime) {
    for (; numberBlinks > 0; numberBlinks--) {
        digitalWrite(txEnablePin, HIGH);
        delay(delayTime);
        digitalWrite(txEnablePin, LOW);
        delay(delayTime);
    }
}

#ifdef DEBUG

int getRMSGauss(void) {
    int inst_gauss = analogRead(currentSensePin);
    inst_gauss = map(inst_gauss - 512, // 2.5v == 0 gauss
                     -512, 512,        // +/- magnetic polarity
                     -2500, 2500);     // mV range
    return abs(inst_gauss / 10);
}

#else

int getRMSGauss(void) {
    double sum_squared_miligauss = 0;
    for (byte index=0; index< SAMPLESPERWAVE; index++) {
        int inst_gauss = analogRead(currentSensePin);
        // Scale to miliVolts
        inst_gauss = map(inst_gauss - 512, // 2.5v == 0 gauss
                         -512, 512,        // +/- magnetic polarity
                         -2500, 2500);     // mV range
        // integrate adjustment by Quiescent output voltage (temp. dependant)
        inst_gauss += message.rms_adjust;
        // Sum Square
        sum_squared_miligauss += inst_gauss * inst_gauss;
        // wait for next sample period
        delayMicroseconds(MICROSPERSAMPLE - analogReadMicroseconds);
    }
    // Square-root of the mean
    sum_squared_miligauss = sqrt(sum_squared_miligauss / SAMPLESPERWAVE);
    // Convert mV into Gauss
    sum_squared_miligauss /= MVPERGAUSS;
    return abs(sum_squared_miligauss);
}

#endif // DEBUG

/* Main Program */

void setup() {
    power_usi_disable();

    // override button to ground
    pinMode(overridePin, OUTPUT);
    digitalWrite(overridePin, HIGH); // enable internal pull-up

    pinMode(txDataPin, OUTPUT);
    digitalWrite(txDataPin, LOW);

    pinMode(txEnablePin, OUTPUT);
    digitalWrite(txEnablePin, LOW);

    // setup messaging
    message.magic = MESSAGEMAGIC;
    message.version = MESSAGEVERSION;

    // get node_id;
    message.node_id = EEPROM.read(0);

    delay(500); // Let sensor values settle down

    // Get adjustment value when sensor is Quiescent
    // doesn't seem to work right
    message.rms_adjust = -1 * getRMSGauss(); // adjustment is added

    // must hold button for ~4 seconds, remaining is entropy
    for(int count=0; count < 40; count++) {
        if (!(digitalRead(overridePin) == LOW)) {
            break; // Not in calibration mode
        } else {
            // blink light
            digitalWrite(txEnablePin, !digitalRead(txEnablePin));
        }
        delay(50);
    }

    // Signal to turn load on, capture final sensor reading
    while ((digitalRead(overridePin) == LOW)) {
        message.node_id = 255;
        message.threshold = TRIGGER_DEFAULT;
        digitalWrite(txEnablePin, HIGH);
        // divide by two
        message.rms_gauss = getRMSGauss() >> 1; // threshold is 1/2 measured value
    }
    digitalWrite(txEnablePin, LOW);

    // Entropy added by delay above waiting for override release
    randomSeed(analogRead(floatingPin) + millis());

    while ((message.node_id == 255)) { // new or reset mode
        message.node_id = random(256); // choose a new one
        if (message.rms_gauss > TRIGGER_DEFAULT) {
            message.threshold = message.rms_gauss;
        }
    }

    // only write if changed
    if (message.node_id != EEPROM.read(0)) {
        ledBlink(4, 250);
        EEPROM.write(0, message.node_id);
        EEPROM.write(1, highByte(message.threshold));
        EEPROM.write(2, lowByte(message.threshold));
    } else { // No changes, enter loop()
        ledBlink(2, 250);
        // installs ISR
    }

    // get threshold
    message.threshold = EEPROM.read(1) << 8;
    message.threshold |= EEPROM.read(2);

    // Enter override mode for 1 TX cycle
    lastTXEvent = 0;
    overrideStart = millis();
    overrideInterval = TXINTERVAL * 2;
    VWI.begin(RXTXBAUD);
    TX.begin();
}

void loop() {
    currentTime = millis();

    if (!TX.is_active()) { // turn off TX power
        digitalWrite(txEnablePin, LOW); // light off
    }

    // Check if override is requested
    if ((digitalRead(overridePin) == LOW)) {
        while ((digitalRead(overridePin) == LOW)) { // button pressed
            digitalWrite(txEnablePin, HIGH); // light on
        }
        delay(10); // debounce
        // Check if already in override mode or not
        if (overrideStart == 0) { // Entering override mode first time
            overrideStart = currentTime;
            overrideInterval = OVERRIDEAMOUNT;
        } else { // already in override mode
            overrideInterval += OVERRIDEAMOUNT;
        }
    }

    if (timerExpired(&currentTime, &lastTXEvent, TXINTERVAL)) {

        // Gather data
        power_adc_enable();
        message.battery_mv = map(analogRead(batteryPin), 0, 1023, 0, 5000);
        // Override mode could negate this
        message.rms_gauss = getRMSGauss();
        power_adc_disable();

        // Check if override mode is done
        if (timerExpired(&currentTime, &overrideStart, overrideInterval)) {
            // clear override mode
            overrideStart = 0;
            overrideInterval = 0;
        } else { // Still in override mode
           message.rms_gauss *= -1; // negative signal for override
        }

        // Only record active threshold breaches and override mode
        if ((message.rms_gauss > message.threshold) || // above threshold
            (message.rms_gauss < 0)) { // override mode
            message.msg_count = message.msg_count + 1;
            digitalWrite(txEnablePin, HIGH); // light + TX on
            TX.send((uint8_t *) &message, sizeof(message));
        }

        // Jiggle next transmit time randomly to help with collisions
        lastTXEvent = currentTime -= random(250); // subtract last to increase
    }
}
