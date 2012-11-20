/*
    Functions/Definitions for 16x2 LCD Block font + scrolling text

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

#ifndef BIGWORDS_H
#define BIGWORDS_H

#include <Arduino.h>

/* Font based on http://www.instructables.com/id/Custom-Large-Font-For-16x2-LCDs/#intro
   many thanks to mpilchfamily for publishing the details. Some characters modified
   from original design, but most remain unchanged. The array elements are condensed
   below to save space, it's easier to see how they look by breaking them up and
   replacing 1's for # and blanking the 0's: e.g.

    {B11111,B11111,B11111,B11111,B01111,B01111,B00111,B00001}, // LL 3 - Lower Left

    becomes:

      #####
      #####
      #####
      #####
       ####
       ####
        ###
          #
*/

uint8_t custom_segments[][8] = {
    {B00001,B00111,B01111,B01111,B11111,B11111,B11111,B11111}, // LT 8 - Left Top
    {B11111,B11111,B11111,B00000,B00000,B00000,B00000,B00000}, // UB 1 - Upper Bar
    {B10000,B11100,B11110,B11110,B11111,B11111,B11111,B11111}, // RT 2 - Right Top
    {B11111,B11111,B11111,B11111,B01111,B01111,B00111,B00001}, // LL 3 - Lower Left
    {B00000,B00000,B00000,B00000,B00000,B11111,B11111,B11111}, // LB 4 - Lower Bar
    {B11111,B11111,B11111,B11111,B11110,B11110,B11100,B10000}, // LR 5 - Lower Right
    {B11111,B11111,B11111,B00000,B00000,B00000,B11111,B11111}, // UM 6 - Upper Middle
    {B11111,B11111,B00000,B00000,B00000,B11111,B11111,B11111}  // LM 7 - Lower Biddle
};                                                             // SC - Solid Character
                                                               // BC - Blank Char

// Make font segments indexes human readable (BC/SC inverted on HD44780)
enum { LT=0, UB, RT, LL, LB, LR, UM, LM, BC=254, SC=255 };

/* Glyphs are actually 4x2 characters, but the last column is
   always BC, save memory by not storing it. Array elements
   in a compact form here to save space.  When working with them
   it helps to spit array elements into 3 across 2 lines: e.g.

    { UB, UB, LR,
      BC, LT, BC },// 9  (ASCII 55 '7')
*/
uint8_t custom_glyphs[][6] = {
    { BC, BC, BC, BC, BC, BC },// 0  (ASCII 255 blank)
    { SC, SC, SC, SC, SC, SC },// 1  (ASCII 254 solid)
    { SC, UB, SC, SC, LB, SC },// 2  (ASCII 48 '0')
    { UB, RT, BC, LB, SC, LB },// 3  (ASCII 49 '1')
    { UM, UM, RT, LL, LM, LM },// 4  (ASCII 50 '2')
    { UM, UM, RT, LM, LM, LR },// 5  (ASCII 51 '3')
    { LL, BC, RT, BC, UB, SC },// 6  (ASCII 52 '4')
    { LT, UB, UB, LM, LM, LR },// 7  (ASCII 83 '5')
    { LT, UM, UM, LL, LM, LR },// 8  (ASCII 54 '6')
    { UB, UB, LR, BC, LT, BC },// 9  (ASCII 55 '7')
    { LT, UM, RT, LL, LM, LR },// 10 (ASCII 56 '8')
    { LT, UB, SC, BC, UB, SC },// 11 (ASCII 57 '9')
    { LT, UM, RT, SC, UB, SC },// 12 (ASCII 65 'A')
    { SC, UM, LR, SC, LM, RT },// 13 (ASCII 66 'B')
    { LT, UB, UB, LL, LB, LB },// 14 (ASCII 67 'C')
    { SC, UB, RT, SC, LB, LR },// 15 (ASCII 68 'D')
    { SC, UM, UM, SC, LM, LM },// 16 (ASCII 69 'E')
    { LT, UM, UM, SC, UB, UB },// 17 (ASCII 70 'F')
    { LT, UB, UB, LL, LB, RT },// 18 (ASCII 71 'G')
    { SC, LB, SC, SC, UB, SC },// 19 (ASCII 72 'H')
    { UB, SC, UB, LB, SC, LB },// 20 (ASCII 73 'I')
    { BC, UB, SC, LL, LB, SC },// 21 (ASCII 74 'J')
    { SC, LB, LR, SC, UB, RT },// 22 (ASCII 75 'K')
    { SC, BC, BC, LL, LB, LB },// 23 (ASCII 76 'L')
    { RT, LB, LT, SC, BC, SC },// 24 (ASCII 77 'M')
    { RT, LB, SC, SC, UB, LL },// 25 (ASCII 78 'N')
    { LT, UB, RT, LL, LB, LR },// 26 (ASCII 79 'O')
    { SC, UB, RT, SC, UB, BC },// 27 (ASCII 80 'P')
    { LT, UB, RT, LL, LB, LL },// 28 (ASCII 81 'Q')
    { SC, UB, RT, SC, UB, LL },// 29 (ASCII 82 'R')
    { LT, UM, UM, LM, LM, LR },// 30 (ASCII 53 'S')
    { UB, SC, UB, BC, SC, BC },// 31 (ASCII 84 'T')
    { SC, BC, SC, LL, LB, LR },// 32 (ASCII 85 'U')
    { SC, BC, LR, SC, LT, BC },// 33 (ASCII 86 'V')
    { SC, BC, SC, LR, UB, LL },// 34 (ASCII 87 'W')
    { LL, BC, LR, LT, UB, RT}, // 35 (ASCII 88 'X')
    { LL, BC, LR, BC, SC, BC },// 36 (ASCII 89 'Y')
    { UB, UM, LR, LT, LM, LB },// 37 (ASCII 90 'Z')
};

