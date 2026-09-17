#pragma once

#include <furi.h>
#include <furi_hal_rtc.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_box.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification.h>

#include "scenes/sigroam_scene.h"
#include "src/sr_settings_store.h"
#include "src/sr_io.h"
#include "src/sr_worker.h"
#include "src/sr_model.h"
#include "src/sr_bloom.h"
#include "src/sr_handshake.h"
#include "src/sr_scan_ctl.h"
#include "src/sr_gps_sample.h"
#include "src/sr_poi.h"
#include "src/sr_rawlog.h"
#include "src/sr_source_codec.h"
#include "src/sr_notify.h"
#include "views/sr_view_dash.h"

#define SR_TAG         "SigRoam"
#define SR_FAP_VERSION "0.3"

/*
 * Brand / referral slot (About page).
 *
 * SR_BRAND_URL is the app's only outbound entry point, and it **resolves to the
 * same short link** as the QR code in the top-right of the About page
 * (assets/sr1g_qr.png, encoding "https://" SR_BRAND_URL).
 * Plaintext and code must both be present: the established product-line finding
 * is "print the plaintext short link next to the code -- the security audience
 * does not scan unknown codes, and plaintext equals trust", and wardriving users
 * are exactly that audience.
 *
 * The short code sr1g follows the frozen naming rule `<product><hw version><purpose>`:
 * sr=SigRoam / 1=v1 / g=github.
 * The target lives in Cloudflare KV, so **changing it needs no code change and no reflash**.
 *
 * Constraints (read before editing):
 *  (1) The plaintext line must be <= 20 chars (one full-width row). It is exactly
 *      20 today, leaving **zero headroom** -- any longer and it wraps, and since
 *      the About page moved to a hand-drawn layout there is no scrolling, so the
 *      wrapped part is simply invisible.
 *  (2) Changing this value REQUIRES regenerating assets/sr1g_qr.png, or the code
 *      and the plaintext point to different places.
 *      With the "https://" prefix the URL is 28 chars; QR version 3 (29x29 modules)
 *      at error correction M holds 42 bytes, leaving 14 spare -- the short code can
 *      grow from 4 up to 18 chars without changing version or code size.
 *  (3) Platform compliance: all 8 clauses of the official Contributing.md impose no
 *      restriction on commercial promotion, outbound links, or branding (read
 *      2026-08-31), but clause 8 reserves refusal "for any reason" -- so keep the
 *      tone out of advertising register.
 *
 * NOTE: SR_BRAND_NAME was removed (2026-09-01, T4.14): after the About page was
 *    rearranged the brand no longer occupies its own row; the short-link domain
 *    go.pingequa.com carries it instead, leaving that macro with no users.
 */
#define SR_BRAND_URL "go.pingequa.com/sr1g"

/* Flipper LCD is 128x64. Fullscreen attach has no status bar, so About
 * text-scroll uses the full canvas. (Plan/T2.4: 128 px wide.) */
#define SR_CANVAS_W 128
#define SR_CANVAS_H 64

/* About page QR geometry (T4.14). **Regenerate the image before changing these
 * numbers; do not just edit the numbers**:
 *  - SIDE = 37 is the actual edge length of assets/sr1g_qr.png, determined by the
 *    QR version (V3 = 29 modules + a 4-module quiet zone on each side = 37, with
 *    box_size=1 giving 1 px per module).
 *    A URL crossing 42 bytes jumps to V4 (33 modules -> 41 px), which must be
 *    mirrored here.
 *  - X is derived by right alignment: 91 + 37 = 128 sits flush with the right
 *    edge, leaving 91 px on the left for the upper text block.
 * The quiet zone is already inside that 37 -- drawing any element into the
 * rectangle x>=91 and y<37 makes the code unscannable. */
#define SR_ABOUT_QR_SIDE 37
#define SR_ABOUT_QR_X    (SR_CANVAS_W - SR_ABOUT_QR_SIDE)

#define SR_ABOUT_TEXT_MAX 640
#define SR_PROBE_TEXT_MAX 320
#define SR_RAW_TEXT_MAX (SR_RAWLOG_LINES * (SR_RAWLOG_LINE_MAX + 2) + 1)
#define SR_TICK_PERIOD_MS 100

typedef enum {
    SigRoamViewSubmenu,
    SigRoamViewWidget,
    SigRoamViewTextBox,
    SigRoamViewDash,
    SigRoamViewVarList,
} SigRoamView;

typedef struct {
    Gui* gui;
    NotificationApp* notify;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    FuriMutex* mtx;
    Submenu* submenu;
    Widget* widget;
    TextBox* text_box;
    VariableItemList* var_list;
    VariableItem* var_item_apply;
    SrViewDash* dash;
    char about_text[SR_ABOUT_TEXT_MAX];
    SrSettings settings;
    SrSettings settings_entry_snapshot;
    char baud_text[12];
    SrIo* io;
    SrIoStatus io_status;
    SrBloom* bloom;
    SrModel model;
    SrWorker* worker;
    /* Last entered item of the Start menu (a SigRoamStartItem value, not a display
     * position). submenu_set_selected_item matches on the item's index field --
     * read from the official submenu.c, which walks items comparing ->index rather
     * than using the array subscript (V-067).
     * Allocation memsets to 0, so the initial value 0 = SigRoamStartItemDash, which
     * is exactly the first entry in the new ordering. */
    uint8_t start_selected;
    uint32_t tick_n;
    SrHandshakeCtx hs;
    SrHandshakeState hs_shown; /* The state already rendered */
    uint32_t hs_rev_shown; /* model.firmware_rev at the time it was rendered */
    char probe_text[SR_PROBE_TEXT_MAX];
    char raw_text[SR_RAW_TEXT_MAX];
    uint32_t raw_pushed_shown; /* Snapshot of rawlog.pushed at the time it was rendered */
    SrRawLog rawlog;
    /* app->scan / app->scan_cmdack_at_send / app->probe_send_busy /
     * app->raw_text / app->raw_pushed_shown /
     * app->dash / app->gps_sample / app->settings are GUI-thread exclusive:
     * read and written only by scene on_enter / on_event / button callbacks, and
     * never touched by the worker thread.
     * They therefore need no app->mtx protection of their own -- ViewDispatcher is
     * a single-threaded event loop.
     * (Contrast: app->model is written by the worker, so reading it must happen
     * under app->mtx. app->rawlog is written by the worker via apply_unknown, so
     * reading it must happen under app->mtx too.) */
    SrScanCtlCtx scan;
    /* T4.10: snapshot of sr_worker_cmdack_count() taken **before** queuing a
     * start/stop, consumed by sr_wait_stage_eval.
     * GUI-thread exclusive like app->scan (see the comment block above). */
    uint32_t scan_cmdack_at_send;
    SrGpsSampleCtx gps_sample;
    SrPoiCtx poi;
    SrAlertCtx alert;
    bool probe_send_busy;
} SigRoamApp;

const char* sigroam_log_device_name(FuriHalRtcLogDevice d);
uint32_t sigroam_log_baud_value(FuriHalRtcLogBaudRate b);
bool sigroam_log_device_conflicts(void);
const char* sigroam_io_status_hint(SrIoStatus st); /* Returns a static literal for the UI to display */
const SrSourceCodec* sigroam_codec(const SigRoamApp* app);
void sigroam_dash_refresh(SigRoamApp* app); /* Copies a snapshot from model/io into the Dash ViewModel, under the lock */
