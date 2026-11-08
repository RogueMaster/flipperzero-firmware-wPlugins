/* The Flipper app's screen flow as a pure reducer
 * (docs/design/flipper-ui-spec.md; the screens are numbered there).
 *
 *   Waiting ⇄ Ready → Reading → LookingUp → Result{Known|UnknownPrefill|
 *   UnknownBare|Blank|NoHost} ⇄ Menu ⇄ Details / ConfirmDelete
 *   Result → Weight → Busy → Saved → Ready
 *   Result → Toast{MarkedNew|MarkedEmpty} → (commit) Busy → Ready
 *   Result → Busy → Toast{Added|Deleted} → Ready
 *   Result → LinkOnHost → (fresh lookup) Result
 *   any intent failure → Error → Retry (same id) / Back
 *   Waiting / Ready → Settings ⇄ About
 *   Ready → Recent → Result{Known}
 *   Waiting ⇄ GetApp → (host connected) Ready
 *
 * app_reduce() applies one event to the state and returns the I/O the
 * caller must perform; it never touches the GUI, NFC or the transport. The
 * screens draw from the state. Keys arrive as AppEventKey and mean what the
 * spec's key map says for the screen on display; keys and events that do
 * not apply to it are ignored, and so are a `lookup` for another scan and a
 * `result` for an intent that is not in flight.
 *
 * Effects: on AppEffectSendScan the caller sends app_encode_scan(), on
 * AppEffectSendIntent app_encode_intent(). On AppEffectSaveSettings it
 * stores s->settings; on AppEffectSwitchTransport it stores them too,
 * closes the transports s->connection does not use and opens those it
 * uses that are not running (a failed one included); a host on a closed
 * transport is lost through link_select as its port closes, a host on one
 * that keeps running stays. AppEffectExit closes the app. When the
 * host cannot have the scan, or surely did not get the intent, the caller
 * sends AppEventSendFailed: a scan then shows the local facts
 * (Result{NoHost}) and an intent an error. An intent the host may have got
 * waits for its result or the timeout.
 *
 * Time: the caller stamps every event with its clock (now_ms) and sends
 * AppEventTick periodically; the tick runs out the result timeout (Busy,
 * LinkOnHost until its result), the Saved screen and the 3 s toasts
 * (app_timer_left_ms() for the progress bar).
 *
 * Feedback: after each call s->cue says which feedback event happened
 * (AppCueNone when none); the caller maps it to LED / vibro / sound per the
 * Feedback and Sound settings.
 *
 * A failed read shows Error{ReadFailed} for APP_READ_RETRY_MS, then Ready.
 * A tag the app cannot read at all (AppEventTagUnsupported) shows
 * Error{UnsupportedTag} the same way, without an error cue; the reader
 * keeps the tag remembered, so it is not read in a loop.
 *
 * Leaving a result nobody answered (Lookup failed, or a scan shown without
 * the host) sets `forget_tag`, so the spool still on the Flipper is read
 * again on Ready.
 *
 * Mark as new / Mark as empty send nothing at first: their toast holds the
 * intent for 3 s, ◀ Undo drops it, and the deadline, OK, Back or a new tag
 * commit it (OK on Mark as new then opens Weight). Only one intent is ever
 * in flight, so a tag on such a toast commits the mark and is not read; the
 * reader finds it again on the next screen that reads.
 *
 * Weight opens in the Weigh as setting's mode every time; the screen's
 * mode chip toggles it for that entry and converts the number on screen,
 * typed or not (gross - core, floored at 0, or net + core). The pre-fill
 * on opening Weight is gross = remaining + core, net = remaining of the
 * spool on screen, falling back to the lookup defaults (gross: last gross,
 * else label + core; net: gross - core, else label).
 *
 * Recent lists the spools whose weight was saved this session, newest
 * first, one entry per tag, APP_RECENT_MAX at most. OK on an entry shows
 * its Result{Known} from the stored scan and spool, as if just scanned: its
 * intents carry that scan's id and tag (the host finds the row or the
 * tag). A later lookup or result for a listed tag refreshes its entry in
 * place, a delete drops it.
 *
 * After scan = Weigh opens Weight in place of Result{Known} when a scan's
 * lookup finds the spool (not one created on the scan, not a fresh lookup
 * replacing a result); Back from it shows the result.
 *
 * Get app (Waiting's OK) shows where the phone app comes from; the reader
 * runs there as on Waiting. The app starts on Waiting: the caller sends
 * AppEventStart once it has set the stored settings, and it changes
 * nothing.
 *
 * The caller sets s->settings and s->connection from the stored settings
 * after app_state_init(). Events about a transport (a host connected over
 * it, it failed to open, a host app of another protocol said hello there)
 * name it in `via`. The BLE status is only recorded for the screens; it
 * never changes the screen or `connected`.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app_settings.h"
#include "intents.h"
#include "proto_decode.h"
#include "tag_dump.h"
#include "transport.h"

/* How long the Flipper waits for a `result` (PROTOCOL.md, Timeouts). */
#define APP_RESULT_TIMEOUT_MS 10000u
#define APP_SAVED_MS          1300u
#define APP_TOAST_MS          3000u
#define APP_READ_RETRY_MS     1500u /* Error{ReadFailed} returns to Ready */
#define APP_DETAILS_SHOWN     5u
/* Tag details rows on screen; ▼ stops at the last (ui_details_count) */
#define APP_LIST_ROWS         4 /* visible rows of Menu and Settings */
#define APP_MENU_MAX          6
#define APP_WEIGHT_DIGITS     4
#define APP_RECENT_MAX        6

