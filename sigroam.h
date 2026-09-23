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
#include "src/sr_peer_sync.h"
#include "src/sr_rawlog.h"
#include "src/sr_source_codec.h"
#include "src/sr_notify.h"
#include "views/sr_view_dash.h"

#define SR_TAG         "SigRoam"
#define SR_FAP_VERSION "0.5"
/* Display-only scanner product number on Probe (ADR-027). Not UART kVersion. */
#define SR_SCANNER_VERSION "0.5"

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
 * SR_BRAND_NAME is the maker word. About line 2 is SR_BRAND_LINE
 * ("by PINGEQUA Lab", 74 px). Changing the spelling must keep line 2 <= 91 px
 * (FontSecondary advance; that row shares y with the QR).
 */
#define SR_BRAND_NAME "PINGEQUA"
#define SR_BRAND_LINE "by " SR_BRAND_NAME " Lab"
#define SR_BRAND_URL  "go.pingequa.com/sr1g"

/* Flipper LCD is 128x64. Fullscreen attach has no status bar, so About
 * text-scroll uses the full canvas. (Plan/T2.4: 128 px wide.) */
#define SR_CANVAS_W 128
#define SR_CANVAS_H 64

/* About page QR geometry (T4.14; Y revised 2026-09-17 lockup A).
 * **Regenerate the image before changing SIDE; do not just edit the numbers**:
 *  - SIDE = 37 is the actual edge of assets/sr1g_qr.png (V3 = 29 modules + a
 *    4-module quiet zone each side, box_size=1). A URL past 42 bytes jumps to
 *    V4 (33 modules -> 41 px) and must be mirrored here.
 *  - X is right-aligned: 91 + 37 = 128. Rows that share the code's y range
 *    have 91 px on the left.
 *  - Y = 13 sits on the second-row cap (row 1 descender ends y=12) so row 1
 *    is a full-width title bar. The code occupies y=13..49; the URL cap at
 *    y=53 stays below it. Y outside 13..15 either clips row 1 or the URL.
 * The quiet zone is already inside the 37. Drawing into x>=91 and
 * y in [Y, Y+SIDE) makes the code unscannable. */
#define SR_ABOUT_QR_SIDE 37
#define SR_ABOUT_QR_X    (SR_CANVAS_W - SR_ABOUT_QR_SIDE)
#define SR_ABOUT_QR_Y    13

#define SR_ABOUT_TEXT_MAX 640
/* 320 until 2026-09-07. The four handshake lines already cost ~190 at their field
 * caps, and the Diag block (state name, stuck-gate name, six 10-digit heartbeats)
 * adds ~109 — 21 bytes of slack was close enough that a long ESP-IDF string would
 * have silently truncated the diagnostic, which is the one line that matters when
 * nothing else is answering. */
#define SR_PROBE_TEXT_MAX 448
#define SR_UPLOAD_TEXT_MAX 192
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
    char upload_text[SR_UPLOAD_TEXT_MAX];
    uint32_t upload_up_rev_shown;
    uint32_t upload_fw_rev_shown; /* model.firmware_rev last drawn on Upload */
    uint32_t upload_qual_rev_shown; /* model.qual_rev last drawn on Upload */
    bool upload_go_retry; /* GUI-thread: Center upload lost the depth-1 slot */
    bool upload_info_armed; /* one info after enter, once the command slot is free */
    bool upload_ident_info_sent; /* empty Version: one info, then stop until -sigroam- */
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
    /* Snapshot of model.busy_rev taken **before** queuing, same reason and same
     * before-send rule as scan_cmdack_at_send. Compared with != , never > : busy_rev
     * wraps. GUI-thread exclusive like app->scan. */
    uint32_t scan_busy_rev_at_send;
    SrGpsSampleCtx gps_sample;
    SrPoiCtx poi;
    SrAlertCtx alert;
    bool probe_send_busy;
    /* Generic Marauder SHOW_INFO clear. GUI-thread exclusive like probe_send_busy.
     * probe_stop_sent: Probe queued stopscan after Ok (do not queue a second).
     * dash_prestart: Dash OK asked for start and is waiting for wifi_stop_rev.
     * clear_stop_rev: wifi_stop_rev snapshot taken **before** that stopscan.
     * Compare with != . */
    bool probe_stop_sent;
    bool dash_prestart;
    uint32_t clear_stop_rev;
    uint32_t dash_prestart_tick_ms;
    /* Dash identity bootstrap while Version is empty. GUI-thread exclusive.
     * pending: hold START until Version arrives (timeout does not drop the
     * hold). info_sent: at least one `info` queued. sends: bootstrap+retry,
     * cap SR_SCAN_CTL_IDENT_MAX_SENDS. */
    bool dash_ident_pending;
    bool dash_ident_info_sent;
    uint8_t dash_ident_sends;
    uint32_t dash_ident_tick_ms;
    /* Stop-seal latch after stopscan ack on SigRoam. busy_rev_at_stop is
     * model.busy_rev at confirm; busy_rev != that value is a new Busy:.
     * post_stop_info: STOP ack queued `info` failed; retry until Diag 0/4. */
    bool stop_seal_latched;
    bool dash_post_stop_info;
    uint32_t busy_rev_at_stop;
    /* Card N1 peer-state adoption. GUI-thread exclusive like app->scan (see the
     * comment block above): armed by Dash on_enter after it queues `info`, consumed
     * by the Dash Tick handler when the reply raises model.firmware_rev.
     * peer_sync_fw_rev is a snapshot taken **before** the send, for the same reason as
     * scan_cmdack_at_send, and compared with != , never > : firmware_rev wraps.
     * Disarm policy lives in sr_peer_sync_on_tick: Wait keeps pending until this
     * #info's Diag: arrives. */
    uint32_t peer_sync_fw_rev;
    bool peer_sync_pending;
    /* Card N6 / n1n6-c1c3-fix. Armed when N1 adoption succeeds; consumed by
     * dash_sess_seed_tick once sess_rev != sess_seed_rev_at_send.
     * sess_seed_rev_at_send is snapshotted **before** the send, same family as
     * peer_sync_fw_rev. Independent of peer_sync_pending: firmware_rev can rise
     * before the Sess: line arrives. */
    uint32_t sess_seed_rev_at_send;
    bool sess_seed_pending;
} SigRoamApp;

const char* sigroam_log_device_name(FuriHalRtcLogDevice d);
uint32_t sigroam_log_baud_value(FuriHalRtcLogBaudRate b);
bool sigroam_log_device_conflicts(void);
const char* sigroam_io_status_hint(SrIoStatus st); /* Returns a static literal for the UI to display */
const SrSourceCodec* sigroam_codec(const SigRoamApp* app);
void sigroam_dash_refresh(SigRoamApp* app); /* Copies a snapshot from model/io into the Dash ViewModel, under the lock */
