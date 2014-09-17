#include <avr/pgmspace.h>
#include <Arduino.h>
#include <SerialUI.h>
#include "serialControl.h"

#define SUI_SERIALUI_ECHO_ON
#undef SUI_DYNAMIC_MEMORY_ALLOCATION_ENABLE

Atmega328p atmega328p;

/*
 * Menu Structure

+ Read Value -----+ Digital
|                 |
|                 |
|                 + Analog
|
|
+ Write Value ----+ High
|                 |
|                 |
|                 + Low
|
|
+ Change Mode --- + Input
                  |
                  |
                  + Input w/ Pullup
                  |
                  |
                  + Output

 * Set up static serial menu strings
 */
SUI_DeclareString(greeting, "\nMain Menu\n");

SUI_DeclareString(read_key, "r");
SUI_DeclareString(read_help, "Read pin value");

SUI_DeclareString(write_key, "w");
SUI_DeclareString(write_help, "Write pin value");

SUI_DeclareString(change_key, "c");
SUI_DeclareString(change_help, "Change pin mode");

SUI_DeclareString(d_read_key, "d");
SUI_DeclareString(d_read_help, "Digital read");

SUI_DeclareString(a_read_key, "a");
SUI_DeclareString(a_read_help, "Analog read");

SUI_DeclareString(all_read_key, "A");
SUI_DeclareString(all_read_help, "Read All pins");

SUI_DeclareString(write_h_key, "1");
SUI_DeclareString(write_h_help, "Set output HIGH");

SUI_DeclareString(write_l_key, "0");
SUI_DeclareString(write_l_help, "Set output LOW");

SUI_DeclareString(change_i_key, "i");
SUI_DeclareString(change_i_help, "Change pin to input");

SUI_DeclareString(change_I_key, "I");
SUI_DeclareString(change_I_help, "Change pin to input w/ pullup");

SUI_DeclareString(change_o_key, "o");
SUI_DeclareString(change_o_help, "Change pin to output");

/*
 * Globals
 */

SUI::SerialUI sui = SUI::SerialUI(greeting);

/*
 * Functions / Methods
 */

void showPins() {
    Serial.println("");
    Serial.println(F("-----------------------------------------------------"));
    for (uint8_t l=0; l < atmega328p.NPINS / 2; l++) {
        const uint8_t buffLen = 30;
        char buff[buffLen] = {0};
        static const char errStr[] = "-ERROR-";
        if (!atmega328p.pin[l].strDetail(buff, buffLen))
            strcpy(buff, errStr);
        Serial.print(buff);
        // Extra ' ' need to reach column 30
        for (uint8_t s = 30 - strlen(buff); s > 0; s--)
            Serial.print(" ");
        uint8_t r = l + 10;
        if (!atmega328p.pin[r].strDetail(buff, buffLen))
            strcpy(buff, errStr);
        Serial.println(buff);
    }
    Serial.println(F("\n(Type ?[ENTER] for help)"));
    Serial.println(F("-----------------------------------------------------"));
}

int8_t readPinNumber(void) {
    Serial.print(F("Enter pin number(0-"));
    Serial.print(atmega328p.NPINS);
    Serial.println(F(") or -1 to cancel:"));
    sui.showEnterNumericDataPrompt();
    int value = sui.parseInt();
    if ((value < 0) || (value > atmega328p.NPINS)) {
        Serial.println("selection canceled");
        value = -1;
    }
    return value;
}

void MenuCB::digitalRead(void) {
    //sui.currentMenu()->showHelp();
    int8_t pinNumber = readPinNumber();
    if (pinNumber != -1)
        atmega328p.pin[pinNumber].readDigital();
    sui.setCurrentMenu(sui.topLevelMenu());
    showPins();  // display all pins & state
}

void MenuCB::analogRead(void) {
    //sui.currentMenu()->showHelp();
    int8_t pinNumber = readPinNumber();
    if (pinNumber != -1)
        if (atmega328p.pin[pinNumber].isAnalog())
            atmega328p.pin[pinNumber].readAnalog();
        else
            Serial.println("Not an analog pin!");
    sui.setCurrentMenu(sui.topLevelMenu());
    showPins();  // display all pins & state
}

