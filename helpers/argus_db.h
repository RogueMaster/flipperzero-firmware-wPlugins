#pragma once

#include <furi.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * Argus in-memory model. Lives on the Flipper and is fed by the ESP32 over
 * UART. Two jobs:
 *   1. count deauth / disassociation frames and surface a "deauth storm" rate
 *   2. keep a small table of nearby APs and flag evil twins of the guarded SSID
 *
 * The DB is touched from two threads (UART worker writes, GUI thread reads), so
 * every public call takes an internal mutex. Views read through the snapshot
 * copy helpers so they never hold the lock while drawing.
 */

#define ARGUS_MAX_APS         32
#define ARGUS_MAX_LOG         40
#define ARGUS_SSID_MAX        33 // 32 chars + NUL
#define ARGUS_STORM_WINDOW_MS 2000u // sliding window for the deauth-rate meter

typedef enum {
    ArgusEncOpen = 0,
    ArgusEncWep,
    ArgusEncWpa,
    ArgusEncWpa2,
    ArgusEncWpa3,
    ArgusEncUnknown,
} ArgusEnc;

typedef enum {
    ArgusThreatDeauth = 0,
    ArgusThreatDisassoc,
    ArgusThreatEvilTwin,
} ArgusThreatKind;

typedef struct {
    uint8_t bssid[6];
    char ssid[ARGUS_SSID_MAX];
    uint8_t channel;
    int8_t rssi;
    ArgusEnc enc;
    bool clone; // guarded SSID seen on a BSSID that is not the home AP
    uint16_t beacons;
    uint32_t last_seen;
} ArgusAp;

typedef struct {
    ArgusThreatKind kind;
    uint8_t addr[6]; // attacker/source for deauth, BSSID for evil twin
    char ssid[ARGUS_SSID_MAX];
    uint8_t channel;
    int8_t rssi;
    uint8_t reason; // 802.11 reason code (deauth/disassoc)
    uint32_t time_tick;
} ArgusThreat;

typedef struct {
    uint32_t deauth_total; // every deauth + disassoc ever seen this session
    uint32_t deauth_rate; // frames inside the current sliding window
    uint32_t frame_total; // all management frames seen (activity meter)
    uint32_t last_deauth_tick; // furi tick of the most recent deauth
    size_t ap_count;
    size_t twin_count;
} ArgusStats;

typedef struct ArgusDb ArgusDb;

ArgusDb* argus_db_alloc(void);
void argus_db_free(ArgusDb* db);

/* Wipe APs, threat log and counters. The guarded SSID is preserved. */
void argus_db_reset(ArgusDb* db);

/* The SSID the user wants protected (their own network). */
void argus_db_set_guard(ArgusDb* db, const char* ssid);
void argus_db_get_guard(ArgusDb* db, char* out, size_t out_len);
bool argus_db_has_guard(ArgusDb* db);

/* Feed from the UART worker. Both return true when a *new* alert condition is
 * crossed (a storm just started / a brand-new evil twin was flagged). */
bool argus_db_on_deauth(
    ArgusDb* db,
    ArgusThreatKind kind,
    const uint8_t src[6],
    const uint8_t bssid[6],
    uint8_t channel,
    int8_t rssi,
    uint8_t reason,
    uint32_t storm_threshold);
bool argus_db_on_ap(
    ArgusDb* db,
    const uint8_t bssid[6],
    uint8_t channel,
    int8_t rssi,
    ArgusEnc enc,
    const char* ssid);

void argus_db_note_frame(ArgusDb* db); // bump the management-frame activity meter

/* Decay the sliding deauth-rate window. Call from the GUI tick. */
void argus_db_tick(ArgusDb* db, uint32_t window_ms);

/* Atomic snapshots for the views. */
void argus_db_get_stats(ArgusDb* db, ArgusStats* out);
size_t argus_db_copy_aps(ArgusDb* db, ArgusAp* out, size_t max);
size_t argus_db_copy_twins(ArgusDb* db, ArgusAp* out, size_t max);
size_t argus_db_copy_log(ArgusDb* db, ArgusThreat* out, size_t max); // newest first

const char* argus_enc_label(ArgusEnc enc);
