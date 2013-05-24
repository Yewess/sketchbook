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

#include <avr/sleep.h>
#include <PinChangeInterruptSimple.h>
#include <EEPROM.h>
#include <RoboVac.h>
#include <util/crc16.h>
/* Definitions */
#undef DEBUG
#define VW_MAX_MESSAGE_LEN 30
#define VW_MAX_PAYLOAD VW_MAX_MESSAGE_LEN-3
#define VW_RX_RAMP_LEN 160
#define VW_RX_SAMPLES_PER_BIT 8
#define VW_RAMP_INC (VW_RX_RAMP_LEN/VW_RX_SAMPLES_PER_BIT)
#define VW_RAMP_TRANSITION VW_RX_RAMP_LEN/2
#define VW_RAMP_ADJUST 9
#define VW_RAMP_INC_RETARD (VW_RAMP_INC-VW_RAMP_ADJUST)
#define VW_RAMP_INC_ADVANCE (VW_RAMP_INC+VW_RAMP_ADJUST)
#define VW_HEADER_LEN 8

// Constants used once (to save space)
#define OVERRIDEAMOUNT (unsigned int) (60000 * 5) // 5 minutes
#define INACTIVITYMAX (unsigned int) (1000 * 60 * 60 * 3) // 3 hours

// AC Frequency
#define ACHERTZ (60)
// Number of measurements to capture per wavelength
#define SAMPLESPERWAVE (20)
// Wavelength in microseconds
#define ACWAVELENGTH (1000000.0/(ACHERTZ))
// Number of microseconds per sample (MUST be larger than AnalogRead)
#define MICROSPERSAMPLE ((ACWAVELENGTH) / (SAMPLESPERWAVE))

/* I/O constants */
const int txEnablePin = 0;
const int txDataPin = 1;
const int currentSensePin = 5;
const int overridePin = 3; // has 1.5k pull up allready
const int batteryPin = 2;
const int floatingPin = 4; // for random seed

/* Global Variables */
message_t message;
unsigned long analogReadMicroseconds = 0; // measured in setup()
int sampleLow = 0;
int sampleHigh = 0;
int sampleRange = 0;
unsigned long currentTime = 0; // current ms
unsigned long overrideStart = 0; // Time when override mode first set
unsigned long lastOvertide = 0; // first time override button pressed
unsigned long lastActivity = 0; // first time override button pressed
unsigned long lastStatus = 0; // last time status message was sent
unsigned long overrideInterval = 0;
unsigned long lastTXEvent = 0; // ms since last entered txEvent()
boolean randomSeeded = false;

uint8_t vw_tx_buf[(VW_MAX_MESSAGE_LEN * 2) + VW_HEADER_LEN] = {
    0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x38, 0x2c
};
uint8_t vw_tx_len = 0;
uint8_t vw_tx_index = 0;
uint8_t vw_tx_bit = 0;
uint8_t vw_tx_sample = 0;
volatile uint8_t vw_tx_enabled = 0;
uint16_t vw_tx_msg_count = 0;
uint8_t symbols[] = {
    0xd,  0xe,  0x13, 0x15, 0x16, 0x19, 0x1a, 0x1c,
    0x23, 0x25, 0x26, 0x29, 0x2a, 0x2c, 0x32, 0x34
};
void vw_setup(uint16_t speed) {
    uint16_t nticks = 125; // 2000 bps @ 16Mhz
    uint8_t prescaler = 8; // Bit values for CS0[2:0]

    TCCR0A = 0;
    TCCR0A = _BV(WGM01); // Turn on CTC mode / Output Compare pins disconnected
    TCCR0B = 0;
    TCCR0B = prescaler; // set CS00, CS01, CS02 (other bits not needed)
    OCR0A = uint8_t(nticks);
    TIMSK |= _BV(OCIE0A);

    pinMode(txDataPin, OUTPUT);
    pinMode(txEnablePin, OUTPUT);
    digitalWrite(txEnablePin, 0);
}
void vw_tx_start() {
    vw_tx_index = 0;
    vw_tx_bit = 0;
    vw_tx_sample = 0;
    digitalWrite(txEnablePin, 1);
    vw_tx_enabled = true;
}
void vw_tx_stop() {
    digitalWrite(txEnablePin, 0);
    digitalWrite(txDataPin, false);
    vw_tx_enabled = false;
}
uint8_t vx_tx_active() {
    return vw_tx_enabled;
}
void vw_wait_tx() {
    while (vw_tx_enabled)
    ;
}
uint8_t vw_send(uint8_t* buf, uint8_t len) {
    uint8_t i;
    uint8_t index = 0;
    uint16_t crc = 0xffff;
    uint8_t *p = vw_tx_buf + VW_HEADER_LEN; // start of the message area
    uint8_t count = len + 3; // Added byte count and FCS to get total number of bytes

    if (len > VW_MAX_PAYLOAD)
        return false;
    vw_wait_tx();
    crc = _crc_ccitt_update(crc, count);
    p[index++] = symbols[count >> 4];
    p[index++] = symbols[count & 0xf];
    for (i = 0; i < len; i++) {
        crc = _crc_ccitt_update(crc, buf[i]);
        p[index++] = symbols[buf[i] >> 4];
        p[index++] = symbols[buf[i] & 0xf];
    }
    crc = ~crc;
    p[index++] = symbols[(crc >> 4)  & 0xf];
    p[index++] = symbols[crc & 0xf];
    p[index++] = symbols[(crc >> 12) & 0xf];
    p[index++] = symbols[(crc >> 8)  & 0xf];
    vw_tx_len = index + VW_HEADER_LEN;
    vw_tx_start();
    return true;
}

