// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#pragma once

#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/modules/popup.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#include "scenes/recon_scene.h"
#include "helpers/alerts.h"
#include "helpers/detect_rules.h" // AlertConfChoice for the settings scene
#include "helpers/flock_db.h"
#include "helpers/flock_ble.h"
#include "views/flock_view.h"
#include "views/flock_detail_view.h"
#include "views/flock_map_view.h"
#include "views/deflock_qr_view.h"
#include "views/locator_view.h"

#define RECON_FLOCK_MAX  64
#define RECON_WIFI_MAX   48
#define RECON_BLE_MAX    48
#define RECON_TEXT_STORE 160
#define RECON_SSID_LEN   33
/** Shown on the main menu and About, so a bug report can name the build.
 *
 *  DEFINED BY THE BUILD, from FAP_VERSION in application.fam -- the same value
 *  stamped into the .fap. It was a hand-maintained literal here until v0.64,
 *  which meant two edits in two files with nothing checking they agreed; the
 *  failure mode is silent and shows a confidently wrong version to exactly the
 *  person trying to report a bug against a known build.
 *
 *  The fallback only appears if someone compiles this outside ufbt/fbt, and says
 *  so rather than inventing a number. */
#ifndef RECON_VERSION
#define RECON_VERSION "v?.??"
#endif
/** Most GPS-capable pins any supported part exposes (classic ESP32 has ~34). */
#define RECON_GPS_PIN_MAX 40

/** BLE device categories (companion firmware classifies these). */
typedef enum {
    BleCatUnknown = 0,
    BleCatFlock = 1, /**< Flock Safety / Raven (mfg 0x09C8) */
    BleCatAirTag = 2, /**< Apple Find My / AirTag */
    BleCatTile = 3,
    BleCatSmartTag = 4, /**< Samsung SmartTag */
    BleCatFindMyDevice =
        5, /**< Google Find My Device network (0xFEAA): Pebblebee/Chipolo/Moto/Eufy */
    BleCatFlipper = 6, /**< Flipper Zero (recon multitool): advertised name "Flipper <name>" */
    BleCatAxon = 7, /**< Axon body-worn / in-car police kit (SIG company id 0x034D) */
} BleCat;

#define RECON_APP_FOLDER    EXT_PATH("apps_data/flipdeflock")
#define RECON_REPORT_FOLDER RECON_APP_FOLDER "/reports"
#define RECON_SETTINGS_PATH RECON_APP_FOLDER "/settings.txt"
#define RECON_HITS_PATH     RECON_APP_FOLDER "/hits.csv"

/** ViewDispatcher view indexes. */
typedef enum {
    ReconViewSubmenu,
    ReconViewVarItemList,
    ReconViewWidget,
    ReconViewPopup,
    ReconViewFlock,
    ReconViewFlockDetail,
    ReconViewFlockMap,
    ReconViewDeflockQr,
    ReconViewLocator,
} ReconView;

/** ESP32 link backend / parsing strategy. */
typedef enum {
    EspBackendCompanion, /**< Our flock_companion firmware, strict line protocol. */
    EspBackendGeneric, /**< Marauder / any firmware: scrape MAC & SSID tokens from output. */
    EspBackendCount,
} EspBackend;

/** Queryable ESP-link lifecycle state, so scenes can tell "waiting for data" from
 *  "the UART is busy" (R6) instead of showing a dead "connecting ESP32..." forever. */
typedef enum {
    EspLinkStopped = 0, /**< not started (or torn down) */
    EspLinkRunning, /**< UART acquired + worker running (may not have data yet) */
    EspLinkPortBusy, /**< UART acquire failed -- another owner holds it (e.g. the GPS port) */
} EspLinkState;

/**
 * Where the position comes from.
 *
 * Plenty of ESP32 carrier boards put the GPS module on the ESP itself rather
 * than on the Flipper's header, so the Flipper's UART can never see it and no
 * pin setting helps (issue #5). For those, the companion firmware relays each
 * sentence over the link it already has.
 *
 * The first two are NMEA from a real receiver. The third is not: it is a phone's
 * own location, fetched over Unleashed's RPC location service (see gps_rpc.h).
 * ORDER IS LOAD-BEARING -- these values are persisted in settings.txt as
 * integers, so append only, never reorder.
 */
