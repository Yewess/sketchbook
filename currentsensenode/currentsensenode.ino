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
#include <RoboVac.h>

/* Have to copy-paste only what's needed from VW to reduce memory space */
#include <util/crc16.h>
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
uint8_t vw_tx_buf[(VW_MAX_MESSAGE_LEN * 2) + VW_HEADER_LEN] = {
    0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x38, 0x2c
};
uint8_t vw_tx_len = 0;
uint8_t vw_tx_index = 0;
uint8_t vw_tx_bit = 0;
uint8_t vw_tx_sample = 0;
volatile uint8_t vw_tx_enabled = 0;
uint16_t vw_tx_msg_count = 0;
uint8_t vw_ptt_pin = 10;
uint8_t vw_ptt_inverted = 0;
uint8_t vw_tx_pin = 12;
uint8_t symbols[] = {
    0xd,  0xe,  0x13, 0x15, 0x16, 0x19, 0x1a, 0x1c,
    0x23, 0x25, 0x26, 0x29, 0x2a, 0x2c, 0x32, 0x34
};
uint16_t vw_crc(uint8_t *ptr, uint8_t count) {
    uint16_t crc = 0xffff;

    while (count-- > 0)
    crc = _crc_ccitt_update(crc, *ptr++);
    return crc;
}
uint8_t vw_symbol_6to4(uint8_t symbol) {
    uint8_t i;
    for (i = 0; i < 16; i++)
    if (symbol == symbols[i]) return i;
    return 0; // Not found
}
void vw_set_tx_pin(uint8_t pin) {
    vw_tx_pin = pin;
}
void vw_set_ptt_pin(uint8_t pin) {
    vw_ptt_pin = pin;
}
void vw_set_ptt_inverted(uint8_t inverted) {
    vw_ptt_inverted = inverted;
}
uint8_t _timer_calc(uint16_t speed, uint16_t max_ticks, uint16_t *nticks) {
    uint16_t prescalers[] = {0, 1, 8, 64, 256, 1024, 3333};
    uint8_t prescaler=0; // index into array & return bit value
    unsigned long ulticks; // calculate by ntick overflow

    if (speed == 0) {
        *nticks = 0;
        return 0;
    }
    for (prescaler=1; prescaler < 7; prescaler += 1)
    {
        float clock_time = (1.0 / (float(F_CPU) / float(prescalers[prescaler])));
        float bit_time = ((1.0 / float(speed)) / 8.0);
        ulticks = long(bit_time / clock_time);
        if ((ulticks > 1) && (ulticks < max_ticks)) {
            break; // found prescaler
        }
    }
    if ((prescaler == 6) || (ulticks < 2) || (ulticks > max_ticks)) {
        *nticks = 0;
        return 0;
    }

    *nticks = ulticks;
    return prescaler;
}
void vw_setup(uint16_t speed) {
    uint16_t nticks; // number of prescaled ticks needed
    uint8_t prescaler; // Bit values for CS0[2:0]

    prescaler = _timer_calc(speed, (uint8_t)-1, &nticks);
    if (!prescaler)
        return; // fault

    TCCR0A = 0;
    TCCR0A = _BV(WGM01); // Turn on CTC mode / Output Compare pins disconnected
    TCCR0B = 0;
    TCCR0B = prescaler; // set CS00, CS01, CS02 (other bits not needed)
    OCR0A = uint8_t(nticks);
    TIMSK |= _BV(OCIE0A);

    pinMode(vw_tx_pin, OUTPUT);
    pinMode(vw_ptt_pin, OUTPUT);
    digitalWrite(vw_ptt_pin, vw_ptt_inverted);
}
void vw_tx_start() {
    vw_tx_index = 0;
    vw_tx_bit = 0;
    vw_tx_sample = 0;
    digitalWrite(vw_ptt_pin, true ^ vw_ptt_inverted);
    vw_tx_enabled = true;
}
void vw_tx_stop() {
    digitalWrite(vw_ptt_pin, false ^ vw_ptt_inverted);
    digitalWrite(vw_tx_pin, false);
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
    digitalWrite(vw_tx_pin, vw_tx_buf[vw_tx_index] & (1 << vw_tx_bit++));
    if (vw_tx_bit >= 6) {
        vw_tx_bit = 0;
        vw_tx_index++;
    }
    if (vw_tx_sample > 7)
        vw_tx_sample = 0;
}

/* end of VW copy-paste */

/* Definitions */
#undef DEBUG

// Constants used once (to save space)
#define SENSEINTERVAL 101       // 10th of a second
#define OVERRIDEAMOUNT (60000 * 3)

