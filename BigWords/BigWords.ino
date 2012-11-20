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

#include <Wire.h>
#include <Adafruit_MCP23017.h>
#include <Adafruit_RGBLCDShield.h>
#include "BigWords.h"

void setup(void) {
    uint8_t index = 0;

    lcd.begin(16, 2);
    lcd.setBacklight(1); // ON
    lcd.clear();

    // Load custom characters as #0 - #7
    for (index = 0; index < 8; index++) {
        lcd.createChar(index, custom_segments[index]);
    }

}

void loop(void) {
    // Spaces on both sides make it clear the screen
    char alphanum[] = "    AnTiDiSeStAbLiShMeNtArInIsM    ";
    // Pointer to current character being printed
    char *stringp = &alphanum[0];
    // Offset off left of screen to start printing
    int offset = 0;
    // ms between moving letters
    int interval = 500;
    // Keep track of time
    unsigned long currentTime = millis();
    // Keep track of last update time
    unsigned long lastTime = 0;

    while (scrollString(&stringp, &offset, interval, &currentTime, &lastTime)) {
        delay(10); // or do something else useful
        currentTime = millis();
    }
}