typedef enum {
    ReconGpsSourceFlipper = 0, /**< default: a GPS wired to the Flipper's own UART */
    ReconGpsSourceCompanion, /**< relayed by the companion as `G,<nmea>` */
    ReconGpsSourcePhone, /**< the paired phone's own fix, over the Unleashed RPC service */
    ReconGpsSourceCount,
} ReconGpsSource;

/**
 * What the phone GPS source has managed to do this session.
 *
 * The companion relay got this treatment in v0.54 after issue #5 showed that one
 * hollow "searching" badge standing in for four different faults costs four
 * rounds of back-and-forth to diagnose. The RPC source has strictly more ways to
 * fail than the relay does -- wrong firmware, no phone attached, permission
 * denied, location switched off, or a fix so coarse it is worthless -- so it gets
 * the same treatment up front rather than after the bug report.
 *
 * ReconGpsPhoneCoarse is the one with no NMEA equivalent: the phone IS answering,
 * with a fused cell/Wi-Fi estimate kilometres wide. A receiver with no lock says
 * so; a phone guesses. Reporting that as "searching" would be a lie the operator
 * cannot see through.
 */
typedef enum {
    ReconGpsPhoneOff = 0, /**< not selected, or never started */
    ReconGpsPhoneUnsupported, /**< this .fap was built without the location service */
    ReconGpsPhoneWaiting, /**< subscribed, nothing delivered yet */
    ReconGpsPhoneNoClient, /**< no RPC session: nothing is paired, or the app is closed */
    ReconGpsPhoneStreaming, /**< a usable fix has arrived */
    ReconGpsPhoneCoarse, /**< answering, but every fix is outside the accuracy gate */
    ReconGpsPhoneNoPermission, /**< the phone refused: location permission not granted */
    ReconGpsPhoneDisabled, /**< the phone's location services are switched off */
    ReconGpsPhoneNoFix, /**< the client cannot supply location at all (e.g. a desktop) */
    ReconGpsPhoneError, /**< the client reported an error it did not classify */
} ReconGpsPhoneState;

/**
 * What the companion has said about its GPS relay this session.
 *
 * The companion echoes `GPSCFG,<on>,<pin>,<baud>` for every `gps` command it
 * receives. Until v0.54 the app discarded that line, so a companion-sourced GPS
 * had exactly one visible state -- the hollow "searching" badge -- whether the
 * relay was running, whether it had refused the pin, or whether the firmware was
 * too old to have a relay at all. Issue #5 spent four rounds inside that blind
 * spot.
 */
typedef enum {
    ReconGpsRelayUnknown = 0, /**< configured, nothing echoed back yet */
    ReconGpsRelayOn, /**< GPSCFG,1: the companion accepted the pin and is relaying */
    ReconGpsRelayOff, /**< GPSCFG,0: it refused the pin, or the relay is off */
} ReconGpsRelayState;

/**
 * Which band(s) the companion's channel hopper sweeps.
 *
 * Only a dual-band part (ESP32-C5) can do anything but 2.4 GHz, and on a 2.4-only
 * radio the companion answers `2g` whatever it is asked. Index-aligned with
 * esp_band_cmd[] in helpers/esp_link.c.
 *
 * DEFAULT IS 2.4 GHz ON PURPOSE, including on a C5. The companion used to default
 * a C5 to "all", which is 41 channels instead of 13: at the same 300 ms dwell
 * that is ~12.3 s per sweep instead of ~3.9 s, so any given camera is revisited a
 * THIRD as often. A user parked beside three known Flock cameras and detected
 * none of them while the radio spent two thirds of its time on 5 GHz channels no
 * Flock signature we hold has ever been seen on (issue #5). Covering a band we
 * cannot yet confirm anything uses must not cost two thirds of the dwell on the
 * band everything we CAN detect actually lives on. 5 GHz stays available, opt-in.
 */