// AC Frequency
#define ACHERTZ (60)
// Number of measurements to capture per wavelength
#define SAMPLESPERWAVE (20)
// Wavelength in microseconds
#define ACWAVELENGTH (1000000.0/(ACHERTZ))
// Number of microseconds per sample (MUST be larger than AnalogRead)
#define MICROSPERSAMPLE ((ACWAVELENGTH) / (SAMPLESPERWAVE))

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
unsigned long currentTime = 0; // current ms
unsigned long overrideEnter = 0; // Time when override mode first set
unsigned long lastOvertide = 0; // first time override button pressed
unsigned long lastStatus = 0; // last time status message was sent
unsigned long overrideInterval = 0;
unsigned long lastTXEvent = 0; // ms since last entered txEvent()
unsigned long lastCurrentEvent = 0; // ms since last entered updateCurrentEvent()
boolean randomSeeded = false;
boolean overrideMode = false;

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

byte getRandomByte(void) {
    if (!randomSeeded) {
        randomSeed(analogRead(floatingPin) + millis());
    }
    return random(256);
}

void txEvent(void) {
    if (thresholdBreached()) {
        digitalWrite(txEnablePin, HIGH);
        makeMessage(&message, nodeID, MSG_BREACH, getBatteryMiliVolts());
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
        }
        // not in override & not over threshold
        return false;
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
    ledBlink(1, 250);
}

void ledBlink(byte numberBlinks, unsigned int delayTime) {
    for (; numberBlinks > 0; numberBlinks--) {
        digitalWrite(txEnablePin, HIGH);
        delay(delayTime);
        digitalWrite(txEnablePin, LOW);
        delay(delayTime);
    }
}

/* Main Program */

void setup() {
    // override button to ground (needs internal pull-up)
    pinMode(overridePin, OUTPUT);
    digitalWrite(overridePin, HIGH);

    pinMode(batteryPin, INPUT);

    pinMode(floatingPin, INPUT);
    pinMode(currentSensePin, INPUT);

    pinMode(txDataPin, OUTPUT);
    digitalWrite(txDataPin, LOW);
    vw_set_tx_pin(txDataPin);

    pinMode(txEnablePin, OUTPUT);
    digitalWrite(txEnablePin, LOW);
    vw_set_ptt_pin(txEnablePin);

    overrideCancel();

    //setNodeID();
    // get currently set value (255 is default)
    nodeID = EEPROM.read(0);

    // sets nodeID == 0 on reset
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

    while ((nodeID == 255) || (nodeID == 0)) {
        nodeID = getRandomByte();
    }
    // only write if changed
    if (nodeID != EEPROM.read(0)) {
        ledBlink(4, 250);
        EEPROM.write(0, nodeID);
    }

    // calibrate Analog Read speed
    // Capture the start time in microseconds
    unsigned long startTime = (millis() * 1000) + micros();
    // 10,000 analog reads for average (below)
    for (int count = 0; count < 10000; count++) {
        threshold = analogRead(currentSensePin);
    }
    // Calculate duration in microseconds
    // divide the duration by number of reads
    analogReadMicroseconds = (((millis() * 1000) + micros()) - startTime) / 10000;

    //calibrateThreshold();
    for(byte count=0; count < 100; count++) {
        threshold = updateCurrentEvent(); // gather max High & min Low over time
    }
    // 256 signals a calibration error
    threshold = constrain(threshold * 5, 5, 256);
    // reset reading for next time
    sampleHigh = 0;
    sampleLow = 0;

    // Check Assumptions
    if ((threshold == 256) || (analogReadMicroseconds > MICROSPERSAMPLE)) {
        // signal error, MICROSPERSAMPLE is too small!
        ledBlink(255, (unsigned int)-1);
    }


    // installs ISR
    vw_setup(RXTXBAUD);

    // Sent 5 hello messages
    for (char count=0; count < 5; count++) {
        makeMessage(&message, nodeID, MSG_HELLO, getBatteryMiliVolts());
        vw_send((uint8_t *) &message, MESSAGESIZE);
        delay(TXINTERVAL);
    }
}

void loop() {
    currentTime = millis();
    if (timerExpired(&currentTime, &lastCurrentEvent, SENSEINTERVAL)) {
        // clear previous data
        sampleRange = updateCurrentEvent();
        lastCurrentEvent = currentTime;
    }
    if (timerExpired(&currentTime, &lastTXEvent, TXINTERVAL)) {
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
    if (timerExpired(&currentTime, &lastStatus, STATUSINTERVAL)) {
        makeMessage(&message, nodeID, MSG_STATUS, getBatteryMiliVolts());
        vw_send((uint8_t *) &message, MESSAGESIZE);
    }
}
