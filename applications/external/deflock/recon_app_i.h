// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
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
#include "helpers/flock_db.h"
#include "helpers/flock_ble.h"
#include "helpers/watchscore.h"
#include "views/flock_view.h"
#include "views/flock_map_view.h"
#include "views/deflock_qr_view.h"
#include "views/guardian_view.h"
#include "views/ble_list_view.h"
#include "views/wifi_list_view.h"
#include "views/locator_view.h"

#define RECON_TAG "ReconSurvey"

#define RECON_FLOCK_MAX  64
#define RECON_WIFI_MAX   48
#define RECON_DEAUTH_MAX 16
#define RECON_BLE_MAX    48
#define RECON_TEXT_STORE 160
#define RECON_SSID_LEN   33

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
} BleCat;

#define RECON_APP_FOLDER    EXT_PATH("apps_data/flipdeflock")
#define RECON_REPORT_FOLDER RECON_APP_FOLDER "/reports"
#define RECON_SETTINGS_PATH RECON_APP_FOLDER "/settings.txt"

/** ViewDispatcher view indexes. */
typedef enum {
    ReconViewSubmenu,
    ReconViewVarItemList,
    ReconViewWidget,
    ReconViewPopup,
    ReconViewFlock,
    ReconViewFlockMap,
    ReconViewDeflockQr,
    ReconViewGuardian,
    ReconViewBleList,
    ReconViewWifiList,
    ReconViewLocator,
} ReconView;

/** ESP32 link backend / parsing strategy. */
typedef enum {
    EspBackendCompanion, /**< Our flock_companion firmware, strict line protocol. */
    EspBackendGeneric, /**< Marauder / any firmware: scrape MAC & SSID tokens from output. */
    EspBackendCount,
} EspBackend;

typedef struct {
    EspBackend backend;
    uint8_t esp_uart; /**< FuriHalSerialId for the ESP32. */
    uint8_t gps_uart; /**< FuriHalSerialId for the GPS module. */
    uint32_t esp_baud;
    uint32_t gps_baud;
    uint8_t marauder_cmd; /**< Generic backend: which Marauder sniff command to run. */
    bool gps_enabled;
    bool sound;
    bool flash_fast; /**< raise the flash (write) baud to 230400 after connect */
    bool log_serials; /**< log Flock device serials to saved reports (default OFF) */
    bool anomaly_flag; /**< Net Guardian: flag unidentified strong/persistent devices (default OFF, higher FP) */
} ReconSettings;

/** One deduplicated surveillance-device sighting. */
typedef struct {
    uint8_t mac[6];
    char ssid[RECON_SSID_LEN];
    int8_t rssi;
    uint8_t channel;
    char ftype; /**< P/B/R/O/F/L */
    FlockConfidence confidence;
    uint32_t ie_fp; /**< probe IE-skeleton fingerprint of this detection (0=none);
                      *   shown on the detail screen so it can be seeded into
                      *   signatures.json to catch MAC-randomized siblings. */
    float lat; /**< geotag of best sighting, NAN if none */
    float lon;
    float heading; /**< observer course-over-ground at sighting, NAN if none */
    int8_t geotag_rssi; /**< rssi when the geotag was last set (hysteresis) */
    uint32_t count;
    uint32_t first_tick;
    uint32_t last_tick;
    bool marked; /**< user flagged this for the report */
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
    bool dup; /**< SSID seen on >1 BSSID -> possible evil twin (or mesh) */
    bool rogue; /**< same SSID with mismatched security -> strong evil-twin signal */
    bool marked; /**< user-tagged for the report */
} WifiAp;

/** A BSSID observed being deauthenticated/disassociated (attack target). */
typedef struct {
    uint8_t bssid[6];
    uint8_t channel;
    uint32_t count;
    uint32_t last_tick;
} DeauthTarget;

/** A BLE device sighting (anti-tracker / BLE-Flock). */
#define RECON_BLE_SERIAL_LEN 24 /**< "TN72023022000771" is 16 chars; room to spare */

typedef struct {
    uint8_t addr[6];
    char name[RECON_SSID_LEN];
    int8_t rssi;
    uint8_t cat; /**< BleCat */
    uint16_t company; /**< BLE company id, 0xFFFF if none */
    char serial[RECON_BLE_SERIAL_LEN]; /**< Flock 0x09C8 device serial, "" if none */
    uint8_t model; /**< FlockBleModel: conservative Falcon/Raven guess */
    uint32_t count; /**< times seen across rescans */
    float first_lat; /**< GPS at first sighting (NAN if none) */
    float first_lon;
    float last_lat; /**< GPS at latest sighting */
    float last_lon;
    uint32_t first_tick; /**< tick at first sighting */
    uint32_t last_tick; /**< tick at latest sighting */
    uint8_t inrange_wp_count; /**< distinct observer waypoints (>=50 m apart) seen at */
    float last_wp_lat; /**< last counted waypoint (NAN if none) */
    float last_wp_lon;
    float max_span_m; /**< running max distance between counted waypoints */
    bool following; /**< multi-condition anti-stalking signal (latched) */
    bool marked; /**< user-tagged for the report */
} BleDevice;

