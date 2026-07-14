#ifndef ARDULIB_USE_FX
#define ARDULIB_USE_FX
#endif

#ifndef ARDULIB_USE_TONES
#define ARDULIB_USE_TONES
#endif

#include "lib/runtime.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wredundant-decls"

#include "lib/Arduboy2.h"
#include "lib/ArduboyFX.h"
#include "src/utils/Arduboy2Ext.h"
#include "src/ArduboyTonesFX.h"
#include "src/entities/Entities.h"
#include "src/utils/Enums.h"

void splashScreen_Init();
void splashScreen();
void title_Init();
void title();
void setSound(SoundIndex index);
void isEnemyVisible(
    Prince& prince,
    bool swapEnemies,
    bool& isVisible,
    bool& sameLevelAsPrince,
    bool justEnteredRoom);

bool testScroll(GamePlay& gamePlay, Prince& prince, Level& level);
void pushSequence();
void processJump(uint24_t pos);
void processRunJump(Prince& prince, Level& level, bool testEnemy);
void processStandingJump(Prince& prince, Level& level);
void initFlash(Prince& prince, Level& level, FlashType flashType);
void initFlash(Enemy& enemy, Level& level, FlashType flashType);
uint8_t activateSpikes(Prince& prince, Level& level);
void activateSpikes_Helper(Item& spikes);
void pushJumpUp_Drop(Prince& prince);
bool leaveLevel(Prince& prince, Level& level);
void pushDead(Prince& entity, Level& level, GamePlay& gamePlay, bool clear, DeathType deathType);
void pushDead(Enemy& entity, bool clear);
void showSign(Prince& prince, Level& level);
void playGrab();
void fixPosition();
uint8_t getImageIndexFromStance(uint16_t stance);
void getStance_Offsets(Direction direction, Point& offset, int16_t stance);
void processRunningTurn();
void saveCookie(bool enableLEDs);
void bindRuntimeStacks();
void restoreRuntimeAfterLoad();
void handleBlades();
void handleBlade_Single(int8_t tileXIdx, int8_t tileYIdx, uint8_t princeLX, uint8_t princeRX);

void game_Init();
void game_StartLevel();
uint16_t getMenuData(uint24_t table);
void game();
void moveBackwardsWithSword(Prince& prince);
void moveBackwardsWithSword(BaseEntity entity, BaseStack stack);

void render(bool sameLevelAsPrince);
void renderMenu();
void renderNumber(uint8_t x, uint8_t y, uint8_t number);
void renderNumber_Small(uint8_t x, uint8_t y, uint8_t number);
void renderNumber_Upright(uint8_t x, uint8_t y, uint8_t number);
void renderTorches(uint8_t x1, uint8_t x2, uint8_t y);

#include "game/PrinceOfArabia.cpp"
#include "game/PrinceOfArabia_Game.cpp"
#include "game/PrinceOfArabia_Render.cpp"
#include "game/PrinceOfArabia_SplashScreen.cpp"
#include "game/PrinceOfArabia_Title.cpp"
#include "game/PrinceOfArabia_Utils.cpp"

namespace {

constexpr uint32_t ExitHoldMs = 500;

bool canExitFromCurrentState(GameState gameState) {
    switch(gameState) {
    case GameState::SplashScreen_Init:
    case GameState::SplashScreen:
    case GameState::Title_Init:
    case GameState::Title:
        return true;

    default:
        return false;
    }
}

bool handleExitRequest() {
    static bool exitButtonHeld = false;
    static uint32_t exitPressStartedAt = 0;
    static GameState lastGameState = GameState::SplashScreen_Init;

    const GameState gameState = gamePlay.gameState;

    if(gameState != lastGameState) {
        lastGameState = gameState;
        exitButtonHeld = false;
        exitPressStartedAt = 0;
    }

    if(!canExitFromCurrentState(gameState) || !arduboy.pressed(B_BUTTON)) {
        exitButtonHeld = false;
        exitPressStartedAt = 0;
        return false;
    }

    const uint32_t now = millis();

    if(!exitButtonHeld) {
        exitButtonHeld = true;
        exitPressStartedAt = now;
        return false;
    }

    if((uint32_t)(now - exitPressStartedAt) < ExitHoldMs) {
        return false;
    }

    exitButtonHeld = false;
    exitPressStartedAt = 0;
    arduboy.exitToBootloader();

    return true;
}

} // namespace

