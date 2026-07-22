// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
/**
 * @file esp_parser.h
 * Companion wire-protocol parser: line text -> plain tagged record.
 *
 * Pure logic (only flock_db + libc), so it is host-testable with adversarial
 * line fixtures. `esp_link.c` owns the thin adapter that applies a parsed record
 * to ReconApp; keeping parse and mutate separate is what makes the wire protocol
 * unit-testable (parse != mutate).
 *
 * The companion emits one record per line, comma-separated:
 *   FLOCKCO,<ver>                       version banner (-> connected)
 *   S,<frames>,<hits>,<ch>[,<deauths>]  status heartbeat
 *   WBEGIN / W,<bssid>,<rssi>,<ch>,<auth>,<pair>,<grp>,<wps>,<ssid> / WEND
 *   BBEGIN / BLE,<addr>,<rssi>,<cat>,<company>,<name>[,<mfghex>][,rv=1] / BEND
 *   D,<mac>,<rssi>,<ch>,<type>,<conf>,<ssid>[,fp=<hex32>]   Flock detection
 *   DA,<bssid>,<ch>                     deauth/disassoc attack target
 *   ATK,<kind>,<value>                  active attack-tool signature
 *   LOC,<rssi>                          live Locator RSSI
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "flock_db.h" // FlockConfidence

#ifdef __cplusplus
extern "C" {
#endif

// Wire-protocol version this app speaks. The companion announces its own version
// in the FLOCKCO banner (see EspMsgBanner.version); a mismatch means the two may
// disagree on line formats, so the app flags it as a health warning rather than
// silently mis-parsing. A banner version of 0 means "old firmware, no version
// field" and is treated as compatible/unknown, not a mismatch.
#define ESP_PROTO_VERSION 1

/** Which companion record a line decoded to (EspMsgIgnore = unrecognised/malformed). */
typedef enum {
    EspMsgIgnore = 0,
    EspMsgBanner, /**< FLOCKCO version banner -> mark connected */
    EspMsgStatus, /**< S: frame/hit/channel heartbeat (+ optional deauth count) */
    EspMsgWifiBegin, /**< WBEGIN: start of a WiFi security scan batch */
    EspMsgWifiEnd, /**< WEND: end of the batch */
    EspMsgWifiAp, /**< W: one scanned access point */
    EspMsgBleBegin, /**< BBEGIN: start of a BLE scan batch */
    EspMsgBleEnd, /**< BEND: end of the batch */
    EspMsgBleDev, /**< BLE: one discovered device */
    EspMsgFlock, /**< D: a Flock/ALPR detection */
    EspMsgDeauthTarget, /**< DA: attributed deauth/disassoc target */
    EspMsgAttack, /**< ATK: active attack-tool signature */
    EspMsgLocate, /**< LOC: live RSSI for the Locator target */
} EspMsgType;

/**
 * Parsed companion record. String fields (ssid/name/attack.kind) point INTO the
 * parsed line buffer and are valid only until it is reused; scalar/byte fields
 * are copies. Unused members for a given `type` are left zeroed.
 */
typedef struct {
    EspMsgType type;
    union {
        struct { // EspMsgFlock (D)
            uint8_t mac[6];
            const char* ssid;
            int8_t rssi;
            uint8_t channel;
            char ftype;
            FlockConfidence conf;
            uint32_t fp;
        } flock;
        struct { // EspMsgWifiAp (W)
            uint8_t bssid[6];
            const char* ssid;
            int8_t rssi;
            uint8_t channel;
            uint8_t auth;
            uint8_t pairwise;
            bool wps;
        } wifi;
        struct { // EspMsgBleDev (BLE)
            uint8_t addr[6];
            const char* name;
            int8_t rssi;
            uint8_t cat;
            uint16_t company;
            uint8_t mfg[32];
            size_t mfg_len;
            bool raven_gatt;
        } ble;
        struct { // EspMsgStatus (S)
            uint32_t frames;
            uint32_t hits;
            uint8_t channel;
            bool have_deauths;
            uint32_t deauths;
        } status;
        struct { // EspMsgDeauthTarget (DA)
            uint8_t bssid[6];
            uint8_t channel;
        } deauth;
        struct { // EspMsgAttack (ATK)
            const char* kind;
            uint32_t value;
        } attack;
        struct { // EspMsgLocate (LOC)
            int8_t rssi;
        } locate;
        struct { // EspMsgBanner (FLOCKCO)
            uint8_t version; /**< companion's announced wire-protocol version (0 = old FW) */
        } banner;
    } u;
} EspMsg;

/**
 * Split `line` in place on commas into up to `max` fields. DESTRUCTIVE: writes a
 * NUL over each comma. Returns the field count (>= 1 for a non-empty line, since
 * the whole line is field 0). Stops after `max` fields (the remainder stays in
 * the last field, matching the hand-rolled splitters this replaces).
 */
int esp_split_fields(char* line, char** fields, int max);

/** One hex nibble 0-15, or -1 if `c` is not a hex digit. */
int esp_hexval(char c);

/**
 * Parse one companion line into `out`. DESTRUCTIVE on `line`. Pure: no app /
 * firmware calls (consults only the pure flock_db matchers). Returns out->type.
 */
EspMsgType esp_parse_companion_line(char* line, EspMsg* out);

#ifdef __cplusplus
}
#endif
