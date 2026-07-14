#ifndef ELEMENTS_H
#define ELEMENTS_H

#include "../lib/Arduino.h"
#include "globals.h"

#define FONT_TINY  0
#define FONT_SMALL 1
#define FONT_BIG   2

#define DATA_TIMER 0
#define DATA_SCORE 1
#define DATA_LEVEL 2

void drawBalloonLives() {
    for(byte i = 0; i < kid.balloons; ++i) {
        sprites.drawOverwrite((i * 7) + 2, 0, elementsHUD, 10);
    }
}

void drawCoinHUD() {
    //for (byte i = 0; i < MAX_PER_TYPE; ++i)
    for(byte i = MAX_PER_TYPE - 1; i < MAX_PER_TYPE; --i) {
        if(i >= MAX_PER_TYPE - coinsActive)
            sprites.drawOverwrite(40 + (i * 6), 0, elementsHUD, 11);
        else
            sprites.drawOverwrite(40 + (i * 6), 0, elementsHUD, 12);
    }
}

void drawNumbers(byte numbersX, byte numbersY, byte fontType, byte data) {
    char buf[10];
    char charLen = 0;
    char pad = 0;

    switch(data) {
    case DATA_SCORE:
        snprintf(buf, sizeof(buf), "%lu", (unsigned long)scorePlayer);
        charLen = strlen(buf);
        pad = 6 - charLen;
        sprites.drawSelfMasked(numbersX - 2, numbersY - 2, numbersBigMask, 0);
        //for (byte i = 0; i < 6; i++)
        for(byte i = 5; i <= 5; --i)
            sprites.drawSelfMasked(numbersX + (7 * i), numbersY - 2, numbersBigMask01, 0);
        sprites.drawSelfMasked(numbersX + 41, numbersY - 2, numbersBigMask, 1);
        break;
    case DATA_LEVEL:
        itoa(level + 1, buf, 10);
        charLen = strlen(buf);
        pad = 2 - charLen;
        sprites.drawSelfMasked(numbersX - 2, numbersY - 9, badgeLevel, 0);
        break;
    }

    //draw 0 padding
    for(byte i = 0; i < pad; i++) {
        switch(fontType) {
        case FONT_SMALL:
            sprites.drawOverwrite(numbersX + (6 * i), numbersY, elementsHUD, 0);
            break;
        case FONT_BIG:
            sprites.drawSelfMasked(numbersX + (7 * i), numbersY, numbersBig, 0);
            break;
        }
    }

    for(byte i = 0; i < charLen; i++) {
        char digit = buf[i];
        if(digit <= 48) {
            digit = 0;
        } else {
            digit -= 48;
            if(digit > 9) digit = 0;
        }
        switch(fontType) {
        case FONT_SMALL:
            sprites.drawOverwrite(numbersX + (pad * 6) + (6 * i), numbersY, elementsHUD, digit);
            break;
        case FONT_BIG:
            sprites.drawSelfMasked(numbersX + (pad * 7) + (7 * i), numbersY, numbersBig, digit);
            break;
        }
    }
}

#endif