typedef enum {
    ReconEspBand24 = 0, /**< 13 channels, ~3.9 s sweep. The detection default. */
    ReconEspBand5, /**< 28 channels (C5 only) */
    ReconEspBandAll, /**< 41 channels; a third the revisit rate */
    ReconEspBandCount,
} ReconEspBand;

typedef struct {
    EspBackend backend;
    uint8_t esp_band; /**< ReconEspBand: which band(s) the companion sweeps */
    uint8_t esp_uart; /**< FuriHalSerialId for the ESP32. */
    uint8_t gps_uart; /**< FuriHalSerialId for the GPS module (Flipper source only). */
    uint32_t esp_baud;
    uint32_t gps_baud;
    uint8_t marauder_cmd; /**< Generic backend: which Marauder sniff command to run. */
    bool gps_enabled;
    uint8_t gps_source; /**< ReconGpsSource: Flipper UART or companion relay */
    uint8_t esp_gps_pin; /**< ESP-side GPS RX pin, for the companion relay. Board
                           *  specific -- there is no standard, so it is a setting
                           *  rather than a guess. */
    bool sound;
    uint8_t alert_mode; /**< ReconAlertMode: beep/vibro on a new Flock hit (default Vibrate) */
    uint8_t alert_min_conf; /**< AlertConfChoice: lowest rung that may alert (default Likely) */
    bool flash_fast; /**< raise the flash (write) baud to 230400 after connect */
    bool esp_auto_5v; /**< power the GPIO 5V rail if the companion never answers.
                        *  Default ON. See recon_app_esp_power_tick(). */
    bool save_hits; /**< persist detections to hits.csv across app restarts (default OFF:
                      *   it is a durable record of where you have been) */
    bool log_serials; /**< log Flock device serials to saved reports (default OFF) */
} ReconSettings;

/**
 * One deduplicated surveillance-device sighting.
 *
 * FIELD ORDER IS SIZE-DRIVEN, not thematic: bytes first, then 2-byte, then
 * 4-byte. 64 of these live inside the single ReconApp allocation, so each byte
 * of padding costs 64. The obvious thematic grouping left 7 bytes of holes
 * (88 bytes/entry); this ordering has none (80), which is 512 bytes of heap on
 * a device where a user on heavier firmware was already being refused with "Not
 * enough RAM to run the app" (issue #5). Nothing serializes this struct by
 * layout -- FlockStoreRec in flock_store.h is the separate on-disk POD, exactly
 * so reordering here is safe -- but keep new fields grouped by width.
 */
typedef struct {
    uint8_t mac[6];
    char ssid[RECON_SSID_LEN];
    int8_t rssi;
    uint8_t channel;
    char ftype; /**< P/B/R/O/F/L */
    FlockConfidence confidence;
    uint8_t dev_class; /**< FlockDevClass: ALPR camera vs SoundThinking acoustic
                         *   sensor. What it is, as opposed to how sure we are. */
    bool hidden; /**< beacons but withholds its SSID. An OBSERVATION shown to the
                   *   operator, never a confidence input -- see esp_parser.c. */
    int8_t geotag_rssi; /**< rssi when the geotag was last set (hysteresis) */
    bool marked; /**< user flagged this for the report */
    bool alerted; /**< the detection alert has already fired for this device (latch) */
    bool archived; /**< restored from hits.csv, not seen yet this session. first_tick/
                     *   last_tick are 0 and MEANINGLESS -- never age-test an archived
                     *   entry with tick arithmetic. */
    uint32_t ie_fp; /**< probe IE-skeleton fingerprint of this detection (0=none);
                      *   shown on the detail screen so it can be seeded into
                      *   signatures.json to catch MAC-randomized siblings. */
    float lat; /**< geotag of best sighting, NAN if none */
    float lon;
    float heading; /**< observer course-over-ground at sighting, NAN if none */
    uint32_t count;
    uint32_t first_tick;
    uint32_t last_tick;
    uint32_t seen_epoch; /**< RTC Unix seconds at the last sighting, 0 if never stored */
} FlockEntry;

