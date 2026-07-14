#ifndef MENU_BITMAPS_H
#define MENU_BITMAPS_H

#include "../lib/Arduino.h"
#include "globals.h"

#define FONT_TINY  0
#define FONT_SMALL 1
#define FONT_BIG   2

#define DATA_TIMER 0
#define DATA_SCORE 1
#define DATA_LEVEL 2

byte blinkingFrames = 0;
byte sparkleFrames = 0;
byte cont = 0;
byte helpFrames = 0;

extern void drawNumbers(byte numbersX, byte numbersY, byte fontType, byte data);

void drawTitleScreen() {
    if(arduboy.everyXFrames(8)) blinkingFrames = (blinkingFrames + 1) % 32;
    for(byte i = 0; i < 4; i++)
        sprites.drawSelfMasked(32 * i, 0, titleScreen, i);
    sprites.drawSelfMasked(85, 45, badgeMysticBalloon, 0);
    sprites.drawSelfMasked(79, 43, stars, sparkleFrames);
    sprites.drawSelfMasked(
        9, 9, leftGuyLeftEye, pgm_read_byte(&blinkingEyesLeftGuy[blinkingFrames]));
    sprites.drawSelfMasked(
        15, 13, leftGuyRightEye, pgm_read_byte(&blinkingEyesLeftGuy[blinkingFrames]));
    sprites.drawSelfMasked(
        109, 34, rightGuyEyes, pgm_read_byte(&blinkingEyesRightGuy[blinkingFrames]));
}

void stateMenuIntro() {
    globalCounter++;
    drawTitleScreen();
    if((globalCounter > 60)) {
        gameState = STATE_MENU_MAIN;
        sound.tone(425, 20);
    }
}

void stateMenuMain() {
    drawTitleScreen();
    sprites.drawOverwrite(51, 9, mainMenu, 0);
    if(arduboy.justPressed(DOWN_BUTTON) && (menuSelection < 5)) {
        menuSelection++;
        sound.tone(300, 20);
    }
    if(arduboy.justPressed(UP_BUTTON) && (menuSelection > 2)) {
        menuSelection--;
        sound.tone(300, 20);
    }
    if(arduboy.justPressed(A_BUTTON | B_BUTTON)) {
        if(menuSelection == STATE_MENU_HELP) helpFrames = 0;
        gameState = menuSelection;
        sound.tone(425, 20);
    }
    sprites.drawPlusMask(46, 9 + 9 * (menuSelection - 2), selector_plus_mask, 0);
}

void stateMenuHelp() {
    if(helpFrames < 50) {
        sprites.drawSelfMasked(34, 4, T_arg, 0);
        helpFrames++;
    } else {
        sprites.drawSelfMasked(43, 10, badgeMysticBalloon, 0);
        sprites.drawSelfMasked(40, 38, madeBy, 0);
    }
    if(arduboy.justPressed(A_BUTTON | B_BUTTON)) {
        helpFrames = 0;
        gameState = STATE_MENU_MAIN;
        sound.tone(425, 20);
    }
}

void stateMenuInfo() {
    sprites.drawSelfMasked(43, 2, badgeMysticBalloon, 0);
    sprites.drawSelfMasked(37, 0, stars, sparkleFrames);
    sprites.drawSelfMasked(40, 48, madeBy, 0);
    EEPROM.get(OFFSET_HSCORE, scorePlayer);
    if(EEPROM.read(OFFSET_COINSHS) == TOTAL_COINS) {
        sprites.drawSelfMasked(21, 28, badgeSuper, 0);
    } else {
        sprites.drawSelfMasked(28, 28, badgeBorder, 0);
    }
    sprites.drawSelfMasked(30, 28, badgeHighScore, 0);
    drawNumbers(55, 30, FONT_BIG, DATA_SCORE);
    if(arduboy.justPressed(A_BUTTON | B_BUTTON)) {
        gameState = STATE_MENU_MAIN;
        sound.tone(425, 20);
    }
}

void stateMenuSoundfx() {
    drawTitleScreen();
    sprites.drawOverwrite(51, 9, soundMenu, 0);
    if(arduboy.justPressed(DOWN_BUTTON)) {
        arduboy.audio.on();
        sound.tone(300, 20);
    }
    if(arduboy.justPressed(UP_BUTTON)) arduboy.audio.off();
    sprites.drawPlusMask(54, 18 + 9 * arduboy.audio.enabled(), selector_plus_mask, 0);
    if(arduboy.justPressed(A_BUTTON | B_BUTTON)) {
        arduboy.audio.saveOnOff();
        gameState = STATE_MENU_MAIN;
        sound.tone(425, 20);
    }
}

void stateMenuPlaySelect() {
    drawTitleScreen();
    sprites.drawOverwrite(53, 18, continueMenu, 0);
    if(arduboy.justPressed(DOWN_BUTTON)) {
        cont = 1;
        sound.tone(300, 20);
    }
    if(arduboy.justPressed(UP_BUTTON)) {
        cont = 0;
        sound.tone(300, 20);
    }
    sprites.drawPlusMask(48, 18 + 9 * cont, selector_plus_mask, 0);
    if(arduboy.justPressed(B_BUTTON)) {
        gameState = STATE_GAME_PLAYCONTNEW + cont;
        cont = 0;
        sound.tone(425, 20);
    }
    if(arduboy.justPressed(A_BUTTON)) {
        gameState = STATE_MENU_MAIN;
        sound.tone(425, 20);
    }
}

#endif
