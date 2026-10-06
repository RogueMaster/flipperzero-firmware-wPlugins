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
#include <gui/modules/text_input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#include "scenes/recon_scene.h"
#include "helpers/alerts.h"
#include "helpers/detect_rules.h" // AlertConfChoice for the settings scene
#include "helpers/flock_db.h"
#include "helpers/flock_store.h"
#include "helpers/flock_ble.h"
#include "helpers/sig_db.h" // SIG_IGNORED_MAX_* for the in-memory exclusion lists
#include "views/flock_view.h"
#include "views/flock_detail_view.h"
#include "views/flock_map_view.h"
#include "views/deflock_qr_view.h"
#include "views/locator_view.h"

#define RECON_FLOCK_MAX  64
// 8 EACH, DOWN FROM 48, AND THE REASON IS RAM. Both tables are written by the
// link and read by exactly one thing, the Locator picker, which lists only rows
// with `marked` set -- and the Wi-Fi and BLE list screens that set it are gone,
// so today nothing does. At 48 rows they cost 6.9 KB of the single tables block
// (19.3 KB), which is more than the largest free block this app has left on a
// loaded firmware (16.9 KB measured at the main menu, 24 KB free in total), and
// the flasher screen has to free and re-allocate that block. When it could not
// be placed again the firmware halted on out-of-memory and rebooted the
// Flipper, seen on leaving the flasher after a successful flash. 8 keeps the
// code paths alive for when a picker returns; raise it only with a measurement.
#define RECON_WIFI_MAX   8
#define RECON_BLE_MAX    8
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
// Written in full first, then swapped in over hits.csv -- see recon_hits_save().
#define RECON_HITS_TMP_PATH RECON_APP_FOLDER "/hits.tmp"
// One appended row per scan session. Exists because a drive that finds nothing
// is INDISTINGUISHABLE from a companion that never scanned, an app that rejected
// everything, and a road with no cameras on it -- all four render as an empty
// list, and every counter that could tell them apart was live-only and died with
// the session. v0.79-v0.83 shipped without this and cost two operators a drive.
#define RECON_DIAG_PATH     RECON_APP_FOLDER "/diag.csv"
// Bump this string whenever the diag COLUMNS change. It is compared against the
// first line of an existing diag.csv, and a mismatch rotates the old file to
// diag.old.csv rather than appending rows of a new shape under an old header --
// which is what produced a file nobody could parse correctly. See recon_diag_save().
#define RECON_DIAG_HEADER_LINE \
    "# FlipDeFlock session diagnostics v4 -- counts only, no MAC/SSID/position\n"
#define RECON_DIAG_OLD_PATH       RECON_APP_FOLDER "/diag.old.csv"
// Every wildcard-probe transmitter seen during a session, MATCHED OR NOT.
// Exists because "83,916 frames, zero candidates" is the one result the detector
// cannot explain: a camera on an OUI we do not carry, or one using a randomised
// MAC, looks exactly like an empty street. Park next to a camera you can see and
// the row with a huge count is it, whatever its OUI turns out to be.
#define RECON_SURVEY_PATH         RECON_APP_FOLDER "/survey.csv"
// The same rows, APPENDED across sessions instead of replacing them.
//
// survey.csv is deliberately a fresh snapshot of the last scan and must stay
// that way: counts are per session, and a row from a different street would be
// actively misleading while hunting one camera. But that also means a stop is
// destroyed the moment the next scan ends, and a field report of several stops
// survived only because the operator happened to copy the file to their phone
// between them. Data that took a drive to collect should not depend on that.
//
// Each row carries the session it came from, so stops stay separable while a
// whole drive is still one file to send. An empty session adds nothing here on
// purpose -- diag.csv is the per-session record of "it ran", and a row saying
// only that would duplicate it.
#define RECON_SURVEY_LOG_PATH     RECON_APP_FOLDER "/survey_log.csv"
// Rotated to survey_log.old.csv past this, so the log is bounded at roughly
// twice it and the most recent drives always survive. ~2800 rows, i.e. dozens of
// drives; a session contributes at most RECON_SURVEY_MAX.
#define RECON_SURVEY_LOG_MAX      131072u
// Printable IE signature carried per survey row. A real camera's full tag list
// runs to about 51 characters, so 72 holds it with room for a couple more
// elements; longer ones truncate rather than drop, because even a cut signature
// shows its leading tag order. Must be >= the companion's SURVEY_SIG_LEN or the
// CSV loses the tail of what the board already measured. 48 rows x 72 B =
// 3.4 KB, which is real money on this heap and is why it lives in the survey
// table only and not in FlockEntry.
#define RECON_SURVEY_SIG_LEN      72
// How often survey.csv is re-snapshotted while a scan runs. 20 s bounds the loss
// from a battery dying or a device wedging mid-stop to the last 20 seconds of
// probing, against a file of a few KB. Detections already had this
// (RECON_HITS_AUTOSAVE_MS); the survey did not, which is the asymmetry this
// closes -- and the survey is the half that explains a stop.
#define RECON_SURVEY_AUTOSAVE_MS  20000u
#define RECON_SURVEY_LOG_OLD_PATH RECON_APP_FOLDER "/survey_log.old.csv"

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
    ReconViewTextInput,
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

