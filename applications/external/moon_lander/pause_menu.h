#pragma once

#include <gui/canvas.h>
#include <input/input.h>
#include <stdbool.h>
#include <stdint.h>

/* The pause menu that a Back tap opens mid-flight. It only tracks the
 * focused row and draws itself; game.c decides what each choice does. */

typedef enum {
    PauseActionNone = 0,
    PauseActionResume,
    PauseActionZeroTilt,
    PauseActionQuit,
} PauseAction;

typedef struct {
    bool    open;
    bool    zero_tilt;  // has the Zero tilt row (the tilt modes)
    uint8_t sel;        // focused row
} PauseMenu;

/* Opens the menu with Resume focused. */
void pause_menu_open(PauseMenu* p, bool zero_tilt);

/* Up/Down move the focus; OK returns the focused row's action. Back is
 * left to the caller, which times the hold-to-exit. */
PauseAction pause_menu_input(PauseMenu* p, const InputEvent* ev);

/* A box centered on the screen, one row per item, the focused row inverted. */
void pause_menu_draw(Canvas* canvas, const PauseMenu* p);
