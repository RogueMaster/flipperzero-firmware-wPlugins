#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/*
 * ★ Shared pure-logic types. Must not include any furi header.
 *
 * Struct fields record only facts marked ✅ in docs/VERIFY.md.
 * Array lengths are storage caps (the +1 for the trailing NUL lives in the
 * field declaration), not "official maximums" from the Marauder protocol --
 * where VERIFY gives no number, the comment says cap, not a verified width.
 */

/* -------------------------------------------------------------------------- */
/* Storage caps                                                               */
/* -------------------------------------------------------------------------- */

enum {
    /* V-002: BSSID / MAC printed via WiFi.BSSIDstr / toString, form AA:BB:CC:DD:EE:FF */
    SR_BSSID_MAX = 17,

    /*
     * V-002 confirms an SSID column exists but gives no maximum length.
     * 32 is the IEEE 802.11 SSID element octet limit (0-32), used as a cap.
     * It is not a macro verified in the Marauder sources.
     */
    SR_SSID_MAX = 32,

    /*
     * V-002 / SPIKE-00-marauder.md: security_int_to_string word list verified.
     * Longest is [WPA2_WPA3_PSK] = 15. BLE rows carry [BLE] in the AUTH column.
     */
    SR_AUTH_MAX = 15,

    /* V-002 / V-004: YYYY-MM-DD HH:MM:SS. Source may print an empty string without a fix */
    SR_DATETIME_MAX = 19,

    /*
     * V-002 / V-004 splice lat/lon/alt/acc into the row as strings.
     * Print precision is not pinned down in VERIFY (needs a T0.4 dump); 31 is only a cap.
     */
    SR_COORD_MAX = 31,

    /* V-005: Firmware row value is Marauder. 31 is a cap, not a verified width */
    SR_FW_NAME_MAX = 31,

    /* V-005: Version row sample v1.14.1. 31 is a cap */
    SR_FW_VERSION_MAX = 31,

    /* V-005: HARDWARE_NAME on the C5 is ESP32-C5 DevKit. 31 is a cap */
    SR_FW_HARDWARE_MAX = 31,

    /* V-005: ESP-IDF row comes from esp_get_idf_version(), not hardcoded here. 63 is a cap */
    SR_FW_IDF_MAX = 63,
    SR_DIAG_HB_COUNT = 6,

    /* V-004: Text row is optional; content and length unverified. 63 is a cap */
    SR_GPS_TEXT_MAX = 63,

    /* V-004: Sats row is the output of getNumSatsString(). 7 is a cap */
    SR_GPS_SATS_MAX = 7,

    /*
     * Cap on effective characters in the sr_line assembly buffer.
     * 511 + trailing NUL = 512 B, the line-assembly budget in SigRoam-Phase1-Plan.md 3.5.
     */
    SR_RAW_LINE_MAX = 511,
};

_Static_assert(SR_BSSID_MAX == 17, "V-002 MAC AA:BB:CC:DD:EE:FF is 17 chars");
_Static_assert(SR_DATETIME_MAX == 19, "V-002/V-004 YYYY-MM-DD HH:MM:SS is 19 chars");
_Static_assert(SR_AUTH_MAX == 15, "V-002 longest verified auth is [WPA2_WPA3_PSK]");

/* -------------------------------------------------------------------------- */
/* Enums                                                                      */
/* -------------------------------------------------------------------------- */

typedef enum {
    SrSourceUnknown = 0,
    SrSourceMarauder,
    SrSourceGhostesp,
    SrSourceNative,
} SrSourceKind;

typedef enum {
    SrParseOk = 0,
    SrParseNeedMore,
    SrParseUnknown,
    SrParseMalformed,
} SrParseResult;

/* V-002 last column Type: WIFI or BLE */
typedef enum {
    SrRadioWifi = 0,
    SrRadioBle,
} SrRadioType;

/*
 * V-003: stopscan returns one of three strings on the serial port, with no exit code.
 * The literal strings must not be rewritten.
 */
typedef enum {
    SrStopWifiTranRecv = 0, /* "Stopping WiFi tran/recv" */
    SrStopEndNmea, /* "END OF NMEA STREAM" */
    SrStopGpsUpdates, /* "Stopping GPS data updates" */
} SrStopReason;