typedef enum {
    AppScreenWaiting, /* no host */
    AppScreenReady,
    AppScreenReading,
    AppScreenLookingUp,
    AppScreenResult,
    AppScreenMenu,
    AppScreenDetails,
    AppScreenConfirmDelete,
    AppScreenWeight,
    AppScreenBusy, /* the intent in flight says Saving / Adding / Deleting / Updating */
    AppScreenSaved,
    AppScreenToast,
    AppScreenLinkOnHost,
    AppScreenError,
    AppScreenSettings,
    AppScreenAbout,
    AppScreenRecent,
    AppScreenGetApp, /* the QR code of the phone app's page */
} AppScreen;

typedef enum {
    AppResultKnown,
    AppResultUnknownPrefill, /* Bambu tag with material: Add */
    AppResultUnknownBare, /* tag data without material: Create on host */
    AppResultBlank, /* no spool data */
    AppResultNoHost, /* local facts only */
} AppResultKind;

typedef enum {
    AppToastAdded,
    AppToastMarkedNew, /* holds the `full` intent */
    AppToastMarkedEmpty, /* holds the `empty` intent */
    AppToastDeleted,
} AppToast;

/* What Ready's bar says after the last action; the spool it names is
 * s->spool. */
typedef enum {
    AppOutcomeNone,
    AppOutcomeSaved,
    AppOutcomeAdded,
    AppOutcomeMarkedNew,
    AppOutcomeMarkedEmpty,
    AppOutcomeDeleted,
} AppOutcome;

typedef enum {
    AppErrorReadFailed, /* the tag could not be read */
    AppErrorUnsupportedTag, /* a tag the app cannot read at all; no error cue */
    AppErrorLookup, /* the host answered `lookup` with status error; see lookup */
    AppErrorHost, /* the host answered `result` with ok false; see result */
    AppErrorTimeout, /* no `result` in time */
    AppErrorHostLost, /* the host went away while an intent was in flight */
    AppErrorSendFailed, /* the intent could not be sent */
} AppError;

typedef enum {
    AppMenuWeigh,
    AppMenuMarkNew,
    AppMenuMarkEmpty,
    AppMenuRelink, /* "Relink in app" */
    AppMenuDetails,
    AppMenuDelete,
    AppMenuAddNew,
    AppMenuAddAndWeigh,
    AppMenuCreateOnHost,
    AppMenuLinkOnHost,
} AppMenuItem;

typedef enum {
    AppSettingConnection,
    AppSettingAutosave,
    AppSettingAfterScan,
    AppSettingWeighAs,
    AppSettingFeedback,
    AppSettingSound,
    AppSettingVersion, /* OK opens About */
    AppSettingCount,
} AppSetting;

typedef enum {
    AppBleOff, /* not advertising, or Bluetooth is not the transport */
    AppBleAdvertising,
    AppBleConnected, /* a host subscribed to TX; it may not have said hello */
} AppBleStatus;

typedef enum {
    AppKeyUp,
    AppKeyDown,
    AppKeyLeft,
    AppKeyRight,
    AppKeyOk,
    AppKeyBack,
} AppKey;

typedef enum {
    AppEventHostConnected,
    AppEventHostLost,
    AppEventTagDetected, /* the reader found a tag and starts reading it */
    AppEventTagRead, /* .tag */
    AppEventReadFailed,
    AppEventTagUnsupported, /* the reader met a tag it cannot read, and never will */
    AppEventLookupReceived, /* .lookup */
    AppEventResultReceived, /* .result */
    AppEventSendFailed, /* the scan or intent of the last effect was not sent */
    AppEventKey, /* .key; a held ▲▼ repeats as more presses */
    AppEventTick,
    AppEventBleStatus, /* .ble */
    AppEventTransportFailed, /* the transport `via` did not open */
    AppEventOtherVersion, /* .version: a host hello of another protocol version, 0 clears */
    AppEventStart, /* the stored settings are set; changes nothing */
} AppEventType;