/**
 * One wildcard-probe transmitter observed, WHETHER OR NOT it matched a table.
 *
 * The detector only ever reports what it already recognises, so "83,916 frames,
 * zero candidates" -- the result two operators independently got next to real
 * cameras -- is indistinguishable from an empty street. This records what was
 * actually in the air, so a camera on an OUI we do not carry, or one using a
 * randomised MAC, becomes visible instead of silently absent.
 *
 * `fp` is the IE-skeleton hash: MAC-independent, so it survives randomisation and
 * is the thing that can populate flock_ie_fps[], which ships empty today.
 * `count` is the discriminator -- a camera probes every ~125 ms forever, a phone
 * emits a burst and goes quiet.
 */
typedef struct {
    uint8_t mac[6];
    uint32_t fp;
    /**
     * IE-CONTENT hash -- the fix for why `fp` above could not identify anything.
     * `fp` folds in each IE's tag and length and discards the bytes, so the
     * capability elements that describe a radio count for nothing: across 120
     * devices in a real capture it produced 49 distinct values with 74% of
     * devices colliding, one hash covering 24 separate devices. 0 from firmware
     * older than v0.96.
     */
    uint32_t fp2;
    int8_t rssi; /**< strongest seen -- closest approach */
    uint8_t channel;
    uint16_t count;
    /**
     * Printable IE signature: an ordered IE tag list with vendor elements
     * expanded, readable rather than hashed.
     *
     * A HASH CANNOT BE READ. It cannot be eyeballed against someone else's
     * capture, partially matched, or published in a form another project can
     * use -- which is exactly what was needed and missing when a field report
     * arrived carrying nothing but eight hex digits per row. This goes in
     * survey.csv beside the hashes, not instead of them.
     */
    char sig[RECON_SURVEY_SIG_LEN];
} SurveyEntry;
#define RECON_SURVEY_MAX 48

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
    uint8_t alert_mode; /**< ReconAlertMode: beep/vibro on a new Flock hit (default Beep+Vibe) */
    uint8_t alert_min_conf; /**< AlertConfChoice: lowest rung that may alert (default Likely) */
    bool flash_fast; /**< raise the flash (write) baud to 230400 after connect */
    bool esp_auto_5v; /**< power the GPIO 5V rail if the companion never answers.
                        *  Default ON. See recon_app_esp_power_tick(). */
    bool save_hits; /**< persist detections to hits.csv across app restarts (default ON;
                      *   turning it off deletes the file) */
    bool card_autodismiss; /**< hit card clears itself after CARD_MS (default ON). Off =
                             *  it stays until the NEXT hit replaces it. */
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
    char ftype; /**< P/B/R/O/F/L/S */
    FlockConfidence confidence;
    uint8_t dev_class; /**< FlockDevClass: ALPR camera vs SoundThinking acoustic
                         *   sensor. What it is, as opposed to how sure we are. */
    bool hidden; /**< beacons but withholds its SSID. An OBSERVATION shown to the
                   *   operator, never a confidence input -- see esp_parser.c. */
    uint8_t probe_rate; /**< wildcard probes this transmitter sent inside the
                          *  companion's ~8 s window, at the strongest sighting.
                          *
                          *  THE ONE MEASUREMENT THAT SEPARATES A POLE FROM A
                          *  HANDHELD. A mains-powered camera phones home every
                          *  ~125 ms forever; a battery radio cannot, and when it
                          *  does use WiFi it looks for a KNOWN network, which is
                          *  a directed probe rather than a wildcard one. That
                          *  matters most on a prefix covering both, which is
                          *  exactly what Motorola Solutions is.
                          *
                          *  It rode the wire as `pr=` from v0.88 and was parsed
                          *  into the message struct and then DROPPED -- never
                          *  stored, never shown, never scored. Kept here so the
                          *  detail screen can show it, and it is what the
                          *  companion's own VENDOR_PROBE_SUSTAINED rung acts on.
                          *  Max-held with the RSSI, same rule as `channel`: the
                          *  closest sighting is the one worth keeping. */
    uint8_t ble_tell; /**< FlockBleTell: WHICH BLE signal classified this (mfg id
                        *   vs Raven GATT vs naming vs a shared OUI). Display only
                        *   -- never a confidence input. LIVE-SESSION ONLY: it is
                        *   not in the hits.csv schema, so a row restored from the
                        *   card reads back as FlockBleTellNone and the detail
                        *   screen falls back to the generic "BLE". */
    int8_t geotag_rssi; /**< rssi when the geotag was last set (hysteresis) */
    int8_t chan_rssi; /**< rssi of the sighting that set `channel`.
                        *
                        *  THE CHANNEL HAS TO COME FROM THE CLOSEST APPROACH.
                        *  2.4 GHz channels are 20 MHz wide on 5 MHz spacing, so
                        *  a camera transmitting only on 6 is genuinely received
                        *  on 2 and 10 as well -- measured at 30 cm on the bench,
                        *  a beacon-only emitter pinned to 6 was heard on
                        *  2/5/6/7/8/10/12, peaking at -20 on 6 and down at -57
                        *  on 2 and 10. This field used to not exist and
                        *  `channel` was last-write-wins, so whichever fringe
                        *  capture arrived last became the stored channel.
                        *
                        *  That is not cosmetic: `locate` parks the companion's
                        *  radio on this channel, so a fringe value sends the
                        *  Locator somewhere the camera never transmits. Seen on
                        *  the bench -- a target stored as channel 12 read -68
                        *  dBm, the same target on 6 read -24. */
    bool marked; /**< user flagged this for the report */
    bool confirmed; /**< the operator SAW this device with their own eyes. Ground
                      *   truth, and the only thing in the table that is not an
                      *   inference -- it is what promotes a candidate fingerprint. */
    char label[FLOCK_STORE_LABEL_LEN]; /**< the operator's own name for it. Kept
                                         *   SEPARATE from ssid on purpose: what was
                                         *   observed on the air and what the operator
                                         *   calls it are different facts. */
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

    /* ---- ASTM F3411 Remote ID (FlockClassDrone only) --------------------- */
    /**
     * The OPERATOR's position, straight out of the aircraft's own System
     * message. NAN until one arrives.
     *
     * The single most actionable field in the app. Everything else FlipDeFlock
     * finds is fixed infrastructure you can walk away from; a drone follows you,
     * and this says where the person flying it is standing. It is broadcast in
     * the clear because federal law requires it.
     */
    float op_lat, op_lon;
    uint8_t ua_type; /**< OdidUaType -- multirotor, fixed wing, ... */
    /**
     * lat/lon came from the aircraft's OWN Remote ID broadcast rather than from
     * our geotag of where we were standing when we heard it.
     *
     * These are not the same claim and must not be shown as one. A geotag says
     * "the observer was here"; a Remote ID position says "the aircraft was
     * there", to GPS accuracy, possibly hundreds of metres away and a few hundred
     * feet up. Merging them silently would put a marker on the map that means
     * whichever one happened to arrive last.
     */
    bool pos_broadcast;
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
    TextInput* text_input;
    FlockView* flock_view;
    FlockDetailView* flock_detail_view;
    FlockMapView* flock_map_view;
    DeflockQrView* deflock_qr_view;
    LocatorView* locator_view;

    ReconSettings settings;

    /* hits.csv autosave. Detections used to reach the card only via
     * scan_session_stop(), so a battery death mid-scan lost everything collected
     * since the scan began -- reported from a real drive. The worker sets
     * hits_dirty on every new/updated detection; the GUI tick flushes on an
     * interval. Not a setting: there is no reason to want crash loss. */
    /* Hit action menu (hold OK on a row). Table index of the device the menu and
     * the rename screen act on, captured when the menu opens so a detection
     * landing mid-edit cannot redirect it at a different camera. */
    int hit_menu_idx;
    char rename_buf[FLOCK_STORE_LABEL_LEN];

    /* Air-survey screen. Index into app->survey[] of the row its detail screen
     * acts on, captured when that screen opens for the same reason as
     * hit_menu_idx: the survey re-ranks every poll, and a row arriving mid-read
     * must not slide a different transmitter under the operator's decision. */
    int survey_menu_idx;

    bool hits_dirty;
    uint32_t hits_last_save; /**< furi tick of the last successful flush */

    EspLink* esp;
    GpsLink* gps;
    GpsRpc* gps_rpc; /**< phone GPS over the Unleashed RPC service; NULL unless selected */
    SigDb* sig_db; /**< SD-loaded extra signatures (NULL = built-ins only) */

    /* ---- operator exclusions (helpers/sig_db.h, ignored.txt) ------------
     * Devices the operator told the app are THEIRS. Held in memory for the
     * whole session because the gate runs on every companion detection line,
     * and refreshed in place the moment one is added -- unlike a learned
     * signature, which only takes effect next start.
     *
     * THAT ASYMMETRY IS DELIBERATE. Learning changes how a device is SCORED,
     * and rebuilding the scoring tables under a live scan is the thing that
     * restriction avoids. An exclusion only has to drop a line. The operator
     * presses it because something is beeping at them right now, so "it will
     * go quiet after you restart the app" is not an answer. */
    uint32_t ignore_fps[SIG_IGNORED_MAX_FPS];
    uint8_t ignore_macs[SIG_IGNORED_MAX_MACS][6];
    size_t ignore_fp_count;
    size_t ignore_mac_count;

    FuriMutex* mutex; /**< protects flock[] and gps_* snapshot */
    /* HEAP, NOT INLINE, so the ESP flasher can have the memory back.
     *
     * These four tables are the app's bulk. Inline in ReconApp they were locked
     * up for the whole run, and with the app resident the largest contiguous
     * block left was ~25 KB while the flasher plugin needs ~23 KB in one piece
     * -- a firmware backup ran the device out of memory and crashed it. The
     * ESP32 Firmware screen holds no scan, so it releases these first (see
     * recon_tables_release/acquire) and the plugin gets a clean block.
     *
     * Indexing is unchanged: app->flock[i] reads identically for a pointer. */
    /* ONE ALLOCATION, carved into the four pointers below.
     *
     * Freeing four separate blocks and re-allocating four left the heap more
     * fragmented on every firmware-screen visit: measured on the bench, the
     * largest contiguous block fell 32,448 -> 25,776 in a single cycle and did
     * not recover, and a long session reached 13,816 -- below what the file
     * browser needs, so "Flash a .bin" silently did nothing and the app looked
     * wedged. One block frees one clean hole for the plugin and takes the same
     * hole back afterwards. */
    void* tables_block;
    bool tables_split; /**< the one block did not fit, so each table is its own
                         *   allocation and must be freed as such. See
                         *   recon_tables_acquire(). */
    FlockEntry* flock;
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
    /* NOTE: a BLE-less companion (see recon_esp_chip_has_no_ble) is reported
     * here and nowhere else, which is why the header keys off this field. */
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
    uint32_t gps_fix_tick; /**< tick of the last VALID fix; the GUI tick clears
                             *   gps_valid once this is RECON_GPS_STALE_MS old,
                             *   because a receiver that goes silent never sends
                             *   the "lock lost" sentence that used to be the
                             *   only way gps_valid could fall. */

    bool esp_connected;
    bool esp_lost; /**< the link WAS up this session and then went silent for
                     *   RECON_ESP_SILENT_MS. Distinct from !esp_connected so the
                     *   header can say "ESP?" rather than the never-connected
                     *   "...", and so the one-shot haptic fires once. */
    uint32_t esp_rx_tick; /**< tick of the last line of any kind from the companion */
    char esp_kickoff[12]; /**< the scan mode the current scene asked the board for
                            *   ("flockcombo"); re-sent after a companion reboot
                            *   because the board boots WiFi-only (g_combo=false)
                            *   and the banner handler used to restore only band
                            *   and GPS, leaving BLE detection silently off. */
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
    /**
     * The companion's BUILD version from the FLOCKCO banner, "" if the firmware
     * predates it (anything before v0.88).
     *
     * The answer to "which firmware is actually on the board", which nothing
     * could answer before. Filenames on the SD card were the only label and they
     * cannot be verified after flashing -- a card here carried
     * companion_forensic/gatefix/survey/ungated .bin files that say nothing at
     * all, and companion_v073/v077/v087 whose labels nobody can check. Shown on
     * the ESP32 Firmware screen and written into diag.csv, so a field report says
     * which pair produced it.
     */
    char esp_build[12];
    /** Probe-signature table revision advertised by the companion. This is
     * separate from app/firmware build versions: two equal binaries can still
     * be paired with a locally rebuilt table. */
    char esp_sig_revision[16];
    uint32_t esp_sigtest_hash; /**< content hash produced by the production self-test */
    bool esp_sig_mismatch; /**< advertised revision differs from FDF_SIGNATURE_REVISION */
    bool esp_sigtest_seen; /**< a SIGTEST reply arrived this preflight/session */
    bool esp_sigtest_pass; /**< that reply matched the production signature table */

    /* ---- session diagnostics (see RECON_DIAG_PATH) ----------------------
     * Counted app-side so they can be compared against the companion's OWN
     * frames/hits totals. The comparison is the whole point: companion hits
     * climbing while `diag_accepted` stays flat means the app is dropping
     * detections; both flat means nothing was ever heard. */
    uint32_t diag_flock_msgs; /**< detection reports handed to report_flock */
    uint32_t diag_accepted; /**< of those, the ones that reached the table */
    uint32_t diag_rej_conf; /**< dropped: scored FlockConfidenceNone */
    uint32_t diag_rej_full; /**< dropped: table full and nothing evictable */
    uint32_t diag_rej_ignored; /**< dropped: the operator excluded this device */
    uint32_t diag_start_epoch; /**< wall clock at scan_session_start */

    /* ---- probe survey (see RECON_SURVEY_PATH) --------------------------- */
    SurveyEntry* survey;
    size_t survey_count;
    uint32_t survey_last_poll;
    uint32_t survey_last_autosave; /**< tick of the last mid-scan survey snapshot */
    /** Wall clock at scan start, the session column in survey_log.csv. Its own
     *  field rather than diag_start_epoch, which recon_diag_save() zeroes before
     *  recon_survey_save() runs. */
    uint32_t survey_session_epoch;
    bool esp_proto_mismatch; /**< companion speaks a different protocol version than the app */
    uint32_t esp_dropped_lines; /**< overlong RX lines dropped whole (wire-protocol health metric) */
    uint8_t esp_link_state; /**< EspLinkState: Stopped / Running / PortBusy (R6 error surface) */

    /* The WiFi Audit SCREEN was removed, but this table stays: the Locator
     * builds its target list from it, so a marked camera can be hunted by
     * BSSID after a sweep. */
    WifiAp* wifi; /**< results of the last WiFi sweep */
    size_t wifi_count;
    bool wifi_scanning; /**< true between WBEGIN and WEND */
    bool wifi_done; /**< a scan has completed at least once */
    uint8_t saved_backend; /**< backend to restore after the WiFi-audit scene */

    BleDevice* ble; /**< BLE devices / trackers */
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

    // Health / Preflight scene. Kept in ReconApp rather than file-scope statics
    // so a scene re-entry cannot inherit another app instance's timing state.
    uint32_t preflight_started_tick;
    uint32_t preflight_last_query_tick;
    uint32_t preflight_last_draw_tick;
    uint8_t preflight_page; /**< 0 = summary, 1 = full revision/counter details */

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
    bool fw_back_warned; /**< "Back locked until done." has been logged this run */
    volatile bool fw_writing; /**< the worker is inside flash_file(): ERASE,
                                *   write and verify. Not derived from fw_pct,
                                *   because the erase runs before the first
                                *   progress callback and a Back during it
                                *   aborted a flash with the chip already wiped. */
    volatile bool fw_log_dirty; /**< log changed -> re-render */
    /**
     * Flash/backup progress, 0..100, or -1 when no transfer is running.
     *
     * Kept OUT of fw_log because a percentage REPLACES itself rather than
     * accumulating. Logging it appended a line per step, so the operator had to
     * scroll a text box to find the current figure -- on a 128x64 screen, during
     * the one operation they cannot walk away from.
     */
    volatile int fw_pct;
    char fw_status[40]; /**< current flasher action, shown above the progress bar */

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
    bool hidden,
    uint8_t probe_rate);