/*
 * SrEventNone is the zero-init sentinel, not a protocol event.
 * The remaining kinds cover the event surface required by the task card.
 */
typedef enum {
    SrEventNone = 0,
    SrEventApFound, /* V-002 WiFi row (carries the cursor prefix) */
    SrEventBleFound, /* V-002 BLE row (no cursor prefix) */
    SrEventGps, /* V-004 gpsdata block */
    SrEventScanStarted, /* V-001 StartingWardrive. Stop with stopscan */
    SrEventScanStopped, /* V-003 */
    SrEventFirmware, /* V-005 info four rows / boot banner */
    SrEventUnknown, /* Malformed or unrecognized row, passed through to the Raw view */
    SrEventBusy, /* Board refused a command in its current state (2026-09-07) */
    SrEventSess, /* Schema 9 Sess: line; must append after Busy (N6 / test_model.c oracle) */
    SrEventQual, /* F2 Qual: line; must append after Sess */
    SrEventRadio, /* T6.5 Radio: line; must append after Qual */
} SrEventKind;

/* Pin the trailing kinds so inserting SrEventSess before Unknown silently
 * retargets test_model.c kOracleNext columns (contiguous 0..7 = None..Unknown).
 * SrEventBusy was itself appended after Unknown (=8) for the same reason.
 * SrEventQual appends after Sess (=10); the 3x10 oracle table is unchanged.
 * SrEventRadio appends after Qual (=11). */
_Static_assert((int)SrEventUnknown == 7, "kOracleNext columns 0..7 are None..Unknown");
_Static_assert((int)SrEventBusy == 8, "SrEventBusy appends after Unknown");
_Static_assert((int)SrEventSess == 9, "SrEventSess appends after Busy");
_Static_assert((int)SrEventQual == 10, "SrEventQual appends after Sess");
_Static_assert((int)SrEventRadio == 11, "SrEventRadio appends after Qual");

/* -------------------------------------------------------------------------- */
/* Structs                                                                    */
/* -------------------------------------------------------------------------- */

typedef struct {
    SrSourceKind kind; /* Probe verdict; not one of the info rows */
    char firmware[SR_FW_NAME_MAX + 1]; // ref: V-005 Firmware: Marauder
    char version[SR_FW_VERSION_MAX + 1]; // ref: V-005 Version: v1.14.1
    char hardware[SR_FW_HARDWARE_MAX + 1]; // ref: V-005 Hardware: ESP32-C5 DevKit
    char esp_idf[SR_FW_IDF_MAX + 1]; // ref: V-005 ESP-IDF: {esp_get_idf_version()}
    /* Fifth #info line, added by the firmware 2026-09-07: session state, the seal-gate
     * bitmap and the six task heartbeats. It is the only channel that still answers
     * when the board's SD path has stopped, so it must not depend on the card.
     * Stored decoded rather than as the raw 83-byte line: SrFirmwareInfo is the widest
     * arm of SrEvent, and ADR-016 pins sizeof(SrEvent) <= 256 / sizeof(SrParser) <= 512
     * (test_line.c:22, test_parse_marauder.c:14). 28 bytes fits; the raw line did not.
     * diag_seen distinguishes "board sent no Diag line" (older firmware) from an
     * all-zero reading, which would otherwise look the same and mean the opposite. */
    bool diag_seen;
    uint8_t diag_state; /* sr_session_state_t on the board */
    uint8_t diag_seal; /* SR_SEAL_* bitmap on the board   */
    uint32_t diag_hb[SR_DIAG_HB_COUNT]; /* gps, link, scan, fuse, store, ble */
} SrFirmwareInfo;

/*
 * "Busy: st=<u> seal=<02x>" — the board echoed the command (so cmdack advanced and the
 * L2 indicator cleared) but refused to act on it, because the previous session has not
 * finished sealing. Distinct from a timeout: a timeout means we do not know, this means
 * the board answered no. sr_scan_ctl.h deliberately does not clear cmd_pending on a
 * timeout; an explicit refusal is the one thing that may.
 */
typedef struct {
    uint8_t state; /* sr_session_state_t on the board */
    uint8_t seal; /* SR_SEAL_* bitmap on the board */
} SrBusyInfo;

