#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

typedef enum {
    ViewIdStatus,
    ViewIdMenu,
    ViewIdPerm,
    ViewIdInfo,
} ViewId;

typedef enum {
    UiEventEnter,
    UiEventEsc,
    UiEventDown,
    UiEventVoice,
    UiEventSpaceHoldStart,
    UiEventSpaceHoldEnd,
    UiEventOpenMenu,
    UiEventMenuSelect,
    UiEventMenuBack,
    UiEventDismiss,
    UiEventExitApp,
    UiEventPermAllow,
    UiEventPermAlways,
    UiEventPermDeny,
    UiEventPermEsc,
    UiEventBackspace,    // short-press Back: send backspace
    UiEventInterrupt,    // long-press Left: send Ctrl+C interrupt
    UiEventToggleMute,   // long-press Down: toggle sound mute
    UiEventYes,          // long-press Ok: type "yes" + enter
    UiEventOpenInfo,     // long-press Right: open info menu
    UiEventPageUp,       // transcript mode: page up
    UiEventPageDown,     // transcript mode: page down
    UiEventCtrlO,        // transcript mode: Ctrl+O
    UiEventCtrlE,        // transcript mode: Ctrl+E
    UiEventShiftTab,     // toggle plan mode (Shift+Tab)
    UiEventToggleBleMode, // BLE profile toggled between Bridge and Desktop
} UiEventType;

typedef enum {
    PoseIdle,      // default: periodic blink
    PoseListening, // eyes up, header shows REC indicator
    PoseThinking,  // eyes right, animated dots above head
    PoseHappy,     // squinted eyes + smile + sparkles, brief bounce (auto-resets)
    PoseAlert,     // wide eyes, blinking ! + horizontal shake (auto-resets)
    PoseSleeping,  // closed eyes, floating z
    PoseExcited,   // arms raised, fast bounce, cycling sparkles (auto-resets)
    PoseWorried,   // shifty eyes, sweat drop, gentle wobble (used on perm screen)
} CharacterPose;

typedef void (*UiEventCallback)(UiEventType event, const char* data, void* context);

// View model structs (stored inside each View's model allocation)
typedef struct {
    /* Sized to match PROTOCOL_MAX_FIELD_LEN / NUS_MSG_FIELD_LEN (64).
     * Desktop mode word-wraps this across up to 3 lines via wrap_text()
     * in ui.c; Bridge mode shows it on a single line and relies on the
     * host to keep messages short. */
    char text[64];    // primary status line
    char subtext[22]; // secondary info line (empty = hide; Bridge only)
    bool connected;         // serial/flipper connected
    bool claude_connected;  // claude code session active
    bool muted;             // sound mute active (shown as indicator in header)
    bool space_hold_active; // true while Up long-press is held for hold-space input
    uint8_t pose;           // CharacterPose
    uint8_t anim_frame;     // animation counter (incremented by timer)
    uint8_t transport_mode; // 0 = USB, 1 = BT (shown in header)
    uint8_t rssi_bars;      // BLE signal bars 0–4 (only used when transport_mode == 1)
} StatusModel;

#define MAX_MENU_ITEMS 256
#define MAX_MENU_ITEM_LEN 27

typedef struct {
    int index;
    int count;
    char items[MAX_MENU_ITEMS][MAX_MENU_ITEM_LEN];
} MenuModel;

typedef struct {
    char tool[22];
    /* Desktop NUS hints can be up to NUS_MSG_FIELD_LEN (64). Bridge uses
     * far shorter strings; both fit. Word-wrapped in the draw callback. */
    char detail[64];
    uint8_t anim_frame;
    uint8_t mode;         // 0 = once, 1 = always (persists across requests)
    bool allow_always;    // show the Once/Always toggle (Bridge only — the
                          // desktop wire protocol has no "always" decision)
} PermModel;

typedef enum {
    InfoPageMenu,   // top-level: Help / About / Transcript
    InfoPageHelp,
    InfoPageAbout,
    InfoPageTranscript,
} InfoPage;

typedef struct {
    InfoPage page;
    int index;      // selected item on menu page
    int scroll;     // vertical scroll offset (help / transcript)
    int h_scroll;   // horizontal scroll offset, pixels (transcript only)
    uint8_t anim_frame; // animation counter for about page character
} InfoModel;

typedef struct {
    ViewDispatcher* view_dispatcher;
    View* status_view;
    View* menu_view;
    View* perm_view;
    View* info_view;
    FuriTimer* anim_timer;
    UiEventCallback event_callback;
    void* event_context;
    ViewId current_view;
    bool up_hold_active;
} UiState;

UiState* ui_alloc(Gui* gui);
void ui_free(UiState* ui);
void ui_set_event_callback(UiState* ui, UiEventCallback callback, void* context);
void ui_show_status(UiState* ui, const char* text, bool connected);
void ui_show_status2(UiState* ui, const char* text, const char* subtext, bool connected);
void ui_show_menu(UiState* ui);
void ui_show_listening(UiState* ui);
void ui_set_claude_connected(UiState* ui, bool connected);
void ui_set_pose(UiState* ui, uint8_t pose);
void ui_set_transport_mode(UiState* ui, bool is_bt);
void ui_update_menu(UiState* ui, const char* pipe_delimited);
void ui_show_permission(UiState* ui, const char* tool, const char* detail, bool allow_always);
void ui_show_info(UiState* ui);
void ui_back_to_status(UiState* ui);
void ui_set_muted(UiState* ui, bool muted);
void ui_set_rssi(UiState* ui, int rssi);
void ui_run(UiState* ui);
void ui_stop(UiState* ui);