void setup() {
    arduboy.setFrameRate(Constants::FrameRate);

    FX::begin(FX_DATA_PAGE, FX_SAVE_PAGE);
    const bool hasSave = FX::loadGameState((uint8_t*)&cookie, sizeof(cookie));

    prince.setStack(&princeStack);

#ifndef SAVE_MEMORY_ENEMY
    enemy.setStack(&enemyStack);
#endif

    if(hasSave) {
        restoreRuntimeAfterLoad();
    }

#ifdef SAVE_MEMORY_OTHER
    gamePlay.gameState = GameState::Game_Init;
#else
#ifdef SAVE_MEMORY_PPOT
    gamePlay.gameState = GameState::Title_Init;
#else
    gamePlay.gameState = GameState::SplashScreen_Init;
#endif
#endif
}

void loop() {
    if(!arduboy.nextFrame()) return;
    arduboy.pollButtons();
    bindRuntimeStacks();
    if(handleExitRequest()) return;

#ifndef SAVE_MEMORY_SOUND
    sound.fillBufferFromFX();
#endif

    switch(gamePlay.gameState) {
#ifndef SAVE_MEMORY_PPOT
    case GameState::SplashScreen_Init:

        splashScreen_Init();
        titleScreenVars.counter = 0;
        [[fallthrough]];

    case GameState::SplashScreen:

        splashScreen();
        break;
#endif

#ifndef SAVE_MEMORY_OTHER
    case GameState::Title_Init:

#ifndef SAVE_MEMORY_SOUND
        setSound(SoundIndex::Theme);
#endif

#ifndef SAVE_MEMORY_OTHER
        fadeEffect.complete();
#endif

        gamePlay.gameState = GameState::Title;

        title_Init();
        [[fallthrough]];

    case GameState::Title:

        title();
        break;

#endif

    case GameState::Game_Init:

#ifndef SAVE_MEMORY_SOUND
        sound.noTone();
#endif

        game_Init();
        [[fallthrough]];

    case GameState::Game_StartLevel:
        game_StartLevel();
        game();
        break;

    case GameState::Game:
#ifndef SAVE_MEMORY_OTHER
    case GameState::Menu:
#endif

        game();
        break;

    default:
        break;
    }

    {
        bool invert = false;

        switch(prince.getStance()) {
        case Stance::Pickup_Sword_3:
        case Stance::Pickup_Sword_5:
        case Stance::Drink_Tonic_Small_12:
        case Stance::Drink_Tonic_Small_14:
        case Stance::Drink_Tonic_Large_12:
        case Stance::Drink_Tonic_Large_14:
        case Stance::Drink_Tonic_Poison_12:
        case Stance::Drink_Tonic_Poison_14:
        case Stance::Drink_Tonic_Float_12:
        case Stance::Drink_Tonic_Float_14:
            invert = true;
            break;

        default:
            break;
        }

        Flash& flash = level.getFlash();

#ifndef SAVE_MEMORY_ENEMY
        if(flash.frame == 1 && flash.type == FlashType::MirrorLevel12) {
            enemy.setStatus(Status::Dormant);
        }
#endif

        if((flash.frame == 2 || flash.frame == 4) &&
           (flash.type == FlashType::SwordFight || flash.type == FlashType::MirrorLevel12)) {
            invert = true;
        }

        FX::enableOLED();
        arduboy.invert(invert);
    }

#ifndef SAVE_MEMORY_OTHER
    if(!fadeEffect.isComplete()) {
        fadeEffect.draw(arduboy);
        fadeEffect.update();
    }
#endif

    FX::display(CLEAR_BUFFER);
}

#pragma GCC diagnostic pop