/*
 * "Sess: ap=<u32> ble=<u32> ms=<u32> ih=<u32> ihmin=<u32> ps=<u32> psmin=<u32>"
 * — last line of the #info reply (after Diag: / CmdTrace:), firmware schema 9.
 * Independent union arm: stuffing these into SrFirmwareInfo would widen SrEvent
 * past ADR-016's 256 B cap (n6-fap-sess-consume.md §4). Parser is strict on all
 * seven fields (p != len); only ap/ble/ms are stored. ih/ihmin/ps/psmin are
 * board heap telemetry and are discarded after the scan.
 *
 * sess_rev == 0 means the line has never been seen (unknown), not "all zeros".
 * ms == 0 means no session is running — never "just started".
 */
typedef struct {
    uint32_t ap;
    uint32_t ble;
    uint32_t ms;
} SrSessInfo;

_Static_assert(
    sizeof(SrSessInfo) == 12,
    "SrSessInfo is 3 x uint32; padding would waste the SrEvent budget");

/*
 * "Qual: gga=<u32> ggafix=<u32> drop=<u32> net=<u32> sd=<u> sats=<u> topd=<u>"
 * — last line of the #info reply (after Sess:), F2 capture-health face.
 * Independent union arm: 4 x uint32 + 3 x uint8 = 19 → aligned 20, narrower
 * than SrFirmwareInfo, so SrEvent stays 240 (host pin in test_scan_ctl.c).
 * Parser is strict on every field (p != len). qual_rev == 0 means the line
 * has never been seen (unknown), not "all zeros" — a genuine gga=0 line is
 * a measurement and must bump the rev.
 */
typedef struct {
    uint32_t gga;
    uint32_t ggafix;
    uint32_t drop;
    uint32_t net;
    uint8_t sd;
    uint8_t sats;
    uint8_t topd;
} SrQualInfo;

_Static_assert(sizeof(SrQualInfo) == 20, "SrQualInfo is 4 x uint32 + 3 x uint8, aligned 20");

/*
 * "Radio: wifi=<0|1> ble=<0|1>" — permission bits after Sess: (ADR-24 / T6.5).
 * Independent union arm, 2 B. Absence of the line is unknown, never zero;
 * wifi/ble here are not Sess: counts. Parser is strict (p != len) and each
 * bit is exactly one character 0 or 1.
 */
typedef struct {
    uint8_t wifi;
    uint8_t ble;
} SrRadioInfo;

_Static_assert(sizeof(SrRadioInfo) == 2, "SrRadioInfo is 2 permission bits");

/*
 * ref: V-001
 * Proprietary wardrive parameters enabled in phase one: none. -s stays commented out.
 * Scan commands share the optional -serial flag (save_serial in the sources).
 * Do not add channel / band / dwell_ms / max_aps here.
 */
typedef struct {
    bool mirror_to_serial; // ref: V-001 -serial / save_serial
} SrScanCfg;

/*
 * ref: V-002
 * WiFi: {cursor} | {BSSID},{SSID},{[AUTH]},{YYYY-MM-DD HH:MM:SS},{ch},{RSSI},{lat},{lon},{alt},{acc},WIFI
 * BLE: {MAC},,[BLE],{dt},0,{RSSI},...,BLE (no cursor prefix)
 * Commas inside the SSID are replaced with _ by the firmware, so the field count is fixed.
 * The row is still printed when there is no GPS fix.
 */
typedef struct {
    uint32_t cursor; // ref: V-002 {cursor} | prefix; BLE rows lack it, filled with 0
    char bssid[SR_BSSID_MAX + 1]; // ref: V-002 BSSID / MAC
    char ssid[SR_SSID_MAX + 1]; // ref: V-002 SSID (commas already replaced with _)
    char auth[SR_AUTH_MAX + 1]; // ref: V-002 [AUTH], or [BLE] on BLE rows
    char datetime[SR_DATETIME_MAX + 1]; // ref: V-002 YYYY-MM-DD HH:MM:SS
    int channel; // ref: V-002 {ch}; fixed 0 on BLE rows
    int rssi; // ref: V-002 {RSSI}
    char lat[SR_COORD_MAX + 1]; // ref: V-002 {lat}
    char lon[SR_COORD_MAX + 1]; // ref: V-002 {lon}
    char alt[SR_COORD_MAX + 1]; // ref: V-002 {alt}
    char acc[SR_COORD_MAX + 1]; // ref: V-002 {acc}
    SrRadioType radio; // ref: V-002 last column WIFI | BLE
} SrApRecord;

