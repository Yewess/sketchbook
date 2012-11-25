#ifndef LCD_H
#define LCD_H

#include <RoboVac.h>
#include "globals.h"

inline void clearLcdBuf(void) {
    // Fill with space characters
    memset((void *)lcdBuf, 0x20, lcdRows * lcdCols);
    // Tag NULLs on the end of each row
    for (int y=0; y<lcdRows; y++) {
        lcdBuf[y][lcdCols] = '\0';
    }
}

inline void printLcdBuf(void) {
    lcd.setCursor(0, 0);
    // Assume buffer is full of spaces
    for (int y=0; y<lcdRows; y++) {
        lcd.print(lcdBuf[0][y]);
    }
}

unsigned int itemsAbove(menuEntry_t *entry) {
    if (entry->prev_sibling) {
        return 1 + itemsAbove(entry->prev_sibling);
    } else {
        return 0;
    }
}

const char *itemHighlight(menuEntry_t *entry) {
    static char highlight[lcdCols+1]; // extra space to make shifting easy

    memset((void *) highlight, int(LCDLARROW), lcdCols);
    highlight[lcdCols] = '\0';
    highlight[0] = char(LCDRARROW);
    // one less to account for left arrow
    memcpy(&highlight[1], entry->name, strlen(entry->name)-1);
    return highlight; // guaranteed lcdCols long
}

void drawMenu(void) {
    if (!currentCallback) { // they draw their own
        unsigned int above = itemsAbove(currentMenu);
        menuEntry_t *entryp = currentMenu;
        int row=0;

        clearLcdBuf();
        if (above == 0) { // top level selected
            memcpy(lcdBuf[0][0], itemHighlight(entryp), lcdCols);
            while (entryp->next_sibling) {
                // clearLcdBuf already took care of NULLs
                memcpy(lcdBuf[row][0], entryp->name, strlen(entryp->name));
                row++;
                entryp = entryp->next_sibling;
            }
        } else { // fill up and down to limits
            for (row = above-1; (entryp) && (row >=0); row--) {
                entryp = entryp->prev_sibling; // guaranteed b/c above > 0
                memcpy(lcdBuf[0][row], entryp->name, strlen(entryp->name));
            }
            // range limit 'above' to lcd rows
            // low range is 1 b/c above != 0, max is last lcd row
            row = constrain(above, 1, (lcdRows-1));
            // highlight current item
            memcpy(lcdBuf[0][row], itemHighlight(currentMenu), lcdCols);
            entryp = currentMenu->next_sibling;
            row++;
            // fill in remaining rows if remaining & room
            while ((entryp) && (row < lcdCols)) {
                memcpy(lcdBuf[0][row], entryp->name, strlen(entryp->name));
                entryp = entryp->prev_sibling;
                row++;
            }
        }
        printLcdBuf();
    }
}

void blinkBacklight(unsigned long *currentTime, uint8_t buttons) {
    if (timerExpired(currentTime, &lastButtonChange, LCDBLINKTIME)) {
        lcd.setBacklight(0x1); // ON
        currentCallback = (menuEntryCallback_t) NULL;
        drawMenu();
    } else {
        lcd.setBacklight(0x0); // OFF
        currentCallback = blinkBacklight;
    }
}

void drawRunning(unsigned long *currentTime, const char *what) {
    // STUB
    lcd.clear();
    lcd.print(what);
    D("VACUUMING ");
    D(what);
    D("\n");
}

void monitorCallback(unsigned long *currentTime, uint8_t buttons) {

}

void portIDSetupCallback(unsigned long *currentTime, uint8_t buttons) {

}

void travelAdjustCallback(unsigned long *currentTime, uint8_t buttons) {

}

void portToggleCallback(unsigned long *currentTime, uint8_t buttons) {

}

void vacToggleCallback(unsigned long *currentTime, uint8_t buttons) {

}

#endif // LCD_H