/**
 * Record/merge an ASTM F3411 Remote ID broadcast from an unmanned aircraft.
 *
 * SEPARATE FROM recon_app_report_flock() rather than more parameters on it: the
 * evidence is a different kind. A Flock detection is an inference from a MAC
 * prefix and some frame behaviour; this is the aircraft stating its own
 * registration and its own coordinates because the law says it must. It also
 * carries a field nothing else has -- the operator's position.
 *
 * `payload` is the raw BLE service data starting at the 0x0D application code,
 * exactly as the companion forwarded it. Decoded here, via the host-tested
 * helpers/open_drone_id.c, rather than on the companion.
 *
 * Thread-safe (takes app->mutex internally); called from the ESP worker thread.
 */
void recon_app_report_remote_id(
    ReconApp* app,
    const uint8_t addr[6],
    int8_t rssi,
    const uint8_t* payload,
    size_t payload_len);

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

/** Record the companion's advertised probe-signature table revision. */
void recon_app_set_esp_sig_revision(ReconApp* app, const char* revision);

/** Record the companion's production signature-path self-test result. */
void recon_app_set_esp_sigtest(ReconApp* app, uint32_t hash, bool pass);

/** Update the count of overlong RX lines dropped whole (health metric; thread-safe). */
/**
 * True for companion SoCs with NO Bluetooth radio at all -- currently the
 * ESP32-S2, which is the chip on the official Flipper Wi-Fi Devboard.
 *
 * Such a board runs the Wi-Fi half of detection only and can never see the BLE
 * half, no matter what firmware it is given. The operator has to be told, because
 * "found nothing" from a board that cannot hear half the signals is a materially
 * weaker statement than the same words from one that can -- and indistinguishable
 * on screen unless we say so.
 *
 * @param target  the IDF target name from the companion's CHIP line.
 */