void MenuCB::allRead(void) {
    for (uint8_t p = 0; p < atmega328p.NPINS; p++)
        atmega328p.pin[p].readDigital();
    sui.setCurrentMenu(sui.topLevelMenu());
    showPins();  // display all pins & state
}

void MenuCB::writeHigh(void) {
    //sui.currentMenu()->showHelp();
    int8_t pinNumber = readPinNumber();
    if (pinNumber != -1)
        atmega328p.pin[pinNumber].writeHigh();
    sui.setCurrentMenu(sui.topLevelMenu());
    showPins();  // display all pins & state
}

void MenuCB::writeLow(void) {
    //sui.currentMenu()->showHelp();
    int8_t pinNumber = readPinNumber();
    if (pinNumber != -1)
        atmega328p.pin[pinNumber].writeLow();
    sui.setCurrentMenu(sui.topLevelMenu());
    showPins();  // display all pins & state
}

void MenuCB::setInput(void) {
    //sui.currentMenu()->showHelp();
    int8_t pinNumber = readPinNumber();
    if (pinNumber != -1)
        atmega328p.pin[pinNumber].modeInput();
    sui.setCurrentMenu(sui.topLevelMenu());
    showPins();  // display all pins & state
}

void MenuCB::setInputPullup(void) {
    //sui.currentMenu()->showHelp();
    int8_t pinNumber = readPinNumber();
    if (pinNumber != -1)
        atmega328p.pin[pinNumber].modeInputPullup();
    sui.setCurrentMenu(sui.topLevelMenu());
    showPins();  // display all pins & state
}

void MenuCB::setOutput(void) {
    //sui.currentMenu()->showHelp();
    int8_t pinNumber = readPinNumber();
    if (pinNumber != -1)
        atmega328p.pin[pinNumber].modeOutput();
    sui.setCurrentMenu(sui.topLevelMenu());
    showPins();  // display all pins & state
}

void setup(void) {
    sui.begin(115200);
    delay(1);

    sui.setReadTerminator('\n');
    sui.setTimeout(-1);  // timeout for reads
    sui.setMaxIdleMs(-1);  // checking for more menu input

    SUI::Menu* mainMenu = sui.topLevelMenu();

    SUI::Menu* readMenu = mainMenu->subMenu(read_key, read_help);
    SUI::Menu* writeMenu = mainMenu->subMenu(write_key, write_help);
    SUI::Menu* changeMenu = mainMenu->subMenu(change_key, change_help);

    readMenu->addCommand(d_read_key, MenuCB::digitalRead, d_read_help);
    readMenu->addCommand(a_read_key, MenuCB::analogRead, a_read_help);
    readMenu->addCommand(all_read_key, MenuCB::allRead, all_read_help);

    writeMenu->addCommand(write_h_key, MenuCB::writeHigh, write_h_help);
    writeMenu->addCommand(write_l_key, MenuCB::writeLow, write_l_help);

    changeMenu->addCommand(change_i_key, MenuCB::setInput, change_i_help);
    changeMenu->addCommand(change_I_key, MenuCB::setInputPullup, change_I_help);
    changeMenu->addCommand(change_o_key, MenuCB::setOutput, change_o_help);

    if ((!mainMenu) || (!readMenu) || (!writeMenu) || (!changeMenu)) {
        Serial.println(F("\n\nERROR ALLOCATING MENU SYSTEM\n\n"));
        for(;;) {}
    }
    for (uint8_t p=0; p < atmega328p.NPINS; p++) {
        atmega328p.pin[p].modeInput();
        atmega328p.pin[p].readDigital();
    }
}

void loop(void) {
    if (sui.checkForUser(-1)) {
        showPins();
        sui.enter();  // show the greeting message and prompt
        //sui.currentMenu()->showHelp();
        while (sui.userPresent())
            sui.handleRequests();
    }
}
