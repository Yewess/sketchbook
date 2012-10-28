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

#include <util/crc16.h>

/* Externals */

extern uint16_t crc16_update(uint16_t crc, uint8_t a);

/* I/O constants */
const int statusLEDPin = 13;
const int currentSensePin = A0;
const int idSelectPins[] = {A5,A4,A3,A2,A1,A5,A4,A3};

/* Sense constants */
const unsigned int baseInputLevel = 512; /* Half of 5v supply, 2.5v */

/* comm. constants */
const byte syncPreamble = B01010101;
const unsigned long statusLEDInterval = 5000;
const word serialBaud = 9600;

/* Types */
struct OutputBuf {
  byte syncPreamble; /* Something to watch for */
  word CRC1; /* CRC of frame (less CRC1 and CRC2)*/
  byte nodeID; /* ID number of node */
  word sense0; /* Sensor one data */
  word sense1; /* Sensor two data */
  word sense2; /* Sensor three data */
  word sense3; /* Sensor four data */
  word CRC2; /* CRC of frame (less CRC2) */  
};

/* Global Variables */
byte nodeID = 0xFF;

/* Functions */

word calcOutputBufCRC(struct OutputBuf *outputbuf) {
  byte byte_counter = sizeof(OutputBuf);
  byte *current_byte = (byte *) outputbuf;
  word result = 0;
  
  for (; byte_counter--; byte_counter > 0) {
    result = _crc16_update(result, *current_byte);
    current_byte += 1;
  }
  return result;
}

struct OutputBuf makeOutputBuf(byte nodeID, word sense0, word sense1,
                               word sense2, word sense3) {
  struct OutputBuf outputBuf = {0};

  outputBuf.syncPreamble = syncPreamble;
  outputBuf.nodeID = nodeID;
  outputBuf.sense0 = sense0;
  outputBuf.sense1 = sense1;
  outputBuf.sense2 = sense2;
  outputBuf.sense3 = sense3;
  outputBuf.CRC1 = calcOutputBufCRC(&outputBuf);
  outputBuf.CRC2 = calcOutputBufCRC(&outputBuf);
  return outputBuf;
}

void printOutputBuf(struct OutputBuf outputBuf) {
  word CRC1 = outputBuf.CRC1;
  word CRC2 = outputBuf.CRC2;

  outputBuf.CRC1 = 0;
  outputBuf.CRC2 = 0;
  outputBuf.CRC1 = calcOutputBufCRC(&outputBuf);
  outputBuf.CRC2 = calcOutputBufCRC(&outputBuf);
  Serial.print("FRAME: B"); Serial.print(outputBuf.syncPreamble, BIN);
  Serial.print("  0x"); Serial.print(CRC1, HEX);
  if (outputBuf.CRC1 == CRC1) {
    Serial.print("(GOOD)");
  } else {
    Serial.print("(BAD!)");
  }
  Serial.print("  0x"); Serial.print(outputBuf.sense0, HEX);
  Serial.print("  0x"); Serial.print(outputBuf.sense1, HEX);
  Serial.print("  0x"); Serial.print(outputBuf.sense1, HEX);
  Serial.print("  0x"); Serial.print(outputBuf.sense3, HEX);
  Serial.print("  0x"); Serial.print(CRC2, HEX);
  if (outputBuf.CRC2 == CRC2) {
    Serial.println("(GOOD)");
  } else {
    Serial.println("(BAD!)");
  }   
}

/* Return True/False if LED state should change */
boolean intervalPassed(unsigned long *previous, const unsigned long *interval) {
  unsigned long current = millis();

  Serial.print("Previous: "); Serial.print(*previous);
  Serial.print(" Current: "); Serial.println(current);


  // TODO: Check for wraparound
  if (current - *previous > *interval) {
    *previous = current;
    return true;
  } else {
    *previous = current;
    return false;
  }
}


/* Main Program */

void setup() {
  unsigned int idSelectCounter = 0;

  /* Let pins settle */  
  delay(1);

  pinMode(statusLEDPin, OUTPUT);
  
  /* Read the node ID */
  for (; idSelectCounter++; idSelectCounter < 8) {
    int pin = idSelectPins[idSelectCounter];
    
    /* BEGIN STUB */
    if (analogRead(pin) < 255) {
    /* END   STUB */      
      bitSet(nodeID, idSelectCounter);
    } else {
      bitClear(nodeID, idSelectCounter);
    }
  }
  
  /* Setup debugging */
  Serial.begin(serialBaud);
  Serial.print("Node ID: 0x");
  Serial.print(nodeID, HEX);
  Serial.print(" B");
  Serial.println(nodeID, BIN);
  Serial.println();
}

void loop() {
  struct OutputBuf outputBuf = {0};
  unsigned long previousMillis = 0;

  if (intervalPassed(&previousMillis, &statusLEDInterval)) {
    digitalWrite(statusLEDPin, HIGH);
    /* BEGIN STUB */
    outputBuf = makeOutputBuf(nodeID, analogRead(5), analogRead(4),
                              analogRead(3), analogRead(2));
    /* END   STUB */
  } else {
    digitalWrite(statusLEDPin, LOW);
  }    
}