/** One access point seen by the WiFi security scan (companion firmware). */
typedef struct {
    uint8_t bssid[6];
    char ssid[RECON_SSID_LEN];
    int8_t rssi;
    uint8_t channel;
    uint8_t authmode; /**< esp wifi_auth_mode_t */
    uint8_t pairwise; /**< esp wifi_cipher_type_t (pairwise) */
    bool wps;
    bool marked; /**< user-tagged for the report */
} WifiAp;

/** A BSSID observed being deauthenticated/disassociated (attack target). */
/** A BLE device sighting (BLE-sourced Flock detection). */
#define RECON_BLE_SERIAL_LEN 24 /**< "TN72023022000771" is 16 chars; room to spare */

/** Same size-driven field ordering as FlockEntry, for the same reason: 48 of
 *  these sit in the ReconApp allocation, and the thematic order left 9 bytes of
 *  holes (120 bytes/device vs 112). Group new fields by width. */
typedef struct {
    uint8_t addr[6];
    char name[RECON_SSID_LEN];
    char serial[RECON_BLE_SERIAL_LEN]; /**< Flock 0x09C8 device serial, "" if none */
    int8_t rssi;
    uint8_t cat; /**< BleCat */
    uint8_t model; /**< FlockBleModel: conservative Falcon/Raven guess */
    bool marked; /**< user-tagged for the report */
    uint16_t company; /**< BLE company id, 0xFFFF if none */
    uint32_t count; /**< times seen across rescans */
    float first_lat; /**< GPS at first sighting (NAN if none) */
    float first_lon;
    float last_lat; /**< GPS at latest sighting */
    float last_lon;
    uint32_t first_tick; /**< tick at first sighting */
    uint32_t last_tick; /**< tick at latest sighting */
} BleDevice;

