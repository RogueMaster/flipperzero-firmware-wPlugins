#ifndef MENU_H
#define MENU_H

#include "globals.h"
#include "player.h"
#include "text.h"
#include "songs.h"
#include "people.h"

void stateMenuIntro() {
    arduboy.fillScreen(0);
    /*if (globalCounter < 88)
  {
    fillWithWord(0, 197);
    drawTextBox(43, 23, WHITE);
    globalCounter++;
  }
  else if (globalCounter < 96)
  {
    textRollAmount = 0;
    globalCounter++;
  }
  else */
    if(globalCounter < 60) {
        fillWithSentence(2);
        drawTextBox(31, 17, WHITE);
        globalCounter++;
    } else {
        //if (arduboy.everyXFrames(48)) frameBoolean = !frameBoolean;
        sprites.drawSelfMasked(52, 16, titleSwordGuard, 0);
        sprites.drawSelfMasked(60, 0, titleSwordGrip, 0);
        sprites.drawSelfMasked(16, 24, titleText, 0);
        sprites.drawSelfMasked(61, 48, titleSword, 0);
        //if (frameBoolean)
        if(arduboy.frameCount() % 100 < 50) {
            sprites.drawErase(61, 52, titleStartMask, 0);
            fillWithWord(0, 148);
            drawTextBox(49, 53, WHITE);
        }
        //    if (arduboy.justPressed(A_BUTTON | B_BUTTON))
        //    {
        //      globalCounter = 0;
        //      gameState = STATE_MENU_MAIN;
        //      if (EEPROM.read(EEPROM_START) == GAME_ID) firstGame = false;
        //      cursorY = STATE_MENU_CONTINUE + firstGame;
        //      //textspeed = TEXT_ROLL_DELAY;
        //    }
    }
}

void stateMenuMain() {
    byte locationMenu = firstGame ? 2 : 8;
    arduboy.fillScreen(0);
    fillWithSentence(0);
    if(!firstGame) fillWithWord(1, 1);
    if(arduboy.audio.enabled())
        fillWithWord(27, 6);
    else
        fillWithWord(26, 5);
    drawTextBox(40, locationMenu, WHITE);
    sprites.drawSelfMasked(32, locationMenu + (cursorY - 2) * 12, font, 44);
    sprites.drawSelfMasked(90, locationMenu + (cursorY - 2) * 12, font, 45);
}

void stateMenuContinue() {
    loadGame();
    ATM.stop();
    // Force region music to start immediately after loading.
    songPlaying = 0;
    changeSong(player.currentRegion);
    gameState = STATE_GAME_PLAYING;
}

void stateMenuNew() {
    setPlayer();
    gameState = STATE_GAME_NEW;
    ATM.play(nameSong);
}

void toggleSound() {
    if(!arduboy.audio.enabled()) {
        arduboy.audio.on();
        // Restart title music immediately when sound is enabled.
        ATM.play(titleSong);
    } else {
        ATM.stop();
        arduboy.audio.off();
    }
    arduboy.audio.saveOnOff();
    cursorY = STATE_MENU_CONTINUE + firstGame;
}

void stateMenuSound() {
    arduboy.fillScreen(0);
    // if sound is not enabled, put it ON, otherwise put it OFF
    toggleSound();
    gameState = STATE_MENU_MAIN;
}

void stateMenuCredits() {
    arduboy.fillScreen(0);
    fillWithSentence(41);
    drawTextBox(18, 11, WHITE);
}

void stateMenuReboot() {
    arduboy.fillScreen(0);
    fillWithSentence(65);
    drawTextBox(4, 8, WHITE);
    fillWithSentence(66);
    drawTextBox(4, 34, WHITE);
    if(arduboy.pressed(DOWN_BUTTON)) {
        arduboy.exitToBootloader();
    } else if(arduboy.justPressed(A_BUTTON)) {
        gameState = STATE_MENU_MAIN;
        cursorY = STATE_MENU_CONTINUE + firstGame;
    }
}

#endif
