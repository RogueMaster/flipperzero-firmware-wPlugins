#pragma once

#include <gui/gui.h>
#include <input/input.h>
#include "moon_lander.h"

/* Menu rows top-to-bottom. Title is at the top — Up from Thrust reaches it.
 * The title row has two focusable sub-elements (see MenuTitleSel). */
typedef enum {
    MenuRowTitle = 0,
    MenuRowThrust,
    MenuRowFuel,
    MenuRowButtons,
    MenuRowCount,
} MenuRow;

/* Sub-selection within the title row: lander sprite (score), title text
 * (about), wrench (settings). Left/Right cycles left-to-right. */
typedef enum {
    MenuTitleSelScore = 0,
    MenuTitleSelAbout,
    MenuTitleSelSettings,
    MenuTitleSelCount,
} MenuTitleSel;

typedef enum {
    MenuBtnStart = 0,
    MenuBtnTutorial,
    MenuBtnCount,
} MenuBtn;

typedef enum {
    MenuActionNone = 0,
    MenuActionStart,
    MenuActionTutorial,
    MenuActionScore,
    MenuActionInfo,
    MenuActionSettings,
    MenuActionExit,
} MenuAction;

/* MenuState carries everything the menu owns, including user preferences
 * (modes + sound/vibration toggles) that persist across screen transitions. */
typedef struct {
    MenuRow      row;
    MenuBtn      btn;
    MenuTitleSel title_sel;
    ThrustMode   thrust_mode;
    FuelMode     fuel_mode;
    Difficulty   difficulty;
    SoundLevel     sound_level;
    VibrationLevel vibration_level;
    uint8_t      custom_vx;      // DifficultyCustom limits: 1..CUSTOM_VX_MAX
    uint8_t      custom_vy;      // 1..CUSTOM_VY_MAX
    uint8_t      custom_angle;   // degrees, 1..CUSTOM_ANGLE_MAX
    bool         tv_mode;        // squish the playfield for the Video Game Module
    ThrustKey    thrust_key;     // UP or OK fires the engine
    uint16_t     seed;           // SEED_MIN..SEED_MAX; picks the set of levels
} MenuState;

void menu_init(MenuState* m);
MenuAction menu_input(MenuState* m, const InputEvent* ev);
void menu_draw(Canvas* canvas, const MenuState* m);
/* A "< label >" row across the screen; draw_selector_row_w sets its x and
 * width (Settings leaves room for its scrollbar). */
void draw_selector_row(Canvas* canvas, int y, const char* label, bool focused);
void draw_selector_row_w(Canvas* canvas, int x, int y, int w, const char* label, bool focused);
/* The thrust row's one-line description, naming the chosen thrust key. */
void thrust_mode_desc(ThrustMode mode, ThrustKey key, char* buf, size_t size);
