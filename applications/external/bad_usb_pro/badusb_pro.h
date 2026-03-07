#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <gui/view.h>
#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>
#include <stdint.h>

#define BADUSB_PRO_SCRIPTS_PATH   EXT_PATH("badusb_pro")
#define BADUSB_PRO_INITIAL_TOKENS 64 /* was 256; 64×160B=10KB fits in RAM  */
#define BADUSB_PRO_MAX_TOKENS     256 /* hard cap on token count             */
#define BADUSB_PRO_MAX_LINE_LEN   256 /* was 512; for stack read buffers     */
#define BADUSB_PRO_TOKEN_STR_LEN  128 /* str_value field in ScriptToken      */
#define BADUSB_PRO_MAX_VARS       16
#define BADUSB_PRO_MAX_FUNCS      16
#define BADUSB_PRO_MAX_STACK      32
#define BADUSB_PRO_VAR_NAME_LEN   32
#define BADUSB_PRO_VAR_VAL_LEN    64 /* was 128                             */
#define BADUSB_PRO_FUNC_NAME_LEN  32
#define BADUSB_PRO_MAX_FILES      32 /* was 64                              */

/* ────────────────────────────────────────────
 *  Token types — every DuckyScript 3.0 command
 * ──────────────────────────────────────────── */
typedef enum {
    /* Basic I/O */
    TokenString, /* STRING <text>          */
    TokenStringLn, /* STRINGLN <text>        */
    TokenDelay, /* DELAY <ms>             */
    TokenRem, /* REM <comment>          */

    /* Single keys */
    TokenKey, /* Bare key name          */
    TokenEnter,
    TokenTab,
    TokenEscape,
    TokenSpace,
    TokenBackspace,
    TokenDelete,
    TokenHome,
    TokenEnd,
    TokenInsert,
    TokenPageUp,
    TokenPageDown,
    TokenUpArrow,
    TokenDownArrow,
    TokenLeftArrow,
    TokenRightArrow,
    TokenPrintScreen,
    TokenPause,
    TokenBreak,
    TokenCapsLock,
    TokenNumLock,
    TokenScrollLock,
    TokenMenu,

    /* Function keys */
    TokenF1,
    TokenF2,
    TokenF3,
    TokenF4,
    TokenF5,
    TokenF6,
    TokenF7,
    TokenF8,
    TokenF9,
    TokenF10,
    TokenF11,
    TokenF12,

    /* Modifiers (standalone or in combos) */
    TokenGui,
    TokenAlt,
    TokenCtrl,
    TokenShift,

    /* Key combo: modifier(s) + key */
    TokenKeyCombo,

    /* Flow control */
    TokenIf,
    TokenElse,
    TokenEndIf,
    TokenWhile,
    TokenEndWhile,
    TokenRepeat,
    TokenStop,

    /* Variables */
    TokenVar,

    /* Functions */
    TokenFunction,
    TokenEndFunction,
    TokenCall,

    /* LED feedback channel */
    TokenLedCheck,
    TokenLedWait,

    /* OS detection */
    TokenOsDetect,

    /* Timing defaults */
    TokenDefaultDelay,
    TokenDefaultStringDelay,

    /* Mouse */
    TokenMouseMove,
    TokenMouseClick,
    TokenMouseScroll,

    /* Consumer keys */
    TokenConsumerKey,

    /* Script restart */
    TokenRestart,

    TokenCount /* sentinel */
} BadUsbTokenType;

/* ────────────────────────────────────────────
 *  A single parsed token
 * ──────────────────────────────────────────── */
typedef struct {
    BadUsbTokenType type;
    char str_value[BADUSB_PRO_TOKEN_STR_LEN];
    int32_t int_value;
    int32_t int_value2; /* second param, e.g. mouse y */
    uint16_t keycodes[8]; /* for key combos */
    uint8_t keycode_count;
    uint16_t source_line; /* 1-based line number in file */
} ScriptToken;

/* ────────────────────────────────────────────
 *  Script state machine
 * ──────────────────────────────────────────── */
