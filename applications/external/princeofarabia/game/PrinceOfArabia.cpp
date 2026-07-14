#include "src/utils/Arduboy2Ext.h"
#include "src/ArduboyTonesFX.h"
#include <lib/ArduboyFX.h>

#include "src/utils/Constants.h"
#include "src/utils/Enums.h"
#include "src/utils/Stack.h"
#include "src/utils/FadeEffects.h"
#include "src/entities/Entities.h"
#include "src/fonts/Font3x5.h"

#ifdef SAVE_MEMORY_USB
ARDUBOY_NO_USB
#endif

#ifndef SAVE_MEMORY_SOUND

uint16_t buffer[16];

ArduboyTonesFX sound(arduboy.audio.enabled, buffer);

#endif

#if(defined(DEBUG) && defined(DEBUG_ONSCREEN_DETAILS)) or \
    (defined(DEBUG) && defined(DEBUG_ONSCREEN_DETAILS_MIN))
Font3x5 font3x5 = Font3x5();
#endif

#ifdef DEBUG_LEVELS
uint8_t startLevel = STARTING_LEVEL;
#endif

Cookie cookie;
Stack<int16_t, Constants::StackSize> princeStack;
Prince& prince = cookie.prince;
Stack<int16_t, Constants::StackSize> enemyStack;
Mouse mouse;
bool menuBRequiresRelease = false;

#ifndef SAVE_MEMORY_ENEMY
Enemy& enemy = cookie.enemy;
#endif

Level& level = cookie.level;
GamePlay& gamePlay = cookie.gamePlay;
TitleScreenVars titleScreenVars;
MenuItem menu;

#ifndef SAVE_MEMORY_OTHER
FadeEffects fadeEffect;
#endif