static inline bool recon_esp_chip_has_no_ble(const char* target) {
    return target && target[0] && strcmp(target, "esp32s2") == 0;
}

/**
 * Record WHICH BLE signal classified the device at @p mac (a FlockBleTell).
 *
 * Separate from recon_app_report_flock() rather than another parameter on it:
 * this is BLE-only evidence and the WiFi callers have nothing to say about it.
 * Display only -- it never feeds a confidence rung.
 */
void recon_app_set_ble_tell(ReconApp* app, const uint8_t mac[6], uint8_t tell);

/**
 * Exclude a device the operator says is theirs, and make it effective NOW.
 *
 * Writes ignored.txt and reloads the in-memory tables the detection gate reads,
 * so the device goes quiet on the next line rather than on the next app start.
 * `out_by_fp` reports whether a probe fingerprint was stored as well as the
 * address, which is what decides whether the exclusion survives the device
 * randomising its MAC; pass NULL if you do not care.
 *
 * @return true if the device is now excluded, including when it already was.
 */
bool recon_app_exclude_device(ReconApp* app, const uint8_t* mac, uint32_t fp, bool* out_by_fp);

/** Delete every exclusion, card and memory together (Reports > Forget Ignored). */
void recon_app_clear_exclusions(ReconApp* app);

/** Record one surveyed wildcard-probe transmitter (see RECON_SURVEY_PATH). */
void recon_app_survey_add(
    ReconApp* app,
    const uint8_t mac[6],
    uint32_t fp,
    int8_t rssi,
    uint8_t channel,
    uint16_t count,
    uint32_t fp2,
    const char* sig);