typedef enum {
    ScriptStateIdle,
    ScriptStateLoaded,
    ScriptStateRunning,
    ScriptStatePaused,
    ScriptStateDone,
    ScriptStateError,
} ScriptState;

/* ────────────────────────────────────────────
 *  Variable storage
 * ──────────────────────────────────────────── */
typedef struct {
    char name[BADUSB_PRO_VAR_NAME_LEN];
    char value[BADUSB_PRO_VAR_VAL_LEN];
} ScriptVar;

/* ────────────────────────────────────────────
 *  Function block (start/end token indices)
 * ──────────────────────────────────────────── */
typedef struct {
    char name[BADUSB_PRO_FUNC_NAME_LEN];
    uint16_t start_index; /* token after FUNCTION line */
    int32_t end_index; /* token of END_FUNCTION, -1 if unmatched */
} ScriptFunc;

/* ────────────────────────────────────────────
 *  Execution engine state
 * ──────────────────────────────────────────── */
typedef struct {
    ScriptToken* tokens; /* dynamically allocated token array */
    uint32_t token_count;
    uint32_t token_capacity; /* allocated size of tokens array   */

    uint16_t pc; /* program counter (token index) */
    ScriptState state;

    ScriptVar vars[BADUSB_PRO_MAX_VARS];
    uint8_t var_count;

    ScriptFunc funcs[BADUSB_PRO_MAX_FUNCS];
    uint8_t func_count;

    uint16_t call_stack[BADUSB_PRO_MAX_STACK];
    uint8_t call_depth;

    uint16_t default_delay; /* ms between commands     */
    uint16_t default_string_delay; /* ms between characters   */

    float speed_multiplier; /* 0.5, 1.0, 2.0, 4.0     */

    uint8_t led_state; /* last polled LED bitmask */

    char error_msg[128];
    uint16_t error_line;

    /* Callback for UI updates */
    void (*status_callback)(void* ctx);
    void* callback_ctx;
} ScriptEngine;

/* ────────────────────────────────────────────
 *  Injection mode
 * ──────────────────────────────────────────── */
typedef enum {
    InjectionModeUSB,
    InjectionModeBLE,
} InjectionMode;

/* ────────────────────────────────────────────
 *  Speed setting indices
 * ──────────────────────────────────────────── */
typedef enum {
    SpeedHalf = 0,
    SpeedNormal,
    SpeedDouble,
    SpeedQuad,
    SpeedCount,
} SpeedSetting;

/* ────────────────────────────────────────────
 *  Views
 * ──────────────────────────────────────────── */
typedef enum {
    ViewFileBrowser,
    ViewScriptInfo,
    ViewExecution,
    ViewSettings,
} BadUsbView;

/* ────────────────────────────────────────────
 *  Main application context
 * ──────────────────────────────────────────── */
typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* file_browser;
    Widget* script_info;
    View* execution_view;
    VariableItemList* settings;

    ScriptEngine engine;
    InjectionMode injection_mode;
    SpeedSetting speed_setting;
    uint16_t settings_default_delay;

    char script_path[256];
    char script_name[64];
    uint32_t script_size;
    uint16_t script_line_count;

    char file_list[BADUSB_PRO_MAX_FILES][64];
    uint8_t file_count;

    FuriThread* worker_thread;
    bool worker_running;

    FuriHalUsbInterface* prev_usb_mode;
    volatile bool usb_restored; /* atomic flag to prevent double USB restore */
} BadUsbProApp;

/* ────────────────────────────────────────────
 *  Execution-view draw model
 * ──────────────────────────────────────────── */
typedef struct {
    uint16_t current_line;
    uint16_t total_lines;
    char current_cmd[64];
    uint8_t led_num;
    uint8_t led_caps;
    uint8_t led_scroll;
    ScriptState state;
    char error_msg[64];
    char detected_os[8]; /* set when OS_DETECT token completes: "WIN", "MAC", "LINUX" */
} ExecutionViewModel;