typedef struct EspLink EspLink;
typedef struct GpsLink GpsLink;
typedef struct GpsRpc GpsRpc;
typedef struct SigDb SigDb;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Storage* storage;
    NotificationApp* notifications;

    Submenu* submenu;
    VariableItemList* var_item_list;
    Widget* widget;
    Popup* popup;
    FlockView* flock_view;
    FlockDetailView* flock_detail_view;
    FlockMapView* flock_map_view;
    DeflockQrView* deflock_qr_view;
    LocatorView* locator_view;

    ReconSettings settings;

    EspLink* esp;
    GpsLink* gps;
    GpsRpc* gps_rpc; /**< phone GPS over the Unleashed RPC service; NULL unless selected */
    SigDb* sig_db; /**< SD-loaded extra signatures (NULL = built-ins only) */

    FuriMutex* mutex; /**< protects flock[] and gps_* snapshot */
    FlockEntry flock[RECON_FLOCK_MAX];
    size_t flock_count;
    int selected; /**< selected flock index for the detail scene */

    // Detection alert (issue #1). The ESP worker only RAISES alert_pending under
    // the mutex; the GUI tick clears it and calls notification_message, mirroring
    // how the WATCHSCORE haptic is deferred off the worker thread.
    bool alert_pending; /**< a qualifying detection is waiting to be announced */
    uint32_t alert_last_tick; /**< tick of the last alert fired (any device) */
    bool alert_have_fired; /**< false until the session's first alert -> cooldown is inert */

    // "WHAT JUST BEEPED?" CARD (discussion #7). An alert says something was
    // found; it cannot say WHAT, and new rows land at the BOTTOM of a list that
    // is routinely 15 long, so the answer costs a scroll every time. Reported by
    // @h00die, who has the largest real list of anyone: "when a new entry beeps,
    // I need to scroll down to see what it is."
    //
    // Raised on exactly the same event as the beep -- not on every new row --
    // so the card and the sound always agree about what they are announcing.
    // Set on the ESP worker thread under the mutex, read by the GUI, same
    // discipline as alert_pending.
    uint8_t alert_card_mac[6]; /**< device the last alert was about */
    uint32_t alert_card_tick; /**< tick the card was raised; 0 = no card */

    // AUTO 5V for the companion board (discussion #7). The Flipper's GPIO 5V
    // rail is off at boot, so a board powered from the header is dead until the
    // user visits GPIO -> 5V by hand and then comes back. Reported by @h00die:
    // "when I start my flipper I have to go to gpio, and turn on 5v to get my
    // esp card going. Then launch deflock."
    //
    // WE OWN THE RAIL ONLY IF WE TURNED IT ON. otg_on_by_us is what makes the
    // teardown safe: leaving a boost converter running after the app exits would
    // flatten the battery of someone who never asked for it, and turning off a
    // rail the USER switched on for their own reasons would be equally wrong.
    bool otg_on_by_us; /**< we enabled 5V, so we must disable it on exit */
    bool otg_attempted; /**< one attempt per app run, success or not */
    uint32_t esp_link_wait_tick; /**< when we started waiting for the companion */
    bool otg_failed; /**< enable was refused or faulted -- surfaced, not retried */

    // Companion GPS-relay health (issue #5). Only meaningful when the GPS source
    // is the companion; the Flipper-UART path has its own busy/conflict test.
    // What the companion reported about itself (CHIP/BAND). Zeroed = not heard
    // yet, in which case the app must not claim to know the board's pinout.
    char esp_chip[12]; /**< IDF target name, "" until a CHIP line arrives */
    uint8_t esp_gpio_count;
    uint64_t esp_gps_pin_mask; /**< bit N = GPIO N can carry a GPS on THIS chip */
    bool esp_has_5ghz;
    uint8_t esp_band_actual; /**< ReconEspBand the board says is in force */
    // GPS pin picker, rebuilt from esp_gps_pin_mask each time Settings opens.
    uint8_t gps_pin_vals[RECON_GPS_PIN_MAX];
    char gps_pin_label[4]; /**< text for the CURRENT pin only. Storing all 40
                             *   cost 160 bytes of a single contiguous
                             *   allocation the loader already struggles to
                             *   place on heavier firmware. */
    uint8_t gps_pin_count;
    uint16_t esp_band_channels; /**< channels the current sweep covers */
    uint8_t gps_relay; /**< ReconGpsRelayState */
    int16_t gps_relay_pin; /**< the pin the companion reported, -1 if none */
    uint32_t gps_relay_baud; /**< the baud it reported */
    uint32_t gps_cfg_tick; /**< tick the config was last sent; 0 = never this session */
    bool gps_cfg_resend; /**< worker saw a banner -> GUI tick must re-send the config.
                           *  A flag, not a direct send: furi_hal_serial_tx from the
                           *  ESP worker would race the GUI thread's own commands on
                           *  the same handle. Same discipline as alert_pending. */

    uint8_t gps_phone; /**< ReconGpsPhoneState. Written by the RPC callback under
                         *   the mutex, read by the UI -- same discipline as the
                         *   gps_* fix snapshot below. */

    bool gps_valid;
    float gps_lat;
    float gps_lon;
    float gps_course; /**< course over ground (deg), NAN if unknown */
    int gps_sats;

    bool esp_connected;
    uint32_t esp_frames; /**< 802.11 frames this *session* (companion total minus base) */
    uint32_t esp_hits; /**< Flock hits this session (companion total minus base) */
    // The companion's frame/hit counters are lifetime totals (reset only on ESP
    // reboot). We rebase them per scan session so the on-screen count starts at 0
    // each time you open a scan screen instead of climbing forever.
    uint32_t esp_frames_base;
    uint32_t esp_hits_base;
    bool esp_rebase; /**< next status line captures the per-session base */
    uint8_t esp_channel;
    uint32_t esp_lines; /**< RX line heartbeat (generic mode liveness) */
    // Live activity, not lifetime totals. A number that only grows tells you the
    // link is up but not whether the radio is hearing anything RIGHT NOW, which
    // is the question you have while parked next to a camera (issue #5).
    int32_t esp_frame_rate; /**< frames/s from the last two status lines, -1 = unknown */
    uint32_t esp_frames_prev; /**< lifetime total at the last rate sample */
    uint32_t esp_rate_tick; /**< tick of that sample */
    uint32_t esp_ble_scans; /**< BLE scan phases COMPLETED (BEND). Distinguishes
                             *   "BLE ran and saw nothing" from "BLE never ran",
                             *   which a bare count of 0 cannot. */
    bool warn_dismissed; /**< the operator has read the fault panel this session */
    bool gps_fault_active; /**< a GPS fault is currently showing, so OK means
                             *   "dismiss" rather than "open detail" */
    uint32_t alert_fired; /**< alerts actually delivered this session. Shown so an
                            *   operator can tell the app not firing from the
                            *   Flipper's own notification settings swallowing it --
                            *   reported three times as "no beep/vibrate" with no way
                            *   to see which half was at fault (issue #5). */
    uint32_t esp_ble_seen; /**< BLE adverts received this session. The Flock screen
                             *  showed NOTHING about BLE, so in flockcombo mode there
                             *  was no way to tell a working BLE half from one that
                             *  never ran -- and BLE is usually the easy detection. */
    uint32_t esp_reboots; /**< times the companion's lifetime counter fell, i.e. the
                            *  board restarted mid-session. Silently absorbed before
                            *  v0.56: a user reported the count "ticking back to 0" on
                            *  long drives as a cosmetic annoyance, when it was the ESP
                            *  resetting and dropping detections. */
    uint8_t esp_proto_version; /**< companion wire-protocol version (FLOCKCO banner; 0 = unknown) */
    bool esp_proto_mismatch; /**< companion speaks a different protocol version than the app */
    uint32_t esp_dropped_lines; /**< overlong RX lines dropped whole (wire-protocol health metric) */
    uint8_t esp_link_state; /**< EspLinkState: Stopped / Running / PortBusy (R6 error surface) */

    /* The WiFi Audit SCREEN was removed, but this table stays: the Locator
     * builds its target list from it, so a marked camera can be hunted by
     * BSSID after a sweep. */
    WifiAp wifi[RECON_WIFI_MAX]; /**< results of the last WiFi sweep */
    size_t wifi_count;
    bool wifi_scanning; /**< true between WBEGIN and WEND */
    bool wifi_done; /**< a scan has completed at least once */
    uint8_t saved_backend; /**< backend to restore after the WiFi-audit scene */

    BleDevice ble[RECON_BLE_MAX]; /**< BLE devices / trackers */
    size_t ble_count;
    bool ble_scanning;
    bool ble_done;
    int ble_selected;

    uint32_t guardian_since; /**< tick the Net Guardian session started (uptime) */
    uint8_t guardian_phase; /**< current rotating-sweep phase (0=flockcombo,1=ble,2=wifi) */
    // Scan-scene UI state, moved out of per-scene file-scope statics (R4-tail) so
    // the scene layer holds no module-global mutable state. Semantics are
    // identical (ReconApp is single-instance and app-lifetime, like the statics).
    uint32_t guardian_phase_mark; /**< guardian: tick of the last rotating-sweep phase switch */
    bool guardian_blocked; /**< guardian: opened in Marauder mode -> guard screen shown */
    int wifi_ui_state; /**< wifi: 0 scanning / 1 results / 2 timeout */
    uint32_t wifi_scan_start; /**< wifi: tick the current scan started */
    bool wifi_blocked; /**< wifi: opened in Marauder mode -> guard screen */
    bool ble_pending; /**< ble: a blescan is in flight (awaiting BEND) */
    uint32_t ble_mark; /**< ble: tick of the last state transition */
    bool ble_blocked; /**< ble: opened in Marauder mode -> guard screen */
    bool locator_blocked; /**< locator: opened in Marauder mode -> guard screen */

    // Locator: hunt down one marked device by live signal strength (hot/cold).
    uint8_t locate_mac[6]; /**< target MAC/BSSID/BLE addr */
    uint8_t locate_kind; /**< 'w' Wi-Fi / 'b' BLE (selects the companion radio) */
    uint8_t locate_ch; /**< Wi-Fi channel to lock to (0 = hop / BLE) */
    char locate_label[28]; /**< human label for the target (SSID/name/type) */
    int8_t locate_rssi; /**< latest live RSSI from the companion LOC line */
    uint32_t locate_tick; /**< furi tick of that reading (0 = none yet) */
    bool locate_have; /**< a reading has arrived this session */
    int8_t locate_peak; /**< strongest RSSI folded from every LOC line (peak-hold) */
    float locate_ema; /**< smoothed RSSI for the warmer/colder trend */
    int8_t locate_trend; /**< +1 warmer / -1 colder / 0 steady */
    bool locate_init; /**< first valid reading folded yet */

    // ESP32 firmware flasher
    uint8_t fw_op; /**< 0 = backup, 1 = flash */
    char fw_path[256]; /**< bin to flash, or backup output path */
    FuriString* fw_log; /**< streaming flasher log (shown in the run scene) */
    FuriThread* fw_thread;
    volatile bool fw_running;
    volatile bool fw_ok;
    volatile bool fw_log_dirty; /**< log changed -> re-render */

    char text_store[RECON_TEXT_STORE];
} ReconApp;

