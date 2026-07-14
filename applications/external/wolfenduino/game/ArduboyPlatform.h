#pragma once

//#include <avr/pgmspace.h>
#include "lib/Arduboy2.h"
#include "lib/ArduboyFX.h"
#include "game/Platform.h"

extern Arduboy2Base arduboy;

extern int y_lut[];

inline uint8_t* GetScreenBuffer() {
    return arduboy.sBuffer;
}

inline void setPixel(uint8_t x, uint8_t y) {
    arduboy.drawPixel(x, y, BLACK);
}

inline void clearPixel(uint8_t x, uint8_t y) {
    arduboy.drawPixel(x, y, WHITE);
}

inline void clearDisplay(uint8_t colour) {
    uint8_t data = colour ? 0xff : 0;
    uint8_t* ptr = arduboy.sBuffer;
    int count = 128 * 64 / 8;
    while(count--)
        *ptr++ = data;
}

class ArduboyPlatform : public PlatformBase {
public:
    void playSound(uint8_t id);
    void update();
};

void PLATFORM_ERROR(const char* msg);
extern ArduboyPlatform Platform;

inline void diskRead(uint24_t address, uint8_t* buffer, int length) {
    FX::readDataBytes(address, buffer, length);
}

inline void writeSaveFile(uint8_t* buffer, int length) {
    FX::saveGameState(buffer, length);
}

inline bool readSaveFile(uint8_t* buffer, int length) {
    return FX::loadGameState(buffer, length) != 0;
}
