#ifndef CALLBACKS_H
#define CALLBACKS_H

#include "globals.h"

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

#endif // CALLBACKS_H
