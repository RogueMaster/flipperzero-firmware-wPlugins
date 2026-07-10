#pragma once

#include "include/scenes/gurpil_scene.h"
#include "include/views/gurpil_game_view.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>

#include <stdint.h>

/*
 * The furi isolation boundary for this app: everything below `app/`, `views/` and `scenes/`
 * stays plain C so it host-tests with gcc; this header (and the app/views/scenes files that
 * include it) are the only place furi/gui types appear.
 *
 * Classic Flipper menu shape, mirroring the firmware's own scene-driven apps (see e.g.
 * applications/examples/example_number_input and applications/main/ibutton): a Submenu-backed
 * Menu scene (Play / How to play / Credits) sits at the base of the SceneManager's scene stack;
 * Play pushes the Game scene (wrapping the existing game loop via GurpilGameView), How to play
 * and Credits each push a Widget text scene. Every leaf scene Backs out to Menu for free via
 * SceneManager's own default back-event handling (see include/scenes/gurpil_scene.h); Back at
 * Menu — the base of the stack — is what actually exits the app.
 */

// View ids registered with the ViewDispatcher — one per screen the app can show.
typedef enum {
    GurpilViewIdMenu,
    GurpilViewIdGame,
    GurpilViewIdHowToPlay,
    GurpilViewIdCredits,
} GurpilViewId;

typedef struct {
    Gui *gui;
    SceneManager *scene_manager;
    ViewDispatcher *view_dispatcher;

    Submenu *menu;
    Widget *how_to_play;
    Widget *credits;
    GurpilGameView *game_view;

    FuriTimer *game_timer; // owned by the Game scene: allocated on_enter, stopped+freed on_exit,
                           // so the endless countdown only ever runs while Play is on screen.

    int32_t best; // persisted best distance; loaded once at startup, saved on every new best.
} GurpilApp;

int32_t gurpil_app_run(void);
