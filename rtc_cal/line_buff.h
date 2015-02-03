#ifndef LINE_BUFF_H
#define LINE_BUFF_H

typedef int (*AvailableCallback)(void);
typedef char (*ReadByteCallback)(void);
typedef void (*PrintByteCallback)(char);

template <bool echo = false,
          bool strip = false,  // blank line
          size_t lineBufLen = 64>
class LineBuff {
    private:
    char buff[lineBufLen];
    size_t length;
    uint8_t status;
    const char BACKSPACE = 0x08;
    const char DELETE = 0x7F;
    const char ESCAPE = 0x1B;
    const char ENTER = '\n';
    const char RETURN = '\r';

    // one for null, five in case of overflow, plus at least one char.
    static_assert(lineBufLen > 7, "lineBufLen must be greater than four");

    void backspace(void) {
        if (length > 0)
            buff[--length] = '\0';
        // This needs special handling
        if (echo) {
            Serial.print(BACKSPACE);
            Serial.print(" ");
            Serial.print(BACKSPACE);
        }
    }

    bool isReady(void) { return status & 1; }

    bool isOverflow(void) { return status & 2; }

    void setReady(void) { status |= 1; }

    void setOverflow(void) { status |= 2; }

    const char* returnCheckOverflow(void) {
        if (isOverflow())
            memcpy(&buff[lineBufLen - 7], "<...>", 5);
        return buff;
    }

    public:
    // Unconditionally empty/reset buffer state
    void flush(void) {
        status = 0;
        length = 0;
        memset(buff, 0, lineBufLen);
    }

    // Constructor
    LineBuff(void) : length(0), status(0) { flush(); };

    // Return pointer to complete line or NULL if none received
    const char* gotLine(void) {
        if (isReady())
            return returnCheckOverflow(); // don't consume any more chars

        if (Serial.available() == 0)
            return NULL;

        char nextChar = Serial.read();
        if ((nextChar == ENTER) || (nextChar == RETURN)) {
            if (strip && (length == 0))
                return NULL;
            setReady();
            return returnCheckOverflow();
        }

        if (length && ((nextChar == BACKSPACE) || (nextChar == DELETE))) {
            backspace();
            return NULL;
        }

        if (nextChar == ESCAPE) {
            if (echo)
                while (length > 0)
                    backspace(); // fill length with ' ', (CTRL-L??)
            flush();  // Reset entire state
            return NULL;
        }

        if (nextChar < 0x20)
            return NULL;  // ignore special characters

        if (echo)
            Serial.print(nextChar);

        // Deal with possible overflow
        buff[length] = nextChar;
        // Guarantee always room for trailing \0
        if (length < (lineBufLen - 2))
            length++;
        else // signal overflow
            setOverflow();
        return NULL;
    }

    // Return length of current line or 0 if no line received
    size_t gotLineLen(void) {
        if (isReady())
            return strnlen(buff, lineBufLen);
        else
            return 0;
    }

    // Block until receiving a complete line, return line.
    const char *gotLineBlock(void) {
        while (!gotLineLen())
            gotLine();
        return returnCheckOverflow();
    }

    // Return true if input caused overflow
    bool didOverflow(void) { return isOverflow(); }
};

#endif // LINE_BUFF_H
