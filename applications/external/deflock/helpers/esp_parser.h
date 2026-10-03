// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
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
 *   BBEGIN / BLE,<addr>,<rssi>,<cat>,<company>,<name>[,<mfghex>][,rv=1][,sep=1] / BEND
 *   D,<mac>,<rssi>,<ch>,<type>,<conf>,<ssid>[,fp=<hex32>][,pr=<n>][,cls=a|x][,hid=1]
 *   DA,<bssid>,<ch>                     deauth/disassoc attack target
 *   ATK,<kind>,<value>                  active attack-tool signature
 *   LOC,<rssi>                          live Locator RSSI
 *   ACT,<op>,<status>[,<rssi>]          explicit tracker action result
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
    EspMsgAction, /**< ACT: result of an explicit Ping/Ring request */
    EspMsgGpsNmea, /**< G: one NMEA sentence relayed from a GPS on the ESP board */
    EspMsgGpsCfg, /**< GPSCFG: the companion's echo of its GPS relay state */
    EspMsgChip, /**< CHIP: the board's real SoC, GPIO count and usable GPS pins */
    EspMsgSurvey, /**< SV: one wildcard-probe transmitter seen, matched or NOT */
    EspMsgBand, /**< BAND: the band selection actually in force */
    EspMsgRemoteId, /**< RID: an ASTM F3411 Remote ID broadcast from an aircraft */
    EspMsgSigRevision, /**< SIGREV: companion probe-signature table revision */
    EspMsgSigTest, /**< SIGTEST: production signature-path self-test result */
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
            FlockDevClass dev_class; /**< from `cls=`; absent -> FlockClassAlpr */
            bool hidden;
            uint8_t probe_rate; /**< probes seen from this transmitter inside the
                                  *  companion's window (0 = not reported). An
                                  *  observation, never a confidence input. */ /**< from `hid=`: beacons but withholds its SSID.
                           *  Reported, never scored -- see parse_flock(). */
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
            bool tracker_separated; /**< tracker advert was in separated state */
            /**
             * The advert carried Axon's "BWCDEVICE" service-data tag.
             *
             * MAC-INDEPENDENT and specific: it says the device is a BODY-WORN
             * CAMERA, where the Axon OUI only ever says "something Axon made".
             * That distinction survives address randomisation, which a prefix
             * match does not.
             */
            bool bwc_tag;
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
        struct { // EspMsgGpsNmea (G)
            /**
             * The relayed sentence, starting at '$'. Points INTO the caller's
             * line buffer like the other string fields here, and the app hands
             * it straight to nmea_parse_line() -- deliberately NOT parsed here.
             *
             * Some boards wire their GPS to the ESP32 rather than to the
             * Flipper's header, so the Flipper cannot see it at all (issue #5).
             * Relaying the raw sentence means the companion needs no NMEA code,
             * and the fix is decoded by the same host-tested parser the direct
             * UART path already uses -- one implementation and one set of
             * lock-loss semantics, whichever wire the bytes arrived on.
             *
             * Mutable (char*, not const char*): nmea_parse_line() tokenizes in
             * place, matching how the direct path already feeds it.
             */
            char* nmea;
        } gps;
        struct { // EspMsgSurvey (SV)
            /**
             * A device seen emitting WILDCARD PROBE REQUESTS, whether or not it
             * matched anything we ship. This is deliberately unfiltered: field
             * reports show tens of thousands of frames captured with zero
             * candidates, and the detector cannot distinguish "an empty street"
             * from "a camera on an OUI we do not carry" or one using a randomised
             * MAC. The survey is what makes that difference visible.
             */
            uint8_t mac[6];
            uint32_t fp; /**< IE-skeleton hash -- survives MAC randomisation */
            /**
             * IE-CONTENT hash. The skeleton above folds in tag id and length and
             * discards every byte of content, so the IEs that actually describe
             * a radio -- rates, HT/VHT/HE capabilities, extended capabilities --
             * count for nothing. Across 120 devices in a real capture it yielded
             * 49 distinct values with 74% of devices in a collision, one hash
             * covering 24 separate devices. This one folds the capability
             * contents in. 0 from firmware that predates it.
             */
            uint32_t fp2;
            int8_t rssi; /**< strongest seen, i.e. closest approach */
            uint8_t channel;
            uint16_t count; /**< a camera probes forever; a phone bursts and stops */
            /**
             * Printable IE signature (see the companion's ie_sig_string): an
             * ordered IE tag list, readable and quotable in a field report
             * where a bare hash is not. Borrowed pointer into the caller's line
             * buffer, valid only for this callback -- copy it if you keep it.
             * Empty from older firmware.
             */
            const char* sig;
        } survey;

        struct { // EspMsgRemoteId (RID)
            /**
             * A Remote ID broadcast, forwarded RAW and decoded elsewhere.
             *
             * The companion recognises only the transport (BLE service data
             * under UUID 0xFFFA with application code 0x0D) and hex-dumps what
             * follows. Nothing here interprets it: helpers/open_drone_id.c does,
             * because that file is pure C with host tests and this is
             * attacker-supplied radio input. Keeping the decoder on one side of
             * the wire is the lesson the duplicated OUI tables taught.
             *
             * 64 bytes is well above what BLE legacy advertising can carry (the
             * entire AD is 31), so a real aircraft never fills it.
             */
            uint8_t addr[6];
            int8_t rssi;
            uint8_t payload[64];
            size_t payload_len;
        } rid;

        struct { // EspMsgChip (CHIP)
            /**
             * What the companion is actually running on. The app used to offer a
             * hardcoded classic-ESP32 pin list on every board: on an ESP32-C5
             * four of those pins do not exist, two are the flash/PSRAM bus and
             * one is UART0 itself. The board is the only thing that knows its own
             * pinout, so it reports it rather than the app guessing (issue #5).
             */
            const char* target; /**< IDF target name, e.g. "esp32" / "esp32c5" */
            uint8_t gpio_count;
            uint64_t gps_pin_mask; /**< bit N set = GPIO N can carry the GPS */
            bool has_5ghz;
        } chip;
        struct { // EspMsgBand (BAND)
            uint8_t sel; /**< ReconEspBand actually in force (2.4-only parts force 0) */
            uint16_t channels; /**< how many channels the sweep covers */
        } band;
        struct { // EspMsgGpsCfg (GPSCFG)
            /**
             * The companion's answer to `gps <rx> [baud]`, echoed on every such
             * command. Without it the app could not tell "relay running, still
             * searching" from "relay refused that pin" from "this firmware has
             * no relay at all" -- three states that all rendered as a hollow
             * "searching" badge forever, which is what made issue #5's GPS
             * problem take four rounds to pin down.
             */
            bool on; /**< the companion is relaying (it accepted the pin) */
            int16_t pin; /**< the pin it is using, or the one it refused */
            uint32_t baud;
        } gpscfg;
        struct { // EspMsgAttack (ATK)
            const char* kind;
            uint32_t value;
        } attack;
        struct { // EspMsgLocate (LOC)
            int8_t rssi;
        } locate;
        struct { // EspMsgAction (ACT)
            const char* op; /**< PING or RING */
            const char* status; /**< ok, sent, rejected, not_found, ... */
            int8_t rssi;
            bool have_rssi;
        } action;
        struct { // EspMsgBanner (FLOCKCO)
            uint8_t version; /**< companion's announced wire-protocol version (0 = old FW) */
            /**
             * The companion's BUILD version ("0.88"), empty on firmware that
             * predates it.
             *
             * SEPARATE FROM `version`, which is the wire-protocol number. The
             * protocol number answers "can these two talk"; this answers "which
             * firmware is actually on the board", which nothing could answer
             * before and which is the question every confusing hardware session
             * has turned on.
             */
            char build[12];
        } banner;
        struct { // EspMsgSigRevision (SIGREV)
            const char* revision; /**< borrowed token in the mutable line buffer */
        } sigrev;
        struct { // EspMsgSigTest (SIGTEST)
            const char* signature; /**< may contain commas; borrowed from line */
            uint32_t hash;
            bool pass;
        } sigtest;
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
