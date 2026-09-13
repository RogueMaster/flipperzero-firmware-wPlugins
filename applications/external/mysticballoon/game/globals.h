#ifndef GLOBALS_H
#define GLOBALS_H

/*-----------------------------*
   To turn on hard mode
   uncomment the below define.

   Hard mode makes it so you
   start each level without
   recovering balloons.
  ----------------------------*/
//#define HARD_MODE

#include <stdint.h>
#include <stdlib.h>

#include "mystic_balloon.h"
#include "render.h"
#include "vec2.h"
#include "bitmaps.h"

//define menu states (on main menu)
#define STATE_MENU_INTRO   0
#define STATE_MENU_MAIN    1
#define STATE_MENU_HELP    2
#define STATE_MENU_PLAY    3
#define STATE_MENU_INFO    4
#define STATE_MENU_SOUNDFX 5

//define game states (on main menu)
#define STATE_GAME_NEXT_LEVEL  6
#define STATE_GAME_PLAYING     7
#define STATE_GAME_PAUSE       8
#define STATE_GAME_OVER        9
#define STATE_GAME_PLAYCONTNEW 10 // 11

#define FACING_RIGHT 0
#define FACING_LEFT  1

#define LEVEL_TO_START_WITH 1
#define TOTAL_LEVELS        39
#define TOTAL_COINS         TOTAL_LEVELS * 6

#define MAX_PER_TYPE 6 // total instances per enemy type

#define LEVEL_WIDTH        384 // 24 * 16
#define LEVEL_HEIGHT       384 // 24 * 16
#define LEVEL_CELLSIZE     16
#define LEVEL_WIDTH_CELLS  24
#define LEVEL_HEIGHT_CELLS 24
#define LEVEL_CELL_BYTES   (LEVEL_WIDTH_CELLS * LEVEL_HEIGHT_CELLS) >> 3
#define LEVEL_ARRAY_SIZE   576

#define PLAYER_JUMP_TIME 11

#define bitRead(value, bit) (((value) >> (bit)) & 0x01u)
#define randomOf(range)     ((int)(rand() % (range)))
#define minOf(a, b)         ((a) < (b) ? (a) : (b))
#define maxOf(a, b)         ((a) > (b) ? (a) : (b))

// This is a replacement for struct Rect in the Arduboy2 library.
// It defines height as an int instead of a uint8_t to allow a higher rectangle.
struct HighRect {
public:
    int x;
    int y;
    uint16_t width;
    int height;
};

MyblSave save;
bool saveDirty = false;
bool soundEnabled = true;
bool exitRequested = false;
uint32_t frameCounter = 0;
uint8_t buttonsHeld = 0;
uint8_t buttonsPressed = 0;

uint8_t gameState = STATE_MENU_INTRO; // start the game with the TEAM a.r.g. logo
uint8_t menuSelection = STATE_MENU_PLAY; // PLAY menu item is pre-selected
uint8_t globalCounter = 0;
uint8_t level;
unsigned long scorePlayer;
uint8_t coinsCollected = 0;
uint8_t totalCoins = 0;
uint8_t balloonsLeft;

bool nextLevelIsVisible;
bool scoreIsVisible;
bool canPressButton;
bool pressKeyIsVisible;

uint8_t walkerFrame = 0;
uint8_t fanFrame = 0;
uint8_t coinFrame = 0;
uint8_t coinsActive = 0;
vec2 levelExit = vec2(0, 0);
vec2 startPos;
uint8_t mapTimer = 10;

bool pressed(uint8_t mask) {
    return (buttonsHeld & mask) != 0;
}

bool justPressed(uint8_t mask) {
    return (buttonsPressed & mask) != 0;
}

bool everyXFrames(uint8_t frames) {
    return (frameCounter % frames) == 0;
}

void playTone(uint16_t frequency, uint16_t duration_ms) {
    if(!soundEnabled) return;

    platform_tone(frequency, duration_ms);
}

void saveByte(uint8_t& field, uint8_t value) {
    if(field == value) return;

    field = value;
    saveDirty = true;
}

void saveLong(uint32_t& field, uint32_t value) {
    if(field == value) return;

    field = value;
    saveDirty = true;
}

// This is a replacement for the collide() function in the Arduboy2 library.
// It uses struct HighRect instead of the struct Rect in the library.
bool collide(HighRect rect1, HighRect rect2) {
    return !(
        rect2.x >= rect1.x + rect1.width || rect2.x + rect2.width <= rect1.x ||
        rect2.y >= rect1.y + rect1.height || rect2.y + rect2.height <= rect1.y);
}

#endif