/*
 * ref: V-004
 * gpsdata emits a block every 5000 ms: Fix / optional Text / Sats / Acc / Lat / Lon / Alt / D/T.
 * The exact string form of coordinates and satellite counts follows the dump;
 * stored here verbatim as printed.
 */
typedef struct {
    bool fix; // ref: V-004 Fix: Yes|No
    char text[SR_GPS_TEXT_MAX + 1]; // ref: V-004 optional Text:; empty string when absent
    char sats[SR_GPS_SATS_MAX + 1]; // ref: V-004 Sats:
    char acc[SR_COORD_MAX + 1]; // ref: V-004 Acc:
    char lat[SR_COORD_MAX + 1]; // ref: V-004 Lat:
    char lon[SR_COORD_MAX + 1]; // ref: V-004 Lon:
    char alt[SR_COORD_MAX + 1]; // ref: V-004 Alt:
    char datetime[SR_DATETIME_MAX + 1]; // ref: V-004 D/T:
} SrGpsSnapshot;

/*
 * CSV-derived live GPS (D12): every wardrive row carries the GPS state of that instant
 * (F6/F7). Deliberately NOT SrGpsSnapshot -- the CSV row has no Sats/Text, and mixing the
 * two sources into one struct makes "where did this number come from" unanswerable, which
 * is exactly how the V-061 (1) gps_blocks probe would get destroyed.
 * fix is DERIVED, not transmitted: F8 pins datetime non-empty <=> nmea.isValid().
 */
typedef struct {
    bool fix; /* derived: datetime[0] != '\0' (F8) */
    char lat[SR_COORD_MAX + 1];
    char lon[SR_COORD_MAX + 1];
    char alt[SR_COORD_MAX + 1];
    char acc[SR_COORD_MAX + 1];
    char datetime[SR_DATETIME_MAX + 1];
} SrGpsCsvView;

/*
 * A **borrowed view** of an unknown / malformed row, not a copy (ADR-010, delivering ADR-009 decision 4).
 *
 * text points into the sr_line assembly buffer and its **lifetime is limited to the
 * single feed_line call**. The caller must treat it as invalid the moment feed_line
 * returns: never store it in a ring buffer, never hand it across threads, never put
 * it in a ViewModel snapshot. To retain it, narrow-copy into your own buffer.
 *
 * **Readable-range contract: only [text, text+len) is guaranteed readable; text[len]
 * is NOT guaranteed to be NUL.** That is exactly what the len field means -- the view
 * is allowed to be a window inside a row rather than a C string.
 * So retaining it requires `memcpy` of len bytes plus your own NUL, and **forbids
 * `sr_strlcpy` / `strlen` / `strcmp` and friends that stop only at a NUL**: they read
 * past text[len]. This is not hypothetical -- proven with ASan during the T2.4 review
 * on 2026-08-16: going through sr_strlcpy triggers a heap-buffer-overflow (on the strlen
 * line in sr_types.h), and a plain make stays green and never catches it.
 * The only current producer, sr_line, happens to deliver NUL-terminated rows, so a
 * violation will not fault immediately; it stays latent until the first producer that
 * really hands out a window view (such as the T2.3 parser) appears.
 * Regression test: test_window_view_no_overread in test_model.c (requires make asan).
 *
 * A copying version (char text[512]) would push SrEvent from 240 B to 528 B, colliding
 * head-on with the 2048 B worker stack in Plan 3.4 -- see the measurement table in ADR-010.
 */
typedef struct {
    const char* text;
    size_t len;
} SrRawView;

typedef struct {
    SrEventKind kind;
    union {
        SrApRecord ap; /* SrEventApFound */
        SrApRecord ble; /* SrEventBleFound */
        SrGpsSnapshot gps; /* SrEventGps */
        SrFirmwareInfo firmware; /* SrEventFirmware */
        SrStopReason stop; /* SrEventScanStopped */
        SrRawView unknown; /* SrEventUnknown -- borrowed, not owned */
        SrBusyInfo busy; /* SrEventBusy */
        SrSessInfo sess; /* SrEventSess — narrower than SrFirmwareInfo, SrEvent stays 240 */
        SrQualInfo qual; /* SrEventQual — 20 B, narrower than SrFirmwareInfo */
        SrRadioInfo radio; /* SrEventRadio — 2 B, narrower than SrFirmwareInfo */
    } u;
} SrEvent;

