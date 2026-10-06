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
#define FLOCK_STORE_SCHEMA "# FlipDeFlock hits v4"

/** The v1 marker, still accepted on READ. v2 only APPENDS columns (`class`,
 *  `hidden`) and reorders nothing, so a v1 record is a complete v2 record minus
 *  its last two fields -- and 0/0 is exactly what those fields held for every v1
 *  detection: an ALPR, hidden-SSID never observed. Rejecting the file instead
 *  would silently bin a user's whole detection history on upgrade, which is a
 *  worse outcome than carrying one compatibility branch. */
#define FLOCK_STORE_SCHEMA_V1 "# FlipDeFlock hits v1"

/** The v2 marker, still accepted on READ. Same append-only rule as v1: v3 adds a
 *  trailing `label` column and widens the MEANING of `marked` from 0/1 to a bit
 *  field, so a v2 record is a complete v3 record with no label and no confirmed
 *  bit -- exactly what a v2 file meant.
 *
 *  The bit field is why `confirmed` did not get a column of its own. An older
 *  build rejects any line whose column count it does not expect, but it reads
 *  `marked` with `!= 0`, so a row that is marked, or confirmed, or both, still
 *  reads as "marked" there instead of vanishing. The label is the one thing an
 *  older build genuinely cannot carry. */
#define FLOCK_STORE_SCHEMA_V2 "# FlipDeFlock hits v2"

/** `marked` column bits. Bit 0 keeps the original meaning so the column stays
 *  truthy for an old reader; bit 1 is the visual confirmation. */
#define FLOCK_STORE_MARK_REPORT    (1u << 0)
#define FLOCK_STORE_MARK_CONFIRMED (1u << 1)

/** The v3 marker, still accepted on READ. v4 appends the three Remote ID
 *  columns and reorders nothing, so a v3 record is a complete v4 record minus
 *  fields that were never an aircraft's anyway. */
#define FLOCK_STORE_SCHEMA_V3 "# FlipDeFlock hits v3"

/** Column header, written as the second line for anyone opening the file. */
#define FLOCK_STORE_HEADER                                                             \
    "mac,ssid,rssi,channel,ftype,conf,ie_fp,lat,lon,heading,count,marked,epoch,class," \
    "hidden,label,op_lat,op_lon,ua_type"

/** Number of comma-separated columns in a v4 record line. */
#define FLOCK_STORE_COLS 19

/**
 * Columns in a v3 record line (v4 minus the three Remote ID columns).
 *
 * v4 appends `op_lat`, `op_lon` and `ua_type` -- the OPERATOR's position and the
 * aircraft type from an ASTM F3411 broadcast. They are STORED rather than
 * re-derived because they cannot be re-derived: a drone is heard once, in
 * passing, and the pilot's location is the single most useful field in the row.
 * Before this, a saved drone reloaded with those fields ZEROED, and 0/0 is a
 * real point in the Gulf of Guinea -- the detail screen duly rendered
 * "Pilot lat: 0.00000" as though it were a fix.
 */
#define FLOCK_STORE_COLS_V3 16

/** Columns in a v2 record line (v3 minus the trailing `label`). */
#define FLOCK_STORE_COLS_V2 15

/** Columns in a v1 record line (v2 minus the trailing `class` and `hidden`). */
#define FLOCK_STORE_COLS_V1 13

/**
 * True if `line` is a schema marker this build can read (v1..v4). Anything
 * else -- including a NEWER marker -- must make the caller ignore the file
 * whole, since a future format may reuse or reorder columns.
 */
bool flock_store_schema_supported(const char* line);

/** Matches RECON_SSID_LEN; kept independent so this module stays firmware-free. */
#define FLOCK_STORE_SSID_LEN 33

/** Operator-supplied label. Shorter than an SSID on purpose: it is typed on a
 *  Flipper keyboard and has to fit a 128 px row beside a tag and a signal. */
#define FLOCK_STORE_LABEL_LEN 25

