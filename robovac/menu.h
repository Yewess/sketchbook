/*
|12345678912345| |12345678912345| |12345678912345|
|--------------| |--------------| |--------------|

 RoboVac v0.0.0 +>Back
   > Menu <     |
                | Port/ID Names <->Back
 ^     |        | Travel Adjust<+  Port#00<ID#00<
 |     v        |               |  PORT00MAPNAME<
 |              |               |      ...
 Back           |               |  Port#10<ID#10<
 Setup  <-------+               |  PORT00MAPNAME<
 Monitor <--------------+       |
 Manual Control         |       |
 ^   |                  |       |
 |   |                  +-------|->ID# ???
 |   V                          |  Elapsed: s
 Back                           |
 Port Control <-->Back          |
 Vac. Control     PORT00MAPNAME?|
     ^            00: Open/Close|
     |                ...       |
     |            PORT00MAPNAME?|
     |            16: Open/Close|
     |                          |
     V                          |
 Back                           |
 Vac: On/Off     |12345678912345|
                 |--------------|
                                |
                                |
                  Back  <-------+
                  PORT00MAPNAME<
                  Lo:000<Hi:000<
                       ...
                  PORT00MAPNAME<
                  Lo:000<Hi:000<
*/

#ifndef MENU_H
#define MENU_H

#include <RoboVac.h>
#include "callbacks.h"
#include "lcd.h"

// Main Menu
menuEntry_t n_root = {"Main Menu "}; // n = node
menuEntry_t m_setup = {"Setup..."}; // m = menu
menuEntry_t m_man = {"Manual Cntrol..."};
menuEntry_t n_mon = {"Monitor..."};

// Setup menu
menuEntry_t n_setup = {"Setup Menu "};
menuEntry_t n_port =   {"Port/ID Setup..."};
menuEntry_t n_travel = {"Travel Adjust..."};

// Manual Control
menuEntry_t n_manc = {"Manual Control "};
menuEntry_t n_portt = {"Port Toggle..."};
menuEntry_t n_vact = {"Vacuum Toggle..."};

void menuSetup(void) {

    // Main Menu
    n_root.next_sibling = &m_setup;
    m_setup.next_sibling = &m_man;
    m_setup.prev_sibling = &n_root;
    m_man.next_sibling = &n_mon;
    m_man.prev_sibling = &m_setup;
    n_mon.prev_sibling = &m_man;

    m_setup.child = &n_port;
    m_man.child = &n_portt;
    // monitor is a node w/ callback

    // Setup menu
    n_setup.next_sibling = &n_port;
    n_port.next_sibling = &n_travel;
    n_port.prev_sibling = &n_setup;
    n_travel.prev_sibling = &n_port;

    // Manual Control
    n_manc.next_sibling = &n_portt;
    n_portt.next_sibling = &n_vact;
    n_portt.prev_sibling = &n_manc;
    n_vact.prev_sibling = &n_portt;

    // Callbacks
    n_mon.callback = monitorCallback;
    n_port.callback = portIDSetupCallback;
    n_travel.callback = travelAdjustCallback;
    n_portt.callback = portToggleCallback;
    n_vact.callback = vacToggleCallback;

    currentCallback = NULL;
    currentMenu = &m_setup;
    lastButtonChange = millis();
    lcdButtons = 0;
    lcdState = LCD_ENDSTATE;
}

#endif // MENU_H