/*
 * A **protocol-neutral** expression of the fact "the device acknowledged the command I sent"
 * (L2 of ADR-022 decision 1 / ADR-023).
 *
 * Not calling it echo is deliberate: echo is merely how Marauder implements L2; our own
 * firmware uses `@SR1 ACK seq=<n>`.
 *
 * ⚠️ Semantic boundary (T4.10 wording must respect this; stated here so the next layer
 *   does not repeat the same mistake):
 *   count[] answers only "how many acknowledgements have arrived". **"No acknowledgement"
 *   must not be rendered as any single specific cause** -- under Marauder there are at
 *   least two: (1) the CLI is still booting (V-056 measured "several seconds", during
 *   which Probe correctly reports No reply); (2) the device is parked in
 *   WIFI_SCAN_GPS_NMEA mode, where commands are dropped and **not echoed** (V-008).
 *
 * ⚠️ rev and count[] are both wrapping counters: **consumers must compare with `!=`,
 *   never with `>`** -- ADR-016 / ADR-017 / ADR-021 each stepped on "using a magnitude
 *   comparison as a counter predicate".
 *
 * No timestamps are stored here: the ★ layer does not touch the clock (feed_line takes
 * no tick argument, and should not gain one). Consumers have their own tick; snapshot
 * count[cls] when sending and watch for it to change to learn when the ack arrived.
 */
typedef enum {
    SrCmdAckNone =
        0, /* Not an acknowledgement of any known command; count[0] stays 0, never used */
    SrCmdAckStart, /* wardrive / wardrive -serial */
    SrCmdAckStop, /* stopscan */
    SrCmdAckGps, /* gpsdata */
    SrCmdAckInfo, /* info */
    SrCmdAckPoi, /* wardrivepoi */
    SrCmdAckClassCount
} SrCmdAckClass;

typedef struct {
    uint32_t rev; /* Incremented whenever any class of ack arrives; 0 = never acknowledged */
    uint32_t count[SrCmdAckClassCount]; /* Per-class totals */
} SrCmdAckObs;

/*
 * A concrete small struct, not an opaque handle.
 * Phase one parses each row independently, with no per-codec private state.
 */
/*
 * ADR-013 / ADR-016: contains SrGpsSnapshot + a partial SrFirmwareInfo + SrCmdAckObs,
 * so it must not live as a local on the 2048 B worker stack; the worker owns a single
 * heap instance. Same as SrLine (552 B) / SrEvent (240 B), see ADR-009.
 */
typedef struct {
    uint32_t lines_seen;
    uint32_t lines_malformed;
    bool in_session; /* Saw Starting, has not yet seen Stopping */
    /* T2.3b / ADR-013: gpsdata block accumulation. */
    bool in_gps_block;
    SrGpsSnapshot gps_partial;
    /* T3.3 / ADR-016: incremental accumulation of the info rows. Same pattern as gps_partial. */
    SrFirmwareInfo fw_partial;
    /* T4.9 / ADR-022 decision 2: L2 observation. Side-channel bookkeeping; changes no event output. */
    SrCmdAckObs cmdack;
} SrParser;

/*
 * Copy src into dst[cap]. cap includes the NUL slot.
 * Truncates if the source is longer; always NUL-terminates when cap > 0.
 * Returns strlen(src) (the untruncated length), matching POSIX strlcpy.
 */
static inline size_t sr_strlcpy(char* dst, size_t cap, const char* src) {
    size_t n = 0;
    if(src != NULL) {
        while(src[n] != '\0') {
            n++;
        }
    }
    if(dst != NULL && cap > 0) {
        size_t copy = n;
        if(copy > cap - 1U) {
            copy = cap - 1U;
        }
        if(src != NULL && copy > 0) {
            memcpy(dst, src, copy);
        }
        dst[copy] = '\0';
    }
    return n;
}
