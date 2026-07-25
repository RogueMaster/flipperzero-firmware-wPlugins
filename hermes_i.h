#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_input.h>
#include <gui/modules/number_input.h>
#include <gui/modules/byte_input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "hermes_icons.h" // generated from icons/ by fbt

#include "helpers/baud_table.h"
#include "helpers/autobaud.h"
#include "helpers/verifier.h"
#include "helpers/term.h"
#include "helpers/uart_tap.h"
#include "helpers/session_log.h"
#include "helpers/selftest.h"
#include "helpers/trigger.h"
#include "helpers/script.h"
#include "helpers/sessionstats.h"

#include "views/detect_view.h"
#include "views/result_view.h"
#include "views/console_view.h"
#include "views/wiring_view.h"
#include "views/selftest_view.h"
#include "views/summary_view.h"

#include "scenes/hermes_scene.h"

#define HERMES_VERSION "1.3"

/* Bounds for the custom-rate entry. Below 50 the timing is absurd; above
 * 2 Mbaud the LPUART cannot follow and the USART is at its limit. */
#define HERMES_BAUD_MIN (50)
#define HERMES_BAUD_MAX (2000000)

/* How long the autoboot interrupt hammers the key for. Bootloader countdowns
 * are typically 1-3 seconds, so this covers one with margin. */
#define HERMES_AUTOBOOT_MS (4000u)
#define HERMES_AUTOBOOT_INTERVAL_MS (20u)

/* Give up on a quiet line rather than listening forever. A busy console fills
 * the buffer in milliseconds; this is purely the patience for a silent one. */
#define HERMES_LISTEN_TIMEOUT_MS (4000u)

#define HERMES_TEXT_INPUT_MAX (64u)

/* Raw bytes the hex-send screen can compose at once. */
#define HERMES_HEX_MAX (32u)

typedef enum {
    HermesViewSubmenu,
    HermesViewSettings,
    HermesViewWidget,
    HermesViewTextInput,
    HermesViewNumberInput,
    HermesViewByteInput,
    HermesViewDetect,
    HermesViewResult,
    HermesViewConsole,
    HermesViewWiring,
    HermesViewSelfTest,
    HermesViewSummary,
} HermesViewId;

typedef enum {
    HermesCustomEventVerifyTick = 100, // the verifier closed a window
    HermesCustomEventVerifyDone,
    HermesCustomEventConsoleText,
    HermesCustomEventConsoleCtrl,
    HermesCustomEventConsoleEnter,
    HermesCustomEventResultRetry,
    HermesCustomEventSelfTestTick,
    HermesCustomEventSelfTestDone,
    HermesCustomEventBaudEntered,
    HermesCustomEventTriggerSet,
    HermesCustomEventHexEntered,

    /* Something else holds the port. Raised from an on_enter, which cannot
     * navigate directly without nesting a scene transition inside itself, so
     * it goes through the queue and unwinds on the next dispatch. */
    HermesCustomEventPortBusy,

    /* The chosen entry's index is added to this, so it needs a range of its
     * own rather than the next number along. */
    HermesCustomEventResultPicked = 200,
} HermesCustomEvent;

/** What the console scene should open, decided by detect or by the manual picker. */
typedef struct {
    uint32_t baud;
    HermesFraming framing;
    bool verified;
} HermesLink;

/** Transient bookkeeping for the detect scene's listen phase. */
typedef struct {
    uint32_t started_tick;
    uint32_t last_fit_tick;
    uint32_t live_baud; // best guess so far, shown while still listening
    uint32_t bit_time; // scales the scope trace once we have a fit
} HermesDetectRuntime;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    /* shared modules */
    Submenu* submenu;
    VariableItemList* var_item_list;
    Widget* widget;
    TextInput* text_input;
    NumberInput* number_input;
    ByteInput* byte_input;

    /* custom views */
    DetectView* detect_view;
    ResultView* result_view;
    ConsoleView* console_view;
    WiringView* wiring_view;
    SelfTestView* selftest_view;
    SummaryView* summary_view;

    /* engines */
    Autobaud* autobaud;
    Verifier* verifier;
    UartTap* tap;
    Term* term;
    SessionLog* log;
    SelfTest* selftest;
    Trigger* trigger;
    Script* script;

    /* state carried between scenes */
    AutobaudResult detect_result;
    HermesDetectRuntime detect_rt;
    HermesLink link;
    char text_buffer[HERMES_TEXT_INPUT_MAX];

    /* hex-send composition buffer + its current length */
    uint8_t hex_buffer[HERMES_HEX_MAX];
    uint8_t hex_len;

    /* filled when the console closes, shown by the summary scene */
    SessionStats last_session;

    /* live console bookkeeping */
    uint32_t session_start_tick; // for the summary's duration
    uint32_t marker_count; // increments per "drop marker"

    /* settings */
    HermesPort port;
    UartTapEnter enter_mode;
    bool tx_enabled;
    bool local_echo;
    bool logging; // capture console sessions to the SD card
    bool sound;
    bool led;

    int32_t custom_baud; // remembered between visits to Manual Console

    /* Tick deadline for the autoboot burst; 0 when idle. Driven from the
     * console tick so the UI stays live while it hammers. */
    uint32_t autoboot_until;
} HermesApp;

/* feedback (defined in hermes.c), all gated by settings */
void hermes_notify_found(HermesApp* app, bool good);
void hermes_notify_blip(HermesApp* app);

/** A watched string appeared. Deliberately not gated by the sound/LED
 *  settings: you armed a watch precisely so it would interrupt you. */
void hermes_notify_trigger(HermesApp* app);