SIGNAL(TIM0_COMPA_vect) {
    digitalWrite(txDataPin, vw_tx_buf[vw_tx_index] & (1 << vw_tx_bit++));
    if (vw_tx_bit >= 6) {
        vw_tx_bit = 0;
        vw_tx_index++;
    }
    if (vw_tx_sample > 7)
        vw_tx_sample = 0;
}

/* end of VW copy-paste */

/* Functions */

int getBatteryMiliVolts(void) {
    return map(analogRead(batteryPin), 0, 1023, 0, 5000);
}

// logic is inverted b/c internal pull-up is used
boolean overridePressed(void) {
    if (digitalRead(overridePin) == HIGH) {
        return false;
    } else {
        return true;
    }
}

void ledBlink(unsigned int numberBlinks, unsigned int delayTime) {
    for (; numberBlinks > 0; numberBlinks--) {
        digitalWrite(txEnablePin, HIGH);
        delay(delayTime);
        digitalWrite(txEnablePin, LOW);
        delay(delayTime);
    }
}

byte getRandomByte(void) {
    if (!randomSeeded) {
        randomSeed(analogRead(floatingPin) + millis());
    }
    return random(256);
}

void txEvent(void) {
    if (thresholdBreached()) {
        lastActivity = currentTime;
        makeMessage(&message, getBatteryMiliVolts(), sampleRange);
        vw_send((uint8_t *) &message, MESSAGESIZE);
        vw_wait_tx(); // don't interrupt a tx in progress
        vw_tx_stop(); // light off
    }
}

int updateCurrentEvent(void) {
    int sample=0;

    // Fill data elements
    for (byte index=0; index< SAMPLESPERWAVE; index++) {
        sample = analogRead(currentSensePin); // 512 == 0 volts
        // translate into miliVolts
        sample = map(sample, 0, 1023, -5000, 5000);
        if (sample > sampleHigh) {
            sampleHigh = sample;
        }
        if (sample < sampleLow) {
            sampleLow = sample;
        }
        // wait difference between MICROSPERSAMPLE and analogReadMicroseconds
        delayMicroseconds(MICROSPERSAMPLE - analogReadMicroseconds);
    }
    return (sampleHigh - sampleLow) / 2; // Max peak @ +5000
}

boolean thresholdBreached() {
    if ((sampleRange >= message.threshold) ||
        (sampleRange < 0)) { // override mode
        return true;
    } else {
        return false;
    }
}

void incOverrideTime() {
    // Check if already in override mode or not
    if (overrideStart == 0) { // Entering override mode first time
        overrideStart = currentTime;
        overrideInterval = OVERRIDEAMOUNT;
    } else { // already in override mode
        overrideInterval += OVERRIDEAMOUNT;
    }
}

/*
void WakeUpISR(void) {
    sleep_disable();
    detachPcInterrupt(overridePin);
}

*/

/* Main Program */

