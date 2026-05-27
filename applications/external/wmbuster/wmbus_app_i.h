#pragma once
/* Shared types for wM-Bus Inspector. Kept minimal to limit RAM. */

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/popup.h>
#include <storage/storage.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "protocol/wmbus_link.h"

#define WMBUS_APP_NAME           "wM-Buster"
#define WMBUS_APP_DATA_DIR       EXT_PATH("apps_data/wmbuster")
#define WMBUS_APP_KEYS_FILE      WMBUS_APP_DATA_DIR "/keys.csv"
#define WMBUS_APP_LOG_DIR        WMBUS_APP_DATA_DIR "/log"

#define WMBUS_MAX_METERS         32
#define WMBUS_MAX_FRAME          290    /* L can be up to 255 + overhead */

/* RX modes the user can pick from Settings.
 *
 *   T1   — 868.95 MHz, 100 kbps 3-of-6 encoded, sync 0x543D.
 *          Used by older Techem firmware and most Diehl/Itron meters.
 *
 *   C1   — 868.95 MHz, 100 kbps NRZ, sync 0x543D + 0xCD/0x3D postamble.
 *          The slicer auto-detects Format A (0xCD) vs Format B (0x3D).
 *          Used by newer Techem firmware (v0x6A) and many Kamstrup units.
 *
 *   CT   — Combined T+C scan on a single CC1101 config. The sync word is
 *          identical for both modes; we route on the first byte after
 *          sync (0xCD/0x3D = C1, anything else = T1 chip stream).
 *          Recommended default — doubles coverage with no extra battery
 *          cost compared to single-mode listening.
 *
 *   S1   — 868.30 MHz, 32.768 kbps Manchester. Older drinking-water and
 *          gas meters. Lower bit rate but very long range.                */
typedef enum {
    WmbusModeT1 = 0,
    WmbusModeC1,                /* auto Format A / Format B               */
    WmbusModeCT,                /* combined T+C — recommended default     */
    WmbusModeS1,
    WmbusMode_Count,
} WmbusMode;

typedef struct {
    uint32_t id;            /* device id (binary or BCD as transmitted) */
    uint16_t manuf;         /* M field */
    uint8_t  version;
    uint8_t  medium;
    int8_t   rssi;
    uint32_t last_seen_tick;
    uint32_t telegram_count;
    char     last_value[32];/* short single-line summary for the meter list */
    bool     encrypted;
    bool     decrypted_ok;
    bool     have_key;       /* a key for this meter is present in the store */

    /* Per-meter snapshot of the most recent telegram. We keep these next
     * to the meter row (instead of a single global "last") so the Detail
     * view always shows data for the meter the user actually selected,
     * even when other meters are transmitting simultaneously. */
    uint8_t  ci;
    uint8_t  apdu_len;
    uint8_t  apdu[80];
    char     text[200];
} WmbusMeter;

typedef enum {
    WmbusFilterAll = 0,        /* show every meter                     */
    WmbusFilterTop10,          /* show 10 strongest signals only       */
    WmbusFilterTop5,           /* show 5  strongest signals only       */
    WmbusFilterTop3,           /* show 3  strongest signals only       */
    WmbusFilter_Count,
} WmbusFilter;

typedef enum {
    WmbusSortRssi = 0,         /* strongest first  (find your meter)  */
    WmbusSortRecent,           /* most recently heard first           */
    WmbusSortId,               /* by device id ascending              */
    WmbusSortPackets,          /* most chatty meter first             */
    WmbusSort_Count,
} WmbusSort;

typedef enum {
    WmbusModuleInternal_ = 0,   /* on-board CC1101 (default)         */
    WmbusModuleExternal_ = 1,   /* GPIO-attached external CC1101     */
    WmbusModule_Count_,
} WmbusModuleSetting;

typedef struct {
    WmbusMode mode;
    uint32_t  freq_hz;     /* derived from mode; kept in sync */
    bool      logging;
    bool      auto_decrypt;
    WmbusFilter filter;    /* RSSI-based filter for the meter list */
    WmbusSort   sort;      /* ordering of the meter list */
    WmbusModuleSetting module; /* internal vs external CC1101       */
} WmbusSettings;