typedef struct {
    AppEventType type;
    uint32_t now_ms; /* the caller's millisecond clock; may wrap */
    TransportKind via; /* HostConnected, TransportFailed, OtherVersion */
    union {
        const TagDump* tag;
        const ProtoLookup* lookup;
        const ProtoResult* result;
        AppKey key;
        AppBleStatus ble;
        uint8_t version;
    };
} AppEvent;

typedef enum {
    AppEffectNone,
    AppEffectSendScan,
    AppEffectSendIntent,
    AppEffectSaveSettings,
    AppEffectSwitchTransport, /* run what s->connection uses, saving the settings */
    AppEffectExit,
} AppEffect;

typedef enum {
    AppCueNone,
    AppCueConnected, /* blue + short vibro */
    AppCueRead, /* blue + short vibro */
    AppCueSuccess, /* green + vibro (+ success sound) */
    AppCueError, /* red + long vibro (+ error sound) */
} AppCue;

typedef struct {
    uint8_t digit[APP_WEIGHT_DIGITS]; /* most significant first */
    uint8_t cursor; /* 0: the mode chip, 1..4: a digit */
    bool touched; /* a digit was changed */
} AppWeight;

/* A Recent entry: the scan and the spool as last known. */
typedef struct {
    Scan scan;
    ProtoSpool spool;
} AppRecent;

typedef struct {
    AppScreen screen;
    AppResultKind result_kind; /* of the Result the other screens return to */
    AppToast toast; /* on Toast */
    AppOutcome last; /* on Ready */
    AppError error; /* on Error; meaningless elsewhere */
    bool retryable; /* on Error: ▶ resends `held` under its id */
    AppCue cue; /* of the last app_reduce() call */
    bool forget_tag; /* the tag read last may still be on the Flipper and is to be read
                      * again: the caller makes the reader forget it, then clears this */
    /* Waiting: per transport, the protocol of a host app that said hello
     * there, 0 none */
    uint8_t other_version[TRANSPORT_KINDS];
    bool connected;
    TransportKind via; /* the connected host's transport */
    AppConnection connection; /* in use */
    bool transport_failed[TRANSPORT_KINDS]; /* did not open; leaving Settings retries it */
    AppBleStatus ble;
    AppSettings settings;
    AppSettings settings_entered; /* as Settings opened */
    WeightMode mode; /* on Weight: Weigh as when it opened, then the chip's */
    AppWeight weight; /* on Weight */
    bool weigh_after_full; /* the `full` in flight opens Weight when done */
    uint8_t sel; /* selected row of Menu / Settings / Recent */
    uint8_t top; /* first visible row of Menu / Settings / Recent */
    uint8_t details_top; /* first visible row of Tag details, see APP_DETAILS_SHOWN */
    uint32_t timer_start_ms;
    uint32_t timer_ms; /* 0: no deadline on this screen */
    Scan scan; /* the tag on screen */
    ProtoLookup lookup; /* the host's answer for scan */
    ProtoResult result; /* the host's answer for the last intent */
    bool has_spool; /* the Bambuddy spool of the tag on screen */
    ProtoSpool spool; /* from lookup.spool, then result.spool */
    IdSeq scan_ids;
    IdSeq intent_ids;
    IntentStore* intents;
    Intent held; /* the last intent built: the one sent, failed or held by a toast */
    AppRecent recent[APP_RECENT_MAX]; /* newest first */
    uint8_t recent_count;
} AppState;

/* Waiting, no host, the default settings and their connection (Auto) with
 * BLE off, gross mode.
 * `first_scan` and `first_intent` seed the id counters (see IdSeq). */
void app_state_init(AppState* s, IntentStore* intents, uint32_t first_scan, uint32_t first_intent);

AppEffect app_reduce(AppState* s, const AppEvent* e);

/* The `scan` for s->scan with the Auto-save new setting, and the intent in
 * flight; see proto_encode.h for the buffer contract (0: nothing to send). */
size_t app_encode_scan(const AppState* s, char* buf, size_t cap);
size_t app_encode_intent(const AppState* s, char* buf, size_t cap);

/* Whether the NFC reader should look for a tag on this screen. */
bool app_reader_wanted(const AppState* s);

/* Milliseconds until the screen's deadline (0 when due or none). */
uint32_t app_timer_left_ms(const AppState* s, uint32_t now_ms);

/* The Menu rows for the result on screen; returns their count. */
uint8_t app_menu_items(const AppState* s, AppMenuItem out[APP_MENU_MAX]);

/* The Weight screen's number, the core it is checked against (0 when
 * unknown) and whether Save is refused (gross below the core). */
uint16_t app_weight_grams(const AppState* s);
uint16_t app_weight_core(const AppState* s);
bool app_weight_below_core(const AppState* s);
