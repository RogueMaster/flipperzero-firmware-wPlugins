#pragma once

#include "Arduboy2Ext.h"

class FadeEffects {
private:
    uint8_t fadeWidth = WIDTH;

public:
    void complete(void) {
        this->fadeWidth = 0;
    }

    void reset(void) {
        this->fadeWidth = WIDTH;
    }

    bool isComplete(void) const {
        return (this->fadeWidth == 0);
    }

    void update(void) {
        if(this->fadeWidth >= 3) {
            this->fadeWidth = this->fadeWidth - 3;
        } else {
            this->fadeWidth = 0;
        }
    }

    void draw(Arduboy2Base& arduboy) const {
#ifdef ARDUINO_ARCH_AVR

        uint8_t* buffer = arduboy.sBuffer;
        asm volatile("ldi   r23, %[rows]         \n" // r = HEIGHT / 8
                     "1:                         \n" // Z = sBuffer
                     "movw  r26, r30             \n" // X = sBuffer + WIDTH
                     "subi  r26, lo8(-%[cols])   \n"
                     "sbci  r27, hi8(-%[cols])   \n"
                     "mov  r25, %[width]        \n" // temp_fadeWidth
                     "2:                        \n"
                     "ld    r24, Z               \n"
                     "andi  r24, 0x55            \n" // *Z &= 0x55
                     "st    Z+, r24              \n"
                     "ld    r24, -X              \n" // *X &= 0xAA
                     "andi  r24, 0xAA            \n"
                     "st    X, r24               \n"
                     "dec   r25                  \n"
                     "brne  2b                   \n" // while --temp_fadeWidth != 0
                     "ldi   r24, %[cols]         \n"
                     "sub   r24, %[width]        \n"
                     "add   r30, r24             \n" // Z += WIDTH - fadeWidth
                     "adc   r31, r1              \n"
                     "dec   r23                  \n"
                     "brne  1b                   \n" // while --r != 0
                     : "+&z"(buffer)
                     : [width] "r"(this->fadeWidth), [cols] "i"(WIDTH), [rows] "i"(HEIGHT / 8)
                     : "r23", "r24", "r25", "r26", "r27");

#else

        for(uint8_t r = 0; r < HEIGHT / 8; r++) {
            for(uint8_t i = 0; i < this->fadeWidth; i++) {
                arduboy.sBuffer[r * WIDTH + i] &= 0x55;
                arduboy.sBuffer[r * WIDTH + WIDTH - 1 - i] &= 0xAA;
            }
        }

#endif
    }
};
