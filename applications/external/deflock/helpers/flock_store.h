// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
/**
 * @file flock_store.h
 * On-disk record format for persisted Flock/ALPR detections (`hits.csv`).
 *
 * Detections used to live only in RAM, so closing the app threw them away
 * (GitHub issue #2 -- two hits, back out to the Flipper menu, both gone). This
 * module is the format half: one detection <-> one CSV line, and the rule for
 * which stale entry to evict when the table is full.
 *
 * Pure (libc + the equally-pure report_escape / report_fmt helpers), no firmware
 * dependencies, so the round trip is host-testable. The file I/O that uses it
 * lives in recon_app.c next to the settings load/save.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Schema marker written as the file's first line. A future format change bumps
 *  the version; a line the loader does not recognise means "ignore this file",
 *  never "parse it anyway and get the columns wrong". */
#define FLOCK_STORE_SCHEMA "# FlipDeFlock hits v2"

/** The v1 marker, still accepted on READ. v2 only APPENDS columns (`class`,
 *  `hidden`) and reorders nothing, so a v1 record is a complete v2 record minus
 *  its last two fields -- and 0/0 is exactly what those fields held for every v1
 *  detection: an ALPR, hidden-SSID never observed. Rejecting the file instead
 *  would silently bin a user's whole detection history on upgrade, which is a
 *  worse outcome than carrying one compatibility branch. */
#define FLOCK_STORE_SCHEMA_V1 "# FlipDeFlock hits v1"

/** Column header, written as the second line for anyone opening the file. */
#define FLOCK_STORE_HEADER \
    "mac,ssid,rssi,channel,ftype,conf,ie_fp,lat,lon,heading,count,marked,epoch,class,hidden"

/** Number of comma-separated columns in a v2 record line. */
#define FLOCK_STORE_COLS 15

/** Columns in a v1 record line (v2 minus the trailing `class` and `hidden`). */
#define FLOCK_STORE_COLS_V1 13

/**
 * True if `line` is a schema marker this build can read (v1 or v2). Anything
 * else -- including a NEWER marker -- must make the caller ignore the file
 * whole, since a future format may reuse or reorder columns.
 */
bool flock_store_schema_supported(const char* line);

/** Matches RECON_SSID_LEN; kept independent so this module stays firmware-free. */
#define FLOCK_STORE_SSID_LEN 33

/** Buffer size a caller must provide to flock_store_fmt_line(). Worst case is a
 *  33-char SSID of nothing but quotes (each doubled, plus the wrapping pair). */
#define FLOCK_STORE_LINE_MAX 192

/** POD mirror of the persisted subset of FlockEntry. Deliberately not FlockEntry
 *  itself: that type lives in the firmware-coupled recon_app_i.h, and copying
 *  through this struct is what keeps the format host-testable. */
typedef struct {
    uint8_t mac[6];
    char ssid[FLOCK_STORE_SSID_LEN];
    int8_t rssi;
    uint8_t channel;
    char ftype; /**< P/B/R/O/F/L, or 0 when unknown */
    uint8_t conf; /**< FlockConfidence rung, 0..4 */
    uint32_t ie_fp;
    float lat, lon, heading; /**< NAN when there was no fix */
    uint32_t count;
    bool marked;
    uint32_t epoch; /**< RTC Unix seconds at the last sighting. NOT a furi tick:
                      *  ticks are uptime-relative and meaningless after the
                      *  reboot this file exists to survive. */
    uint8_t dev_class; /**< FlockDevClass rung (0 = ALPR). Absent in a v1 file,
                         *  which is exactly the 0 default. */
    bool hidden; /**< the AP beacons but withholds its SSID. Absent in a v1
                   *  file, where the 0 default reads as "not observed". */
} FlockStoreRec;

/**
 * Highest FlockDevClass value this build will accept out of a stored line.
 *
 * MUST TRACK FlockDevClass in flock_db.h. This header stays dependency-free on
 * purpose -- it is the on-disk POD, host-tested without the firmware or the
 * detection tables -- so the enum cannot simply be included here, and the bound
 * is restated instead.
 *
 * Get it wrong and the failure is quiet and total: the parser rejects the whole
 * LINE, so a detection of the new class does not come back with a wrong label,
 * it does not come back at all. Adding FlockClassBodycam without raising this
 * would have silently dropped every stored Axon sighting on load.
 */
#define FLOCK_STORE_MAX_DEV_CLASS 2u /* FlockClassBodycam */

/**
 * Format one record as a CSV line, including the trailing newline. Returns the
 * number of bytes written, or 0 if the record could not be formatted within
 * `out_len` (in which case `out` holds no usable line).
 */
size_t flock_store_fmt_line(char* out, size_t out_len, const FlockStoreRec* r);

/**
 * Parse one CSV line back into a record. Tolerates a trailing LF or CRLF.
 * Returns false -- leaving `*out` untouched -- for anything malformed: a wrong
 * column count, a bad MAC, an unterminated quote, or an out-of-range enum. A bad
 * line is skipped by the caller, never fatal (same fail-safe posture as sig_db).
 */
bool flock_store_parse_line(const char* line, FlockStoreRec* out);

/**
 * Eviction ordering for a full detection table: is candidate A a better entry to
 * drop than the current best candidate B? Lowest confidence goes first (a
 * "Possible" lead is worth less than a "Confirmed" camera), oldest breaks a tie.
 *
 * The caller loops over its own array and applies this only to archived entries,
 * so a live detection is never evicted for a stored one.
 */
bool flock_store_evict_better(uint8_t conf_a, uint32_t epoch_a, uint8_t conf_b, uint32_t epoch_b);