/**
 * Record/merge a Flock detection. Thread-safe (takes app->mutex internally).
 * Called from the ESP worker thread; geotags with the latest GPS fix.
 *
 * `dev_class` is what the device IS (ALPR camera vs acoustic sensor), separate
 * from `confidence`, which is how sure we are. FlockClassAlpr is the default;
 * only a positive acoustic identification overwrites a stored class.
 */
void recon_app_report_flock(
    ReconApp* app,
    const uint8_t mac[6],
    const char* ssid,
    int8_t rssi,
    uint8_t channel,
    char ftype,
    FlockConfidence confidence,
    uint32_t ie_fp,
    FlockDevClass dev_class,
    bool hidden);

/** Update the cached ESP status line (thread-safe). */
void recon_app_set_esp_status(
    ReconApp* app,
    uint32_t frames,
    uint32_t hits,
    uint8_t channel,
    bool connected);

/** Update the RX line heartbeat counter (thread-safe). Marks ESP connected. */
void recon_app_set_esp_lines(ReconApp* app, uint32_t lines);

/** Update the deauth/disassoc frame counter (thread-safe). */

/** Record the companion's announced wire-protocol version + whether it mismatches
 *  what this app speaks (thread-safe). See ESP_PROTO_VERSION in esp_parser.h. */