/**
 * Release the four bulk detection tables, returning ~10 KB of contiguous heap.
 *
 * ONLY safe with no scan running and no ESP/GPS worker alive -- every consumer
 * walks these under app->mutex and a NULL table would fault. The firmware screen
 * is the one place that qualifies. Persists hits first, and is idempotent.
 */
void recon_tables_release(ReconApp* app);

/** Re-allocate the tables released above, zeroed. Idempotent. Restores hits. */
void recon_tables_acquire(ReconApp* app);

/** Write survey.csv. Counts and signatures only -- no SSID, no position. */
void recon_survey_save(ReconApp* app);

/**
 * Snapshot survey.csv mid-scan, so a session that ends badly still leaves its
 * survey on the card. See the definition for why the log append is excluded.
 */
void recon_survey_autosave_tick(ReconApp* app);

/**
 * Append this session's survey rows to survey_log.csv, rotating past the cap.
 *
 * Called by recon_survey_save() with the Storage record already open, which is
 * why the parameter is a void* -- recon_app_i.h is included by pure-logic
 * helpers that must not pull in the storage headers. No-op for an empty session.
 */
void recon_survey_log_append(ReconApp* app, void* storage_rec);

/** Ask the companion for its survey on an interval (see RECON_SURVEY_PATH). */
void recon_survey_tick(ReconApp* app);

