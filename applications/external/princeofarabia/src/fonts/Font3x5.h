#pragma once

#include <lib/Arduino.h>
#include <lib/Print.h>

class Font3x5 : public Print {
public:
    Font3x5(uint8_t lineSpacing = 8);

    virtual size_t write(uint8_t); // used by the Arduino Print class
    void printChar(const char c, const int8_t x, int8_t y);
    void setCursor(const int8_t x, const int8_t y);

    void setTextColor(const uint8_t color);
    void setHeight(const uint8_t color);

private:
    int8_t _cursorX; // Default is 0.
    int8_t _baseX; // needed for linebreak.
    int8_t _cursorY; // Default is 0.

    int8_t _textColor; // BLACK == 0, everything else is WHITE. Default is WHITE.

    uint8_t _letterSpacing; // letterSpacing controls the distance between letters. Default is 1.
    uint8_t
        _lineHeight; // lineHeight controls the height between lines breakend by \n. Default is 8.
};
