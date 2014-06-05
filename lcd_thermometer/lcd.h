#ifndef LCD_H
#define LCD_H

#include <stdlib.h>

typedef class LCD {
    private:
    LiquidCrystal liquidCrystal;
    uint8_t lcdCols;
    uint8_t lcdRows;
    char *lcdLive;

    public:
    // Write data here
    char *lcdWork;

    // Constructors
    LCD(uint8_t rs, uint8_t enable,
        uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
        uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7)
        :
        liquidCrystal(rs, enable, d0, d1, d2, d3,
                      d4, d5, d6, d7) {}

    LCD(uint8_t rs, uint8_t rw, uint8_t enable,
        uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
        uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7)
        :
        liquidCrystal(rs, rw, enable, d0, d1, d2, d3,
                      d4, d5, d6, d7) {}

    LCD(uint8_t rs, uint8_t rw, uint8_t enable,
        uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7)
        :
        liquidCrystal(rs, rw, enable, d4, d5, d6, d7) {}

    LCD(uint8_t rs, uint8_t enable,
        uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7)
        :
        liquidCrystal(rs, enable, d4, d5, d6, d7) {}

    // Initializer
    void begin(uint8_t cols, uint8_t rows, uint8_t charsize = LCD_5x8DOTS) {
        if (lcdLive != NULL)
            return;
        lcdCols = cols;
        lcdRows = rows;
        lcdLive = new char[(cols * rows)];
        lcdWork = new char[(cols * rows)];
        liquidCrystal.begin(cols, rows, charsize);
        liquidCrystal.clear();
        liquidCrystal.noCursor();
        liquidCrystal.noBlink();
        liquidCrystal.noAutoscroll();
        clear();
    }

    // Member Functions
    void update(void) {
        uint16_t idx=0;
        for (uint8_t lcdY=0; lcdY < lcdRows; lcdY++)
            for (uint8_t lcdX=0; lcdX < lcdCols; lcdX++) {
                idx = lcdY * lcdX;
                if (lcdLive[idx] != lcdWork[idx]) {
                    liquidCrystal.setCursor(lcdY, lcdX);
                    liquidCrystal.write(lcdWork[idx]);
                    lcdLive[idx] = lcdWork[idx];
                }
            }
    }

    void clear(void) {
        memset(lcdWork, (int)' ', lcdCols * lcdRows);
    }
} LCD;

#endif // LCD_H
