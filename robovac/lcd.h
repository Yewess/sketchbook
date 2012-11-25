#ifndef LCD_H
#define LCD_H

#include <RoboVac.h>
#include "globals.h"

inline void clearLcdBuf(void) {
    // Fill with space characters
    for (int y=0; y<lcdRows; y++) {
        memset(lcdBuf[y], ' ', lcdCols);
    }
    // Tag NULLs on the end of each row
    for (int y=0; y<lcdRows; y++) {
        lcdBuf[y][lcdCols] = '\0';
    }
}

inline void printLcdBuf(void) {
    // Assume buffer is full of spaces
    for (int y=0; y<lcdRows; y++) {
        lcd.setCursor(0, y);
        lcd.print(lcdBuf[y]);
        D(lcdBuf[y]); D("\n");
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

    memset(highlight, ' ', lcdCols);
    highlight[lcdCols] = '\0';
    highlight[0] = char(LCDRARROW);
    // one less to account for left arrow
    if (strlen(entry->name) + 2 > lcdCols) {
        memcpy(&highlight[1], entry->name, lcdCols-2);
        highlight[lcdCols-1] = LCDLARROW;
    } else {
        memcpy(&highlight[1], entry->name, strlen(entry->name));
        highlight[strlen(entry->name)] = LCDLARROW;
    }
    return highlight; // guaranteed lcdCols long
}

void drawMenu(void) {
    const char *highlight = itemHighlight(currentMenu);

    D("Menu:\n");
    if (!currentCallback) { // they draw their own
        unsigned int above = itemsAbove(currentMenu);
        menuEntry_t *entryp = currentMenu;
        int row=0;

        clearLcdBuf();
        if (above == 0) { // top level selected
            // place highlighted currentMenu->name
            memcpy(lcdBuf[row], highlight, lcdCols);
            // place remaining rows if any
            entryp = entryp->next_sibling;
            for (row = 1; entryp && (row < lcdRows); row++) {
                memcpy(lcdBuf[row], entryp->name, strlen(entryp->name));
            // place remaining rows if any
                entryp = entryp->next_sibling;
            }
        } else { // above >= 1
            // clip above to size of LCD minus 1 for currentEntry
            if (above >= lcdRows - 1) {
                row = lcdRows - 1; // currentMenu->name on last row
            } else {
                row = above; // currentMenu->name on 'above' row
            }
            // place highlighted currentMenu->name
            memcpy(lcdBuf[row], highlight, lcdCols);
            // place rows above
            entryp = entryp->prev_sibling;
            for (row -= 1; (entryp) && (row >=0); row--) {
                memcpy(lcdBuf[row], entryp->name, strlen(entryp->name));
                entryp = entryp->prev_sibling; // guaranteed b/c above > 0
            }
            // figure where currentMenu was printed again
            if (above >= lcdRows - 1) {
                row = lcdRows - 1; // currentMenu->name on last row
            } else {
                row = above; // currentMenu->name on 'above' row
            }
            // print remaining items if any, starting on next row
            entryp = currentMenu->next_sibling;
            for(row += 1; (entryp) && (row < lcdRows); row++) {
                memcpy(lcdBuf[row], entryp->name, strlen(entryp->name));
                entryp = entryp->next_sibling;
            }
        }
        printLcdBuf();
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

void handleCallback(unsigned long *currentTime) {
    // Enter callback if any set
    if (currentCallback) {
        // Callback will handle & clear buttons
        if ( (*currentCallback)(currentTime) ) {
            currentCallback = NULL;
        }
        lcdButtons = 0;
    }
}

boolean blinkBacklight(unsigned long *currentTime) {
    if (timerExpired(currentTime, &lastButtonChange, LCDBLINKTIME)) {
        D("Light\n");
        lcd.setBacklight(0x1); // ON
        return true;
    } else {
        D("Dark\n");
        lcd.setBacklight(0x0); // OFF
        return false;
    }
}

boolean monitorCallback(unsigned long *currentTime) {
    D("monitorCB\n");
}

boolean portIDSetupCallback(unsigned long *currentTime) {
    D("portIDSetupCB\n");
}

boolean travelAdjustCallback(unsigned long *currentTime) {
    D("travelAdjustCB\n");
}

boolean portToggleCallback(unsigned long *currentTime) {
    D("portToggleCB\n");
}

boolean vacToggleCallback(unsigned long *currentTime) {
    D("vacToggleCB\n");
}

#endif // LCD_H