typedef enum {
    WmbusSceneStart,
    WmbusSceneMeters,
    WmbusSceneDetail,
    WmbusSceneSettings,
    WmbusSceneAbout,
    WmbusSceneKeys,
    WmbusScene_Count,
} WmbusScene;

typedef enum {
    WmbusViewMeters,
    WmbusViewDetail,
    WmbusViewSettings,
    WmbusViewSubmenu,
    WmbusViewPopup,
    WmbusViewScan,            /* canvas-based meter list (custom) */
    WmbusViewDetailCanvas,    /* canvas-based detail view (custom) */
    WmbusViewAbout,           /* canvas-based about page (custom)  */
    WmbusView_Count,
} WmbusView;

typedef struct WmbusApp WmbusApp;
typedef struct WmbusWorker WmbusWorker;

struct ScanCanvas;        /* fwd-decl: defined in views/scan_canvas.h     */
struct DetailCanvas;      /* fwd-decl: defined in views/detail_canvas.h   */
struct AboutCanvas;       /* fwd-decl: defined in views/about_canvas.h    */
struct KeyStore;          /* fwd-decl: defined in key_store.h             */

struct WmbusApp {
    Gui*               gui;
    ViewDispatcher*    view_dispatcher;
    SceneManager*      scene_manager;
    Submenu*           submenu;
    VariableItemList*  var_list;
    TextBox*           text_box;
    Popup*             popup;
    struct ScanCanvas*   scan_canvas;
    struct DetailCanvas* detail_canvas;
    struct AboutCanvas*  about_canvas;
    struct KeyStore*     key_store;
    FuriString*        text_buf;
    Storage*           storage;
    NotificationApp*   notifications;

    WmbusSettings      settings;
    WmbusMeter         meters[WMBUS_MAX_METERS];
    size_t             meter_count;
    int                selected;     /* index into meters[] */

    /* Last raw + parsed telegram (for the Detail view). */
    uint8_t            last_frame[WMBUS_MAX_FRAME];
    size_t             last_frame_len;
    WmbusLinkFrame     last_link;
    char               last_text[512];

    WmbusWorker*       worker;
    FuriMutex*         lock;
    bool               led_active;     /* set while cyan blink is running */
    bool               on_meters_view; /* tick callback rebuilds list while true */
    uint32_t           telegram_seq;   /* incremented per accepted telegram */
    uint32_t           last_shown_seq; /* what UI last drew */

    /* Cumulative count of every accepted telegram, regardless of whether
     * the meter row was retained or evicted from the bounded meters[]
     * table. Useful as a "the radio is alive" indicator when the storage
     * cap (WMBUS_MAX_METERS) has been hit and meter_count plateaus. */
    uint32_t           total_telegrams;

    /* Submenu state for the meter list. We avoid `submenu_reset` on every
     * tick because each reset tears down all rows, reallocates strings
     * and resets the selection cursor. Instead the meter list scene
     * tracks the current row->meter mapping here, calls reset+add_item
     * only when count or sort order changes (rare), and otherwise just
     * updates the visible label of each row in place. */
    uint16_t           row_to_meter[WMBUS_MAX_METERS];
    uint16_t           row_count;
    uint32_t           last_full_rebuild_tick;
};

/* Lifecycle */
WmbusApp* wmbus_app_alloc(void);
void wmbus_app_free(WmbusApp* app);

/* Called from worker context with a fully decoded link-layer frame. */
void wmbus_app_on_telegram(
    WmbusApp* app,
    const uint8_t* frame, size_t len,
    int8_t rssi);

/* Idempotent helpers: start/stop the SubGHz worker + cyan scan LED.
 * Used by the Meters and Detail views so navigation between them does not
 * tear the radio down. */
void wmbus_scanning_start(WmbusApp* app);
void wmbus_scanning_stop(WmbusApp* app);

/* LED-only control: pause/resume the scan LED without touching the radio
 * worker. Used by views that are not scan-related (Root, Settings) so the
 * user gets a clear visual cue of "are we listening?" without churning
 * CC1101 state. */
void wmbus_scanning_indicator_off(WmbusApp* app);
void wmbus_scanning_indicator_on (WmbusApp* app);
