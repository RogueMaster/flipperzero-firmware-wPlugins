/*
  Mystic Balloon: http://www.team-arg.org/mybl-manual.html

  Arduboy version 1.7.2:  http://www.team-arg.org/mybl-downloads.html

  MADE by TEAM a.r.g. : http://www.team-arg.org/more-about.html

  2016-2018 - GAVENO - CastPixel - JO3RI - Martian220

  Game License: MIT : https://opensource.org/licenses/MIT

*/

#include "globals.h"
#include "menu.h"
#include "game.h"
#include "inputs.h"
#include "player.h"
#include "enemies.h"
#include "elements.h"
#include "levels.h"

typedef void (*FunctionPointer)();

static const FunctionPointer mainGameLoop[] = {
    stateMenuIntro,
    stateMenuMain,
    stateMenuHelp,
    stateMenuPlaySelect,
    stateMenuInfo,
    stateMenuSoundfx,
    stateGameNextLevel,
    stateGamePlaying,
    stateGamePause,
    stateGameOver,
    stateMenuPlayContinue,
    stateMenuPlayNew,
};

void mybl_start(bool sound, const MyblSave* stored) {
    soundEnabled = sound;
    save = *stored;
}

void mybl_frame(uint8_t held, uint8_t pressEdges) {
    frameCounter++;
    buttonsHeld = held;
    buttonsPressed = pressEdges;

    if(gameState < STATE_GAME_NEXT_LEVEL && everyXFrames(10))
        sparkleFrames = (sparkleFrames + 1) % 5;

    gfx_clear(GfxDark);
    mainGameLoop[gameState]();
}

bool mybl_exit_requested(void) {
    return exitRequested;
}

bool mybl_take_save(MyblSave* stored) {
    if(!saveDirty) return false;

    *stored = save;
    saveDirty = false;
    return true;
}