// initialize the interface pins
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

/* Function to map from an ASCII character to it's
   index within the custom_glyph array (above) */
int ascii_to_custom_glyph_index(unsigned char ascii) {
    if (ascii == 32) {
        return 0; // Blank Character
    } else if ((ascii >= 48) && (ascii <= 57)) {
        return ascii - 46; // numbers 0 - 9
    } else if ((ascii >= 65) && (ascii <= 90)) {
        return ascii - 53; // letters A - Z
    } else if ((ascii >= 97) && (ascii <= 122)) {
        return ascii - 85; // letters A - Z (lower case)
    } else if (ascii == 254) {
        return 0; // Solid Character
    } else if (ascii == 255) {
        return 1; // Blank Character
    } else {
        // unmapped character
        return -1;
    }
}

/* Function that copies a glyph for an ASCII character
   into a static 4x2 character buffer. Return pointer
   to static buffer for use.  N/B: The buffer contents
   change on every call! */
const uint8_t *glyph_buffer(unsigned char ascii) {
    int index = ascii_to_custom_glyph_index(ascii);
    static uint8_t glyph[8]; // Return buffer

    if (index >= 0) { // glyph is defined in table
        glyph[0] = custom_glyphs[index][0]; glyph[1] = custom_glyphs[index][1];
        glyph[2] = custom_glyphs[index][2]; glyph[3] = BC;
        glyph[4] = custom_glyphs[index][3]; glyph[5] = custom_glyphs[index][4];
        glyph[6] = custom_glyphs[index][5]; glyph[7] = BC;
    } else { /* Glyph NOT defined in table.  Aid in debugging
                by encoding ASCII value (in decemal) as the
                character in the glyph buffer */
        glyph[0] = (ascii / 100)+48; glyph[1] = (ascii / 10)+48;
        glyph[2] = (ascii % 10)+48;  glyph[3] = BC;
        glyph[4] = (ascii / 100)+48; glyph[5] = (ascii / 10)+48;
        glyph[6] = (ascii % 10)+48;  glyph[7] = BC;
    }
    return glyph;
}

/* Function to draw an ASCII character on the LCD at a particular
   X location ranging from -3 (off-screen left) to 15 (right most column)
   on a HD44780 based 16x2 LCD display */