void recon_app_set_esp_proto(ReconApp* app, uint8_t version, bool mismatch);

/** Update the count of overlong RX lines dropped whole (health metric; thread-safe). */
void recon_app_set_esp_dropped(ReconApp* app, uint32_t dropped);

/** Update the queryable ESP-link state (thread-safe). See EspLinkState. */
void recon_app_set_esp_link_state(ReconApp* app, EspLinkState state);

/** Record a `GPSCFG` echo from the companion (issue #5 diagnosability). */
void recon_app_set_gps_relay(ReconApp* app, bool on, int16_t pin, uint32_t baud);

/** Record a `CHIP` report: what the companion physically is. */
void recon_app_set_chip(
    ReconApp* app,
    const char* target,
    uint8_t gpio_count,
    uint64_t gps_pin_mask,
    bool has_5ghz);

/** Record a `BAND` echo: the sweep actually in force. */
void recon_app_set_band(ReconApp* app, uint8_t sel, uint16_t channels);

/** Rebuild the GPS pin choices from what the board reported (or the safe
 *  fallback if it has not reported yet). */
void recon_settings_build_gps_pins(ReconApp* app);

/** Mark the relay config as sent and awaiting its echo; starts the ack clock. */
void recon_app_gps_relay_pending(ReconApp* app);

/** Worker-side: ask the GUI thread to re-send the relay config (banner seen). */
void recon_app_request_gps_cfg(ReconApp* app);

/** GUI-tick side: send the relay config if the worker asked for it. */
void recon_app_gps_cfg_tick(ReconApp* app);

/**
 * Opt-in "anomaly": an unnamed, unidentified (no mfg id / no recognized category),
 * strong, repeatedly-seen BLE device -- the closest passive proxy for "an unknown
 * device is sitting right on you." Shared by the scorer and the Guardian sus-list
 * so they agree. `now` is furi_get_tick(); caller holds app->mutex.
 */
