/**
 * @file flipper_tv_remote.h
 * @brief Flipper TV Remote - record TV remote buttons and replay them.
 *
 * This app uses the Flipper Zero IR transceiver to capture IR signals from a
 * physical TV remote and then maps them to the Flipper's buttons so the
 * Flipper can act as a TV remote.
 */

#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <input/input.h>
#include <infrared_worker.h>
#include <infrared.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include <lib/toolbox/saved_struct.h>

#define TV_REMOTE_APP_TAG         "TvRemote"
#define TV_REMOTE_FILE_HEADER     "IR signals file"
#define TV_REMOTE_FILE_VERSION    1
#define TV_REMOTE_FILE_DIR        EXT_PATH("infrared")
#define TV_REMOTE_FILE_PREFIX     "tv_remote_"
#define TV_REMOTE_NAME_MAX        24
#define TV_REMOTE_SOURCE_NAME_MAX 64
#define TV_REMOTE_SETTINGS_PATH   APP_DATA_PATH("tv_remote_settings.dat")

/** Number of buttons the app can learn and replay. */
#define TV_BUTTON_COUNT 14

/** Indices for each learnable TV button. */
typedef enum {
    TvButtonPower = 0,
    TvButtonMute,
    TvButtonVolUp,
    TvButtonVolDn,
    TvButtonChUp,
    TvButtonChDn,
    TvButtonUp,
    TvButtonDown,
    TvButtonLeft,
    TvButtonRight,
    TvButtonOk,
    TvButtonBack,
    TvButtonHome,
    TvButtonPlayPause,
} TvButton;

/** Screen orientation setting. */
typedef enum {
    TvRemoteOrientationVertical = 0, /**< Portrait – Flipper held rotated like a remote. */
    TvRemoteOrientationHorizontal, /**< Landscape – Flipper held normally. */
    TvRemoteOrientationCount,
} TvRemoteOrientation;

/** View identifiers used with the ViewDispatcher. */
typedef enum {
    TvRemoteViewMainMenu = 0,
    TvRemoteViewLearnMenu,
    TvRemoteViewLearn,
    TvRemoteViewRemote,
    TvRemoteViewSelectRemote,
    TvRemoteViewTextInput,
    TvRemoteViewButtonMap,
    TvRemoteViewSettings,
    TvRemoteViewAbout,
    TvRemoteViewImportFileSelect,
    TvRemoteViewImportButtonSelect,
    TvRemoteViewImportSignalSelect,
} TvRemoteViewId;

/** Main menu item indices. */
typedef enum {
    TvRemoteMenuLearn = 0,
    TvRemoteMenuUse,
    TvRemoteMenuDelete,
    TvRemoteMenuButtonMap,
    TvRemoteMenuSettings,
    TvRemoteMenuAbout,
    TvRemoteMenuImport,
} TvRemoteMenuItem;

/** What action to perform when a remote is selected in the picker. */
typedef enum {
    TvRemoteSelectModeUse = 0,
    TvRemoteSelectModeOverwrite,
    TvRemoteSelectModeDelete,
} TvRemoteSelectMode;

/** Custom events sent via ViewDispatcher from ISR/worker callbacks. */
typedef enum {
    TvRemoteCustomEventSignalReceived = 0,
    TvRemoteCustomEventBackTimeout,
} TvRemoteCustomEvent;

/** Opaque app context type. */
typedef struct TvRemoteApp TvRemoteApp;

/** Temporary state for importing a saved IR file. */
typedef struct {
    char source_path[256]; /**< Full path to the selected .ir file. */
    char** source_names; /**< Signal names found in the source file. */
    size_t source_count;
    int16_t map[TV_BUTTON_COUNT]; /**< Source index mapped to each TV button, or -1. */
    char new_remote_name[TV_REMOTE_NAME_MAX + 1];
    uint8_t selected_button; /**< TV button currently being mapped. */
} TvRemoteImportState;

/** Stored IR signal: either a decoded (parsed) message or raw timings. */
typedef struct {
    bool is_raw;
    InfraredMessage message; /**< Valid when is_raw == false. */
    uint32_t* timings; /**< Heap-allocated raw timing array; NULL when unused. */
    size_t timings_size;
    uint32_t frequency;
    float duty_cycle;
} TvRemoteIrSignal;