void putLCDChar(char ascii, char xPos) {
    const uint8_t *glyph = glyph_buffer(ascii);
    unsigned char c=0;

    // Glyphs hanging off the left edge
    if ((xPos > -4) && (xPos < 0)) { // print trailing segments only
        if (xPos == -3) { // print last col.
            lcd.setCursor(0,0); lcd.write(glyph[3]);
            lcd.setCursor(0,1); lcd.write(glyph[7]);
        } else if (xPos == -2) { // print second to last, and last col.
            lcd.setCursor(0,0); lcd.write(glyph[2]);
            lcd.setCursor(1,0); lcd.write(glyph[3]);
            lcd.setCursor(0,1); lcd.write(glyph[6]);
            lcd.setCursor(1,1); lcd.write(glyph[7]);
        } else if (xPos == -1) { // print last three col.
            lcd.setCursor(0,0); lcd.write(glyph[1]);
            lcd.setCursor(1,0); lcd.write(glyph[2]);
            lcd.setCursor(2,0); lcd.write(glyph[3]);
            lcd.setCursor(0,1); lcd.write(glyph[5]);
            lcd.setCursor(1,1); lcd.write(glyph[6]);
            lcd.setCursor(2,1); lcd.write(glyph[7]);
        } else {
            // xPos == 0 case is picked up in next block
        }

    // Glyphs hanging off the right edge
    } else if (xPos < 16) { // print leading segments only upto x=15
        if (xPos == 15) { // print first col. of glyph
            lcd.setCursor(15,0); lcd.write(glyph[0]);
            lcd.setCursor(15,1); lcd.write(glyph[4]);
        } else if (xPos == 14) { // first & second col
            lcd.setCursor(14,0); lcd.write(glyph[0]);
            lcd.setCursor(15,0); lcd.write(glyph[1]);
            lcd.setCursor(14,1); lcd.write(glyph[4]);
            lcd.setCursor(15,1); lcd.write(glyph[5]);
        } else if (xPos == 13) { // first, second & third col
            lcd.setCursor(13,0); lcd.write(glyph[0]);
            lcd.setCursor(14,0); lcd.write(glyph[1]);
            lcd.setCursor(15,0); lcd.write(glyph[2]);
            lcd.setCursor(13,1); lcd.write(glyph[4]);
            lcd.setCursor(14,1); lcd.write(glyph[5]);
            lcd.setCursor(15,1); lcd.write(glyph[6]);

    // Glyphs that fit entirely on the screen
        } else { // can write whole char
            for (c = 0; c < 4; c++) {
                // first row
                lcd.setCursor(xPos + c, 0); lcd.write(glyph[c]);
                // second row
                lcd.setCursor(xPos + c, 1); lcd.write(glyph[c+4]);
            }
        }

    // xPos is out of range
    } else {
        // do nothing, xPos too far off-screen
    }
}

/* Update a scrolling string on the LCD, using stringp,
   offset, currentTime and lastTime to keep track of
   progress. Returns true if scrolling remaining or
   false if scrolling is finished. */
boolean scrollString(char **stringp, int *offset, int interval,
                     const unsigned long *currentTime, unsigned long *lastTime) {
    unsigned long elapsedTime;

    // Find out how much time has passed
    if (*currentTime < *lastTime) { // millis() has wrapped
        // wrap amount is (((unsigned long)-1) - lastTime)
        elapsedTime = (((unsigned long)-1) - *lastTime) + *currentTime;
    } else {
        elapsedTime = *currentTime - *lastTime;
    }

    // Check if scrolling is finished pointing into land of OZ
    if ( (offset == NULL) || (*offset > 0) || (stringp == NULL) ||
         (*stringp == NULL) || (**stringp == NULL) ) {
        return false; // done scrolling :S

    // Time to manipulate screen
    } else if (elapsedTime >= interval) {
        int index=0;

        // offset ranges from 0 to -3, decrementing on each call

        // Write up-to next 5 non-NULL characters
        for (index=0; index < 6; index++) {
            // Copy character to print
            char character = *(*stringp + index);
            // Calculate where to put it (-3 - 15)
            int xPos = ((index % 6) * 4) + *offset;

            if (character == '\0') {
                break; // Don't print past the end of the string
            } else {
                putLCDChar(character, xPos);
            }
        }

        // Update offset & stringp for next call
        *offset -= 1;
        if (*offset < -3) {
            *offset = 0;
            *stringp += 1; // point at next character
            if ((*stringp == NULL) || (**stringp == NULL)) {
                return false; // done scrolling
            }
        }
        // Keep track of last time scrolling happened
        *lastTime = *currentTime;

    // Not enough time has passed
    } else {
        // Wait some more
    }

    // Not done scrolling
    return true;
}

#endif // BIGWORDS_H