bool recon_ble_is_anomaly(const BleDevice* e, uint32_t now);

/** Store the latest Locator target RSSI from a companion LOC line (thread-safe). */
void recon_app_set_locate_rssi(ReconApp* app, int8_t rssi);

/** BLE scan results (thread-safe; called from the ESP worker). */
void recon_app_ble_begin(ReconApp* app);

/** A BLE scan phase finished (BEND). Counted so "BLE never ran" and "BLE ran
 *  and saw nothing" stop looking identical on the header. */
void recon_app_ble_scan_done(ReconApp* app);
void recon_app_ble_add(
    ReconApp* app,
    const uint8_t addr[6],
    const char* name,
    int8_t rssi,
    uint8_t cat,
    uint16_t company,
    const uint8_t* mfg, /**< raw mfg-data bytes (Flock 0x09C8), NULL if none */
    size_t mfg_len,
    bool raven_gatt); /**< companion saw Raven-specific GATT services (0x3100-0x3500) */
void recon_app_ble_end(ReconApp* app);

/** WiFi security scan results (thread-safe; called from the ESP worker). */
void recon_app_wifi_begin(ReconApp* app);
void recon_app_wifi_add(
    ReconApp* app,
    const uint8_t bssid[6],
    const char* ssid,
    int8_t rssi,
    uint8_t channel,
    uint8_t authmode,
    uint8_t pairwise,
    bool wps);
void recon_app_wifi_end(ReconApp* app);

/**
 * Recompute the fused WATCHSCORE (C1). Snapshots the shared signal arrays under
 * app->mutex, evaluates the scorer after release, and fires exactly one
 * notification on the transition INTO ELEVATED. Safe to call from the GUI tick.
 */

/**
 * Announce any pending detection alert (issue #1). Reads and clears
 * app->alert_pending under the mutex, then fires the configured beep/vibro
 * OUTSIDE the lock. Must be called from the GUI thread -- every scan scene's
 * tick branch does. Cheap and safe to call when nothing is pending.
 */
void recon_app_alert_tick(ReconApp* app);

void recon_settings_load(ReconApp* app);
void recon_settings_save(ReconApp* app);

/**
 * Persisted detections (issue #2), gated on settings.save_hits.
 *
 * Save is called from scan_session_stop(), i.e. every scan scene's on_exit, so
 * hits survive backing out of the app. Load runs once at startup and marks every
 * restored entry `archived`. Clear removes the file AND the archived entries, so
 * turning the setting off actually erases the trail rather than just hiding it.
 */
void recon_hits_load(ReconApp* app);

/**
 * Power the GPIO 5V rail if the companion has not answered, once per app run.
 *
 * DETECT FIRST, THEN POWER -- never power unconditionally. A board that is
 * already alive is a board powered some other way (its own USB, most often
 * while it is being flashed), and energising the header rail underneath it
 * would be feeding a second supply into hardware that did not ask for one. So
 * this waits out a grace period and acts only on silence, which is also exactly
 * what was requested: "it would see that the card isn't there and attempt to
 * power it on before scanning".
 *
 * Runs from the dispatcher tick, so it covers every scene without any scene
 * having to remember it -- the same reasoning that hoisted the alert tick there,
 * but it only ACTS while a scan session holds the UART open. On the main menu
 * nothing is listening, so silence there says nothing about power.
 */
void recon_app_esp_power_tick(ReconApp* app);

/** Drop the 5V rail IF this app raised it. Safe to call more than once. */
void recon_app_esp_power_release(ReconApp* app);
void recon_hits_save(ReconApp* app);
void recon_hits_clear(ReconApp* app);

/**
 * Persist after the operator DELETED an entry. Writes the table, or removes
 * `hits.csv` entirely when they deleted the last one.
 *
 * Never call this for an incidentally empty table. recon_hits_save() runs on
 * every scan-session exit, and folding this removal into it turned Net
 * Guardian's baseline reset into permanent data loss (issue #5).
 */
void recon_hits_save_after_delete(ReconApp* app);