/** Buffer size a caller must provide to flock_store_fmt_line(). Worst case is a
 *  33-char SSID of nothing but quotes (each doubled, plus the wrapping pair). */
#define FLOCK_STORE_LINE_MAX 256

/**
 * Every frame-type letter a detection can carry. ONE definition, because the
 * store's writer, the store's reader and the report formatter each used to
 * spell this out themselves, and when 'S' (community-signature match) was added
 * the formatter was missed: signature detections round-tripped through hits.csv
 * correctly and then printed as '?' in the false-positive report, the one
 * column that report exists to show.
 */
#define FLOCK_FTYPE_SET "PBROFLS"

/** POD mirror of the persisted subset of FlockEntry. Deliberately not FlockEntry
 *  itself: that type lives in the firmware-coupled recon_app_i.h, and copying
 *  through this struct is what keeps the format host-testable. */
typedef struct {
    uint8_t mac[6];
    char ssid[FLOCK_STORE_SSID_LEN];
    int8_t rssi;
    uint8_t channel;
    char ftype; /**< P/B/R/O/F/L/S, or 0 when unknown */
    uint8_t conf; /**< FlockConfidence rung, 0..4 */
    uint32_t ie_fp;
    float lat, lon, heading; /**< NAN when there was no fix */
    uint32_t count;
    bool marked; /**< tagged for the report (bit 0 of the `marked` column) */
    bool confirmed; /**< operator visually confirmed it (bit 1). False from v1/v2. */
    uint32_t epoch; /**< RTC Unix seconds at the last sighting. NOT a furi tick:
                      *  ticks are uptime-relative and meaningless after the
                      *  reboot this file exists to survive. */
    uint8_t dev_class; /**< FlockDevClass rung (0 = ALPR). Absent in a v1 file,
                         *  which is exactly the 0 default. */
    bool hidden; /**< the AP beacons but withholds its SSID. Absent in a v1
                   *  file, where the 0 default reads as "not observed". */
    char label[FLOCK_STORE_LABEL_LEN]; /**< the operator's own name for this
                                         *  device. NEVER overwrites `ssid`: what
                                         *  was observed on the air and what the
                                         *  operator calls it are different facts,
                                         *  and conflating them loses the evidence.
                                         *  Empty in a v1/v2 file. */
    /**
     * ASTM F3411 Remote ID, for FlockClassDrone rows. NAN / 0 when absent, which
     * is what every pre-v4 file and every non-aircraft row means.
     *
     * op_lat/op_lon are the OPERATOR's position -- the pilot, not the aircraft,
     * whose own position is in lat/lon. They must default to NAN and not 0: the
     * standard uses 0/0 as its "no value" marker and it is also a real place, so
     * a zeroed field renders as a confident fix in the middle of the ocean.
     */
    float op_lat, op_lon;
    uint8_t ua_type; /**< OdidUaType; 0 = unknown/not an aircraft */
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
 *
 * Raised to 3 in v0.77 for FlockClassGear (vendor-exclusive competitor hardware:
 * Ubicquia, Motorola Solutions, Verkada, Genetec, Avigilon). Note the asymmetry
 * this creates and accept it: a file written by v0.77 that contains a Gear
 * sighting loses exactly those lines if read back by an older build. That is the
 * documented fail-safe -- an old build refuses a class it cannot label rather
 * than guessing "ALPR" and printing "Flock" over a Motorola radio.
 */
/**
 * Highest device class a stored row may carry. Rows above it are REJECTED
 * OUTRIGHT by the parser, not clamped.
 *
 * KEEP THIS IN STEP WITH FlockDevClass. It was left at 3 when FlockClassDrone (4)
 * was added, and the failure was silent and total: a Remote ID drone detection
 * was written to hits.csv correctly, then dropped on the next load, so the whole
 * row vanished the moment the app restarted and never appeared in an export.
 * Nothing warned -- the row simply was not there.
 */
#define FLOCK_STORE_MAX_DEV_CLASS 4u /* FlockClassDrone */

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
