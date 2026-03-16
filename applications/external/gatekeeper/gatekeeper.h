#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb_hid.h>
#include <furi_hal_usb.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>
#include <storage/storage.h>
#include <input/input.h>

// ─── Constants ───────────────────────────────────────────────────────────────

#define MAX_ENTRIES      15
#define MAX_NAME_LEN     32
#define MAX_TEXT_LEN     128
#define MAX_PASSWORD_LEN 4

#define GATEKEEPER_STORAGE_PATH APP_DATA_PATH("gatekeeper.dat")

#define GK_BTN_UP    0x01
#define GK_BTN_DOWN  0x02
#define GK_BTN_LEFT  0x03
#define GK_BTN_RIGHT 0x04

// Custom events sent via view_dispatcher_send_custom_event()
#define GK_EVENT_START_DEPLOY 0x01
#define GK_EVENT_REPAINT      0x02

// ViewDispatcher view IDs
typedef enum {
    VIEW_ID_COMBO      = 0,
    VIEW_ID_MENU       = 1,
    VIEW_ID_DEPLOY     = 2,
    VIEW_ID_NAME_INPUT = 3,
    VIEW_ID_TEXT_INPUT = 4,
    VIEW_ID_DELETE_CONFIRM = 5,
    VIEW_ID_ICON_PICK  = 6,
} GkViewId;

// ─── State ───────────────────────────────────────────────────────────────────

typedef enum {
    STATE_SET_PASSWORD,
    STATE_SET_PASSWORD_CONFIRM,
    STATE_UNLOCK,
    STATE_MAIN_MENU,
    STATE_DEPLOY,
    STATE_DELETE_CONFIRM,
    STATE_ICON_PICK,
} GkState;

typedef struct {
    char    name[MAX_NAME_LEN];
    char    text[MAX_TEXT_LEN];
    uint8_t icon; // 0..GK_ICON_COUNT-1
} GkEntry;

// ─── Forward declarations for TextInput callbacks ────────────────────────────
// Needed because GkApp references them as function pointers and the
// definitions appear later in gatekeeper.c

static void on_name_done(void* ctx);
static void on_text_done(void* ctx);
static void icon_pick_draw_cb(Canvas* canvas, void* model);
static bool icon_pick_input_cb(InputEvent* event, void* context);

// ─── App struct ──────────────────────────────────────────────────────────────

typedef struct {
    Gui*            gui;
    ViewDispatcher* view_dispatcher;

    View* view_combo;
    View* view_menu;
    View* view_deploy;
    View* view_delete_confirm;
    View* view_icon_pick;

    TextInput* name_input;
    TextInput* text_input;

    GkState state;

    // Combo lock
    uint8_t password[MAX_PASSWORD_LEN];
    uint8_t password_len;
    uint8_t password_temp[MAX_PASSWORD_LEN];
    uint8_t input_buf[MAX_PASSWORD_LEN];
    uint8_t input_len;
    bool    unlock_failed;

    // Entries
    GkEntry entries[MAX_ENTRIES];
    uint8_t entry_count;

    // Menu navigation  (-1 = "+ Add", 0..N-1 = entry row)
    int menu_cursor;
    int menu_scroll;

    // TextInput buffers
    char edit_name[MAX_NAME_LEN];
    char edit_text[MAX_TEXT_LEN];

    // Deploy
    int          selected_entry;
    float        deploy_progress;
    bool         deploy_not_connected;
    FuriThread*  deploy_thread;

    // Delete confirmation
    int          delete_entry_index;
    bool         delete_failed;

    // Icon picker
    uint8_t      icon_cursor;
} GkApp;

int32_t gatekeeper_app(void* p);
