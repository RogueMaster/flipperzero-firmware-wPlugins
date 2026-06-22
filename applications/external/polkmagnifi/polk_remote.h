#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <infrared.h>
#include <infrared_transmit.h>

// ── Row types ─────────────────────────────────────────────────────────────────

typedef enum {
    RowSingle,   // POWER, NIGHT  — OK fires cmd_ok
    RowLR,       // VOICE, BASS   — Left fires cmd_left, Right fires cmd_right
    RowLOR,      // VOL, PLAYBACK — Left/OK/Right each fire a command
    RowSubmenu,  // INPUT, AUDIO MODE — OK opens a submenu
} RowType;

typedef struct {
    const char*           label;
    RowType               type;
    const InfraredMessage* cmd_left;
    const InfraredMessage* cmd_ok;
    const InfraredMessage* cmd_right;
    bool                  ok_is_icon;
    const char* const*    sub_labels;
    const InfraredMessage* const* sub_cmds;
    uint8_t               sub_count;
} RemoteRow;

#define ROWS_COUNT 7

extern const RemoteRow ROWS[ROWS_COUNT];

// ── View model ────────────────────────────────────────────────────────────────

typedef struct {
    uint8_t selected_row;
    uint8_t scroll_offset;  // index of first visible row; zero-init = 0
    bool    is_pressed;     // true while an action key is held
    uint8_t pressed_row;
    uint8_t pressed_cell;   // 0=left, 1=center/ok, 2=right
} MainViewModel;

typedef struct {
    uint8_t            selected;
    uint8_t            count;
    const char*        title;
    const char* const* labels;
    bool               is_pressed;  // true while OK is held on the selected item
} SubMenuViewModel;

// ── Application state ─────────────────────────────────────────────────────────

typedef struct {
    SceneManager*    scene_manager;
    ViewDispatcher*  view_dispatcher;
    Gui*             gui;
    View*            main_view;
    View*            submenu_view;
    uint8_t          selected_row;
    uint8_t          active_row;
} PolkRemoteApp;

// ── IR send helper ────────────────────────────────────────────────────────────

void polk_send(const InfraredMessage* msg);

// ── Scene draw / input callbacks ──────────────────────────────────────────────

void scene_main_draw_cb(Canvas* canvas, void* model);
bool scene_main_input_cb(InputEvent* event, void* ctx);

void scene_submenu_draw_cb(Canvas* canvas, void* model);
bool scene_submenu_input_cb(InputEvent* event, void* ctx);
