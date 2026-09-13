#ifndef GAME_H
#define GAME_H

#include "globals.h"
#include "inputs.h"
#include "player.h"
#include "enemies.h"
#include "elements.h"
#include "levels.h"

#define TOTAL_TONES 10
static const uint8_t tones[] = {
    //200, 100, 250, 125, 300, 150, 350, 400, 425, 475
    131,
    145,
    139,
    152,
    131,
    172,
    200,
    188,
    213,
    255};

uint8_t toneindex = 0;

void stateMenuPlayNew() {
    level = LEVEL_TO_START_WITH - 1;
    coinsCollected = 0;
    totalCoins = 0;
    balloonsLeft = 0;
    scorePlayer = 0;
    globalCounter = 0;
    kid.balloons = 3;
    gameState = STATE_GAME_NEXT_LEVEL;
    scoreIsVisible = false;
    nextLevelIsVisible = true;
    pressKeyIsVisible = false;
}

void stateMenuPlayContinue() {
    level = save.level;
    totalCoins = save.coins;
    coinsCollected = 0;
    balloonsLeft = 0;
    //scorePlayer = 0;
    scorePlayer = save.score;
    globalCounter = 0;
    kid.balloons = 3;
    gameState = STATE_GAME_NEXT_LEVEL;
    scoreIsVisible = false;
    nextLevelIsVisible = true;
    pressKeyIsVisible = false;
}

void stateGameNextLevel() {
    //if (level < TOTAL_LEVELS)
    //{
    if(everyXFrames(20)) {
        canPressButton = false;
        if(coinsCollected > 0) {
            coinsCollected--;
            scorePlayer += 20;
            playTone(*(tones + toneindex++), 150);
        } else if(balloonsLeft > 0) {
            balloonsLeft--;
            scorePlayer += 30;
            playTone(*(tones + toneindex++), 150);
        } else {
            canPressButton = true;
            scoreIsVisible = false;
            pressKeyIsVisible = !pressKeyIsVisible;
            if(toneindex < TOTAL_TONES) {
                playTone(*(tones + toneindex++), 200);
                toneindex = TOTAL_TONES;
            }
            if(level >= TOTAL_LEVELS) gameState = STATE_GAME_OVER;
        }
    }
    /*}
  else
  {
    gameState = STATE_GAME_OVER;
    return;
  }*/

    // Update the save slot
    saveByte(save.level, level);
    saveByte(save.coins, totalCoins);
    saveLong(save.score, scorePlayer);

    //if (nextLevelIsVisible)
    //{
    if(level < TOTAL_LEVELS) {
        gfx_sprite_self_masked(35, 4, badgeNextLevel, 0);
        drawNumbers(78, 13, FONT_BIG, DATA_LEVEL);
    } else {
        saveByte(save.level, (uint8_t)LEVEL_TO_START_WITH - 1);
        // Score remains after completing game? (no)
        saveLong(save.score, 0);
    }
    drawNumbers(43, 49, FONT_BIG, DATA_SCORE);
    //}

    if(scoreIsVisible) {
        uint8_t totalBadges = coinsCollected + balloonsLeft;

        for(uint8_t i = 0; i < totalBadges; ++i) {
            if(i < coinsCollected)
                gfx_sprite_overwrite(65 - (7 * totalBadges) + (i * 14), 27, badgeElements, 0);
            else
                gfx_sprite_overwrite(65 - (7 * totalBadges) + (i * 14), 27, badgeElements, 1);
        }
    }

    if(canPressButton) {
        if(pressKeyIsVisible) gfx_sprite_overwrite(38, 29, badgePressKey, 0);
        if(justPressed(MYBL_BACK | MYBL_OK)) {
            toneindex = 0;
            playTone(425, 20);
            setKid();
            //cam.pos = vec2(0, 0);
            cam.pos = vec2(0, LEVEL_HEIGHT - 64);
            cam.offset = vec2(0, 0);
            enemiesInit();
            levelLoad(levels[level]);
            gameState = STATE_GAME_PLAYING;
        }
    }
}

void stateGamePlaying() {
    checkInputs();
    checkKid();
    updateCamera();

    drawGrid();
    enemiesUpdate();

    drawKid();
    drawHUD();

    checkCollisions();
}

void stateGamePause() {
    gfx_sprite_self_masked(47, 17, badgePause, 0);
    if(justPressed(MYBL_OK)) {
        gameState = STATE_GAME_PLAYING;
    }
    if(justPressed(MYBL_BACK)) {
        gameState = STATE_MENU_MAIN;
    }
}

void stateGameOver() {
    uint8_t x = 35 + 12;
    if(level < TOTAL_LEVELS) {
        drawNumbers(78, 26, FONT_BIG, DATA_LEVEL);
        x -= 12;
    }
    gfx_sprite_self_masked(x, 17, badgeGameOver, 0);
    drawNumbers(43, 49, FONT_BIG, DATA_SCORE);

    if(scorePlayer > save.highscore) {
        saveByte(save.coinsHighscore, totalCoins);
        saveLong(save.highscore, scorePlayer);
    }

    if(justPressed(MYBL_BACK | MYBL_OK)) {
        gameState = STATE_MENU_MAIN;
    }
}

#endif