void recon_app_set_esp_dropped(ReconApp* app, uint32_t dropped);

/** Zero the per-session diagnostic counters and stamp the start time. */
void recon_diag_begin(ReconApp* app);

/** Append this session's diagnostic row. Always written, even with save_hits
 *  off: it records COUNTS, never a MAC, an SSID or a position, so it carries no
 *  record of where you have been. That is why it is not behind the privacy
 *  toggle -- the toggle exists to stop logging places, not to stop logging
 *  whether the hardware worked. */
void recon_diag_save(ReconApp* app);

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
 * GUI-tick watchdog for the two inputs that used to be "up until told
 * otherwise": drops esp_connected (and raises esp_lost, with one vibro pulse)
 * after RECON_ESP_SILENT_MS without a companion line, and drops gps_valid after
 * RECON_GPS_STALE_MS without a fix. Both only ever fall here or on an explicit
 * report; a silent device never reports.
 */
void recon_app_liveness_tick(ReconApp* app);

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
 * Drop every detection row that matches the exclusion lists (by address, or by
 * a non-zero fingerprint). Returns how many went. Called when an exclusion is
 * added and after the lists are loaded, so "never alert" applies to what is
 * already stored and not only to the next sighting.
 */
size_t recon_app_purge_excluded(ReconApp* app);

/**
 * Delete hits.csv AND empty the detection table, live rows included. Returns
 * false, with the table untouched, if the card refused the delete. For the
 * explicit "Clear Saved Hits" action, where rows left in RAM would be written
 * back by the next save. recon_hits_clear() keeps live rows and is for turning
 * the Save hits setting off, where what is on screen should stay on screen.
 */
bool recon_hits_clear_all(ReconApp* app);

/**
 * Persist after the operator DELETED an entry. Writes the table, or removes
 * `hits.csv` entirely when they deleted the last one.
 *
 * Never call this for an incidentally empty table. recon_hits_save() runs on
 * every scan-session exit, and folding this removal into it turned Net
 * Guardian's baseline reset into permanent data loss (issue #5).
 */
/** Flush hits.csv on an interval while a scan runs, so a flat battery cannot
 *  take the whole session with it. No-op when Save hits is off. */
void recon_hits_autosave_tick(ReconApp* app);

void recon_hits_save_after_delete(ReconApp* app);
