#ifndef MENU_BITMAPS_H
#define MENU_BITMAPS_H

#include "globals.h"

#define FONT_TINY  0
#define FONT_SMALL 1
#define FONT_BIG   2

#define DATA_TIMER 0
#define DATA_SCORE 1
#define DATA_LEVEL 2

uint8_t blinkingFrames = 0;
uint8_t sparkleFrames = 0;
uint8_t cont = 0;
uint8_t helpFrames = 0;

extern void drawNumbers(uint8_t numbersX, uint8_t numbersY, uint8_t fontType, uint8_t data);

void drawTitleScreen() {
    if(everyXFrames(8)) blinkingFrames = (blinkingFrames + 1) % 32;
    for(uint8_t i = 0; i < 4; i++)
        gfx_sprite_self_masked(32 * i, 0, titleScreen, i);
    gfx_sprite_self_masked(85, 45, badgeMysticBalloon, 0);
    gfx_sprite_self_masked(79, 43, stars, sparkleFrames);
    gfx_sprite_self_masked(9, 9, leftGuyLeftEye, blinkingEyesLeftGuy[blinkingFrames]);
    gfx_sprite_self_masked(15, 13, leftGuyRightEye, blinkingEyesLeftGuy[blinkingFrames]);
    gfx_sprite_self_masked(109, 34, rightGuyEyes, blinkingEyesRightGuy[blinkingFrames]);
}

void stateMenuIntro() {
    globalCounter++;
    if(globalCounter < 160) {
        gfx_sprite_self_masked(34, 4, T_arg, 0);
    } else {
        drawTitleScreen();
    }
    if((globalCounter > 250) || justPressed(MYBL_OK | MYBL_BACK)) {
        gameState = STATE_MENU_MAIN;
        playTone(425, 20);
    }
}

void stateMenuMain() {
    drawTitleScreen();
    gfx_sprite_overwrite(51, 9, mainMenu, 0);
    if(justPressed(MYBL_DOWN) && (menuSelection < 5)) {
        menuSelection++;
        playTone(300, 20);
    }
    if(justPressed(MYBL_UP) && (menuSelection > 2)) {
        menuSelection--;
        playTone(300, 20);
    }
    if(justPressed(MYBL_OK)) {
        if(menuSelection == STATE_MENU_HELP) helpFrames = 0;
        gameState = menuSelection;
        playTone(425, 20);
    }
    if(justPressed(MYBL_BACK)) {
        exitRequested = true;
    }
    gfx_sprite_plus_mask(46, 9 + 9 * (menuSelection - 2), selector_plus_mask, 0);
}

void stateMenuHelp() {
    if(helpFrames < 50) {
        gfx_sprite_self_masked(34, 4, T_arg, 0);
        helpFrames++;
    } else {
        gfx_sprite_self_masked(43, 10, badgeMysticBalloon, 0);
        gfx_sprite_self_masked(40, 38, madeBy, 0);
    }
    if(justPressed(MYBL_BACK | MYBL_OK)) {
        helpFrames = 0;
        gameState = STATE_MENU_MAIN;
        playTone(425, 20);
    }
}

void stateMenuInfo() {
    gfx_sprite_self_masked(43, 2, badgeMysticBalloon, 0);
    gfx_sprite_self_masked(37, 0, stars, sparkleFrames);
    gfx_sprite_self_masked(40, 48, madeBy, 0);
    scorePlayer = save.highscore;
    if(save.coinsHighscore == TOTAL_COINS) {
        gfx_sprite_self_masked(21, 28, badgeSuper, 0);
    } else {
        gfx_sprite_self_masked(28, 28, badgeBorder, 0);
    }
    gfx_sprite_self_masked(30, 28, badgeHighScore, 0);
    drawNumbers(55, 30, FONT_BIG, DATA_SCORE);
    if(justPressed(MYBL_BACK | MYBL_OK)) {
        gameState = STATE_MENU_MAIN;
        playTone(425, 20);
    }
}

void stateMenuSoundfx() {
    drawTitleScreen();
    gfx_sprite_overwrite(51, 9, soundMenu, 0);
    if(justPressed(MYBL_DOWN)) {
        soundEnabled = true;
        playTone(300, 20);
    }
    if(justPressed(MYBL_UP)) soundEnabled = false;
    gfx_sprite_plus_mask(54, 18 + 9 * soundEnabled, selector_plus_mask, 0);
    if(justPressed(MYBL_BACK | MYBL_OK)) {
        gameState = STATE_MENU_MAIN;
        playTone(425, 20);
    }
}

void stateMenuPlaySelect() {
    drawTitleScreen();
    gfx_sprite_overwrite(53, 18, continueMenu, 0);
    if(justPressed(MYBL_DOWN)) {
        cont = 1;
        playTone(300, 20);
    }
    if(justPressed(MYBL_UP)) {
        cont = 0;
        playTone(300, 20);
    }
    gfx_sprite_plus_mask(48, 18 + 9 * cont, selector_plus_mask, 0);
    if(justPressed(MYBL_OK)) {
        gameState = STATE_GAME_PLAYCONTNEW + cont;
        cont = 0;
        playTone(425, 20);
    }
    if(justPressed(MYBL_BACK)) {
        gameState = STATE_MENU_MAIN;
        playTone(425, 20);
    }
}

#endif
