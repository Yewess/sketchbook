#ifndef EEPROM_H
#define EEPROM_H

#define FLEXVERSION

#include <Eep.h>
#include "tm.h"

class EepromData {
    public:
    uint8_t clkoutPin = 3;
    uint8_t clkoutMode = INPUT_PULLUP;
    uint8_t clkoutIntNo = 1;
    uint8_t clkoutIntMode = FALLING;

    uint8_t intPin = 2;
    uint8_t intMode = INPUT_PULLUP;
    uint8_t intIntNo = 0;
    uint8_t intIntMode = FALLING;

    uint16_t clkoutCfg = SQW_32KHZ;  // from Rtc_Pcf8563.h
    int32_t clkoutHz = 32768;  // ideal rtc clkout pin frequency
    int32_t hzOffset = 0;  // measured offset * 1000 from clkoutHz

    uint16_t cmdModeWindow = 1000;  // ms
    char cmdModeCode[4] = "<<<";

    // Time clock was set or last adjusted for ppmError
    // (not more than once per month)
    clock::TMPod setTime;
};

const EepromData defaults PROGMEM;
typedef Eep::Eep<EepromData, uint32_t, 1, uint32_t, 0x146f4ef9> Eep_type;
Eep_type::Block eemem EEMEM;

#endif // EEPROM_H
