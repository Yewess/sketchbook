#define EEPDEBUG
#include <eep.h>
#include "eep_test.h"

void setup(void) {
    Serial.begin(115200);
    delay(1);  // Serial client sometimes needs extra time to initialize
    Serial.println(F("\nSetup()"));

    // DO NOT USE _data any other way!
    Eep_type eep(defaults, &_data);

    #ifdef EEPDEBUG
    eep.dump();
    #endif //EEPDEBUG

    // Data loaded during initialization
    // otherwise validate with data = eep.load()
    Data* data = eep.data();
    if (data) {
        Serial.println(F("EEPRom content is valid"));
        Serial.print(F("The data is: "));
        Serial.print(data->h);
        Serial.print(F(" "));
        Serial.print(data->w);
        Serial.print(F(" answer is: "));
        Serial.println(data->answer);
        if (data->answer == 24) {
            // Can update buffer directly
            data->answer = 42;
            memcpy(&data->h, "hello", 7);
            memcpy(&data->w, "world", 7);
            if (eep.save())
                Serial.println(F("Data updated"));
            else
                Serial.println(F("Update failed"));
        } else {
            // Or from a new instance
            Data newdata;
            newdata.answer = 24;
            memcpy(&newdata.h, "world", 7);
            memcpy(&newdata.w, "hello", 7);
            if (eep.save(&newdata))
                Serial.println(F("Data updated"));
            else
                Serial.println(F("Update failed"));
        }
    } else
        Serial.println(F("Eeprom content is invalid!"));
}

void loop(void) {
    // New varible, do not reset data to defaults
    Eep_type eep(&_data);  // DO NOT USE _data any other way!
    #ifdef EEPDEBUG
    eep.dump();
    #endif //EEPDEBUG
    Data* data = eep.data();
    if (data) {
        Serial.println(F("EEPRom content is still valid"));
        Serial.print(F("The data is: "));
        Serial.print(data->h);
        Serial.print(F(" "));
        Serial.print(data->w);
        Serial.print(F(" answer is: "));
        Serial.println(data->answer);
    } else
        Serial.println(F("Eeprom content is invalid!"));
    while (true)
        delay(1000);
}
