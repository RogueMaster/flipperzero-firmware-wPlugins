#ifndef ELEMENTS_H
#define ELEMENTS_H

#include "globals.h"

#define FONT_TINY  0
#define FONT_SMALL 1
#define FONT_BIG   2

#define DATA_TIMER 0
#define DATA_SCORE 1
#define DATA_LEVEL 2

void drawBalloonLives() {
    for(uint8_t i = 0; i < kid.balloons; ++i) {
        gfx_sprite_overwrite((i * 7) + 2, 0, elementsHUD, 10);
    }
}

void drawCoinHUD() {
    //for (uint8_t i = 0; i < MAX_PER_TYPE; ++i)
    for(uint8_t i = MAX_PER_TYPE - 1; i < MAX_PER_TYPE; --i) {
        if(i >= MAX_PER_TYPE - coinsActive)
            gfx_sprite_overwrite(40 + (i * 6), 0, elementsHUD, 11);
        else
            gfx_sprite_overwrite(40 + (i * 6), 0, elementsHUD, 12);
    }
}

uint8_t numberToDigits(unsigned long value, char* out) {
    char reversed[10];
    uint8_t length = 0;

    do {
        reversed[length++] = (char)('0' + (value % 10));
        value /= 10;
    } while(value > 0 && length < sizeof(reversed));

    for(uint8_t i = 0; i < length; i++)
        out[i] = reversed[length - 1 - i];

    return length;
}

void drawNumbers(uint8_t numbersX, uint8_t numbersY, uint8_t fontType, uint8_t data) {
    char buf[10];
    char charLen = 0;
    char pad = 0;

    switch(data) {
    case DATA_SCORE:
        charLen = (char)numberToDigits(scorePlayer, buf);
        pad = 6 - charLen;
        gfx_sprite_self_masked(numbersX - 2, numbersY - 2, numbersBigMask, 0);
        //for (uint8_t i = 0; i < 6; i++)
        for(uint8_t i = 5; i <= 5; --i)
            gfx_sprite_self_masked(numbersX + (7 * i), numbersY - 2, numbersBigMask01, 0);
        gfx_sprite_self_masked(numbersX + 41, numbersY - 2, numbersBigMask, 1);
        break;
    case DATA_LEVEL:
        charLen = (char)numberToDigits((unsigned long)(level + 1), buf);
        pad = 2 - charLen;
        gfx_sprite_self_masked(numbersX - 2, numbersY - 9, badgeLevel, 0);
        break;
    }

    //draw 0 padding
    for(uint8_t i = 0; i < pad; i++) {
        switch(fontType) {
        case FONT_SMALL:
            gfx_sprite_overwrite(numbersX + (6 * i), numbersY, elementsHUD, 0);
            break;
        case FONT_BIG:
            gfx_sprite_self_masked(numbersX + (7 * i), numbersY, numbersBig, 0);
            break;
        }
    }

    for(uint8_t i = 0; i < charLen; i++) {
        char digit = buf[i];
        if(digit <= 48) {
            digit = 0;
        } else {
            digit -= 48;
            if(digit > 9) digit = 0;
        }
        switch(fontType) {
        case FONT_SMALL:
            gfx_sprite_overwrite(numbersX + (pad * 6) + (6 * i), numbersY, elementsHUD, digit);
            break;
        case FONT_BIG:
            gfx_sprite_self_masked(numbersX + (pad * 7) + (7 * i), numbersY, numbersBig, digit);
            break;
        }
    }
}

#endif