void setup() {
    //sleep_disable();

    // override button to ground
    pinMode(overridePin, OUTPUT);
    digitalWrite(overridePin, HIGH); // enable internal pull-up

    pinMode(batteryPin, INPUT);

    pinMode(floatingPin, INPUT);

    pinMode(currentSensePin, INPUT);

    pinMode(txDataPin, OUTPUT);
    digitalWrite(txDataPin, LOW);

    pinMode(txEnablePin, OUTPUT);
    digitalWrite(txEnablePin, LOW);

    // Calibrate Analog Read speed (rc timers affected by temperature
    //                              or clock ref changed while sleeping)
    // Capture the start time in microseconds
    unsigned long startTime = (millis() * 1000) + micros();
    // 10,000 analog reads for average (below)
    for (int count = 0; count < 10000; count++) {
        message.threshold = analogRead(currentSensePin);
    }
    // Calculate duration in microseconds
    // divide the duration by number of reads
    analogReadMicroseconds = (((millis() * 1000) + micros()) - startTime) / 10000;

    // get node_id;
    message.node_id = EEPROM.read(0);
    // get threshold
    message.threshold = EEPROM.read(1) << 8;
    message.threshold |= EEPROM.read(2);

    // must hold button for ~4 seconds, remaining is entropy
    for(int count=0; count < 40; count++) {
        if (!overridePressed()) {
            break;
        } else {
            // blink light
            digitalWrite(txEnablePin, !digitalRead(txEnablePin));
        }
        delay(50);
        updateCurrentEvent(); // gather max High & min Low over time
    }

    // signal reset mode entered & add entropy
    while (overridePressed()) {
        message.node_id = 255;
        message.threshold = TRIGGER_DEFAULT;
        digitalWrite(txEnablePin, HIGH);
        updateCurrentEvent(); // gather max High & min Low over time
    }
    digitalWrite(txEnablePin, LOW);

    while ((message.node_id == 255)) { // new or reset mode
        message.node_id = getRandomByte();
        sampleRange = updateCurrentEvent(); // one last reading
        if (sampleRange >= TRIGGER_DEFAULT) {
            message.threshold = sampleRange;
        }
    }
    // Check Assumptions
    if ((analogReadMicroseconds > MICROSPERSAMPLE) ||
        (message.threshold > 4500)) {
        // SAMPLESPERWAVE is too big
        // or sensor is saturated
        while (true)
            ledBlink((unsigned int)-1, 1000);
    }

    // only write if changed
    if (message.node_id != EEPROM.read(0)) {
        ledBlink(4, 250);
        EEPROM.write(0, message.node_id);
        EEPROM.write(1, highByte(message.threshold));
        EEPROM.write(2, lowByte(message.threshold));
    } else {
        ledBlink(2, 250);
    }

    // reset reading for next time
    sampleRange = 0;
    sampleHigh = 0;
    sampleLow = 0;
    overrideStart = 0;
    overrideInterval = 0;

    // installs ISR
    vw_setup(RXTXBAUD);

    lastActivity = millis(); // wake up //

    digitalWrite(txEnablePin, LOW); // light off
}

void loop() {
    currentTime = millis();

    if (overrideStart == 0) { // not in override
        // Sample data
        sampleRange = updateCurrentEvent();
    } else { // in override mode
        sampleRange = -1;
        if (timerExpired(&currentTime, &overrideStart, overrideInterval)) {
            // cancel override mode
            overrideStart = 0;
            overrideInterval = 0;
        }
    }

    if (overridePressed()) {
        while (overridePressed()) {
            digitalWrite(txEnablePin, HIGH); // light on
        }
        delay(10); // debounce
        incOverrideTime();
    }

    if (timerExpired(&currentTime, &lastTXEvent, TXINTERVAL)) {
        txEvent(); // Also check override button press

        // jiggle lastTXEvent +/- 1/5 TXINTERVAL to help with collisions
        lastTXEvent = currentTime;
        lastTXEvent += map(random(TXINTERVAL/5) + 1, 0, (TXINTERVAL/5),
                           -1 * (TXINTERVAL/10), (TXINTERVAL/10));
        // reset reading for next time
        sampleHigh = 0;
        sampleLow = 0;
    }

/*
    if (timerExpired(&currentTime, &lastActivity, INACTIVITYMAX)) {
        digitalWrite(txEnablePin, LOW); // light off
        attachPcInterrupt(overridePin, WakeUpISR, CHANGE);
        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
        sleep_cpu();
        setup(); // wakeup //
    }
*/
}
