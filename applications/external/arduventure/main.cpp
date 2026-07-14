#ifndef ARDULIB_USE_ATM
#define ARDULIB_USE_ATM
#endif

#include "lib/ATMlib.h"
#include "lib/runtime.h"
#include "lib/Arduboy2.h"
#include "lib/Arduino.h"

// Preserve original Arduventure color semantics from the pre-shared-lib port.
#undef BLACK
#undef WHITE
#define BLACK 0
#define WHITE 1

#define TARGET_FRAMERATE 60

#define GAME_ID 46

#include "game/globals.h"
#include "game/songs.h"
#include "game/menu.h"
#include "game/game.h"
#include "game/inputs.h"
#include "game/text.h"
#include "game/inventory.h"
#include "game/items.h"
#include "game/player.h"
#include "game/enemies.h"
#include "game/battles.h"

typedef void (*FunctionPointer)();
const FunctionPointer mainGameLoop[] = {
    stateMenuIntro,     stateMenuMain,         stateMenuContinue,     stateMenuNew,
    stateMenuSound,     stateMenuCredits,      stateGamePlaying,      stateGameInventory,
    stateGameEquip,     stateGameStats,        stateGameMap,          stateGameOver,
    showSubMenuStuff,   showSubMenuStuff,      showSubMenuStuff,      showSubMenuStuff,
    walkingThroughDoor, stateGameBattle,       stateGameBoss,         stateGameIntro,
    stateGameNew,       stateGameSaveSoundEnd, stateGameSaveSoundEnd, stateGameSaveSoundEnd,
    stateGameObjects,   stateGameShop,         stateGameInn,          battleGiveRewards,
    stateMenuReboot,
};

void setup() {
    arduboy.boot();
    arduboy.audio.begin();
    EEPROM.begin();

    if(arduboy.audio.enabled()) {
        ATM.play(titleSong);
    }

    arduboy.setFrameRate(TARGET_FRAMERATE);
}

// Helper function to check if current state is a menu state
bool isMenuState(int state) {
    return (state >= STATE_MENU_INTRO && state <= STATE_MENU_CREDITS) || (state == STATE_REBOOT);
}

void loop() {
    if(!arduboy.nextFrame()) return;

    arduboy.pollButtons();

    // Handle display inversion for menu vs game states
    if(isMenuState(gameState)) {
        arduboy.invert(true);
    } else {
        arduboy.invert(false);
    }

    drawTiles();
    updateEyes();

    mainGameLoop[gameState]();

    checkInputs();
    if(question) drawQuestion();
    if(yesNo) drawYesNo();
    if(flashBlack)
        flashScreen(BLACK);
    else if(flashWhite)
        flashScreen(WHITE);

    arduboy.display();
}