/** Per-button state: name, learned flag, and stored IR signal. */
typedef struct {
    const char* name; /**< Human-readable button name (stored in .ir file). */
    TvRemoteIrSignal signal; /**< Stored IR signal data. */
    bool learned; /**< True when a signal has been successfully recorded. */
} TvRemoteButton;

/** Application state. */
struct TvRemoteApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    /* Views */
    Submenu* main_menu;
    Submenu* learn_menu;
    Submenu* select_submenu;
    TextInput* text_input;
    View* learn_view;
    View* remote_view;
    View* button_map_view;
    View* settings_view;
    View* about_view;
    Submenu* import_button_submenu;
    Submenu* import_signal_submenu;

    /* Settings */
    TvRemoteOrientation orientation;
    bool button_swap; /**< True: short press = vol/ch, hold = directional (default is reversed). */
    bool lr_hold_repeat; /**< True: hold Left/Right sends the short-press (directional) button. */
    bool ud_hold_repeat; /**< True: hold Up/Down sends the short-press (directional) button. */
    bool hold_continuous; /**< True: hold action uses continuous TX; False: single burst. */

    /* Button storage */
    TvRemoteButton buttons[TV_BUTTON_COUNT];

    /* Learning state */
    uint8_t learn_index; /**< Index of the button currently being learned. */
    bool learn_signal_received; /**< Set when IR callback fires during learn. */

    /* Remote naming & selection */
    char current_remote_name[TV_REMOTE_NAME_MAX + 1]; /**< Name of the active remote. */
    char text_input_buf[TV_REMOTE_NAME_MAX + 1]; /**< Working buffer for TextInput. */
    TvRemoteSelectMode select_mode;
    char** remote_names; /**< Heap array of scanned remote name strings. */
    size_t remote_count;

    /* IR worker */
    InfraredWorker* worker;
    bool worker_active;

    /* TX state (for remote view button hold) */
    uint8_t remote_selected; /**< Button index highlighted in the remote view. */
    bool tx_active; /**< True while sending IR in remote view. */
    uint8_t remote_pressed_keys; /**< Bitmask of physically held d-pad/ok keys. */
    bool remote_held_long; /**< True once InputTypeLong fires (alt action started). */

    /* Back-button double-tap detection */
    FuriTimer* back_timer; /**< One-shot timer for deferred single-tap Back. */
    bool back_pending; /**< True while waiting for possible double-tap. */

    /* Import from saved IR file */
    TvRemoteImportState import;
};

/** Canonical button names (must match .ir file entries). */
extern const char* const tv_remote_button_names[TV_BUTTON_COUNT];

/* ---- Navigation callbacks ---- */
uint32_t tv_remote_back_to_import_callback(void* context);

/* ---- App lifecycle ---- */
TvRemoteApp* tv_remote_app_alloc(void);
void tv_remote_app_free(TvRemoteApp* app);

/* ---- File I/O ---- */
void tv_remote_app_clear_buttons(TvRemoteApp* app);
void tv_remote_build_path(FuriString* out, const char* name);
bool tv_remote_app_save_named(TvRemoteApp* app, const char* name);
bool tv_remote_app_load_named(TvRemoteApp* app, const char* name);
void tv_remote_scan_remotes(TvRemoteApp* app);
void tv_remote_free_remote_names(TvRemoteApp* app);
bool tv_remote_delete_remote(TvRemoteApp* app, const char* name);
bool tv_remote_list_ir_file_signals(const char* path, char*** names_out, size_t* count_out);
void tv_remote_free_source_names(TvRemoteImportState* import);
bool tv_remote_import_load_source_signal(TvRemoteApp* app, size_t source_index, TvButton target);
bool tv_remote_import_save_mapped_remote(TvRemoteApp* app, const char* name);

/* ---- Import helpers ---- */
void tv_remote_import_reset(TvRemoteImportState* import);

/* ---- IR file picker entry point ---- */
bool tv_remote_import_run_file_browser(TvRemoteApp* app);
void tv_remote_import_build_button_submenu(TvRemoteApp* app);
/* ---- Entry point ---- */
int32_t flipper_tv_remote_app(void* p);