typedef struct EspLink EspLink;
typedef struct GpsLink GpsLink;
typedef struct ReconNfc ReconNfc;
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
    FlockMapView* flock_map_view;
    DeflockQrView* deflock_qr_view;
    GuardianView* guardian_view;
    BleListView* ble_list_view;
    WifiListView* wifi_list_view;
    LocatorView* locator_view;

    ReconSettings settings;

    EspLink* esp;
    GpsLink* gps;
    ReconNfc* nfc;
    SigDb* sig_db; /**< SD-loaded extra signatures (NULL = built-ins only) */

    FuriMutex* mutex; /**< protects flock[] and gps_* snapshot */
    FlockEntry flock[RECON_FLOCK_MAX];
    size_t flock_count;
    int selected; /**< selected flock index for the detail scene */

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
    uint32_t esp_deauths; /**< deauth/disassoc frames seen (attack indicator) */

    // Active attack-tool signature reported by the companion (ATK line): BLE-spam
    // advert flood, beacon-spam (Marauder / Pineapple), or probe-request flood.
    uint32_t esp_attack_tick; /**< furi tick of the last ATK line (0 = none this session) */
    uint32_t esp_attack_value; /**< the count/rate the companion reported with it */
    char esp_attack_kind[16]; /**< short kind from the ATK line, e.g. "BLE-spam" */
    bool esp_attack_ble; /**< true if the signature is BLE-borne (BLE-spam) vs Wi-Fi */

    WifiAp wifi[RECON_WIFI_MAX]; /**< results of the last WiFi security scan */
    size_t wifi_count;
    bool wifi_scanning; /**< true between WBEGIN and WEND */
    bool wifi_done; /**< a scan has completed at least once */
    int wifi_selected; /**< selected AP index for the detail scene */
    uint8_t saved_backend; /**< backend to restore after the WiFi-audit scene */

    DeauthTarget deauth[RECON_DEAUTH_MAX]; /**< BSSIDs seen under deauth attack */
    size_t deauth_count;

    BleDevice ble[RECON_BLE_MAX]; /**< BLE devices / trackers */
    size_t ble_count;
    bool ble_scanning;
    bool ble_done;
    int ble_selected;

    WatchScore watch; /**< fused "am I being watched?" scorer (C1) */
    uint32_t guardian_since; /**< tick the Net Guardian session started (uptime) */
    uint8_t guardian_phase; /**< current rotating-sweep phase (0=flockcombo,1=ble,2=wifi) */
    uint8_t guardian_flip_n; /**< cached nearby-Flipper count for the HUD (set each
                              *   watchscore tick, so the view needn't re-lock). */
    uint8_t guardian_atk_n; /**< cached active-attack count for the HUD. */

    // Locator: hunt down one marked device by live signal strength (hot/cold).
    uint8_t locate_mac[6]; /**< target MAC/BSSID/BLE addr */
    uint8_t locate_kind; /**< 'w' Wi-Fi / 'b' BLE (selects the companion radio) */
    uint8_t locate_ch; /**< Wi-Fi channel to lock to (0 = hop / BLE) */
    char locate_label[28]; /**< human label for the target (SSID/name/type) */
    int8_t locate_rssi; /**< latest live RSSI from the companion LOC line */
    uint32_t locate_tick; /**< furi tick of that reading (0 = none yet) */
    bool locate_have; /**< a reading has arrived this session */

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
 */
void recon_app_report_flock(
    ReconApp* app,
    const uint8_t mac[6],
    const char* ssid,
    int8_t rssi,
    uint8_t channel,
    char ftype,
    FlockConfidence confidence,
    uint32_t ie_fp);

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
void recon_app_set_deauths(ReconApp* app, uint32_t deauths);

/** Record a deauth attack target BSSID (thread-safe); dedups by BSSID. */
void recon_app_add_deauth_target(ReconApp* app, const uint8_t bssid[6], uint8_t channel);

/** Record an active attack-tool signature from the companion ATK line (thread-safe). */
void recon_app_set_attack(ReconApp* app, const char* kind, uint32_t value);

// Net Guardian HUD tallies (nearby Flippers / active attacks) are cached in
// app->guardian_flip_n / guardian_atk_n by recon_app_watchscore_tick each tick,
// so the view reads them from its own snapshot without re-acquiring the mutex.

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
void recon_app_watchscore_tick(ReconApp* app);

void recon_settings_load(ReconApp* app);
void recon_settings_save(ReconApp* app);
