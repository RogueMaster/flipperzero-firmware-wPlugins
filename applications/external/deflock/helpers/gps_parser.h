// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
/**
 * @file gps_parser.h
 * Pure NMEA sentence parsing (RMC / GGA / GLL): line text -> plain NmeaFix record.
 *
 * Extracted from gps_link.c so the parsing is host-testable (parse != mutate,
 * mirroring esp_parser). gps_link.c owns the thin adapter that applies a parsed
 * NmeaFix to ReconApp under the lock. No app/lock/firmware dependencies.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/** One parsed NMEA position/status sentence. */
typedef struct {
    bool valid; /**< sentence reports a valid fix (RMC/GLL status 'A', GGA fix quality > 0) */
    float lat; /**< decimal degrees, signed (NAN if the field was empty) */
    float lon; /**< decimal degrees, signed (NAN if empty) */
    int sats; /**< satellite count (GGA), clamped 0..64; -1 if this sentence doesn't report it */
    bool has_course; /**< a course-over-ground value is present (RMC, only when valid) */
    float course; /**< course over ground, degrees */
} NmeaFix;

/** NMEA ddmm.mmmm + hemisphere ("N"/"S"/"E"/"W") -> signed decimal degrees (NAN if empty). */
float nmea_to_decimal(const char* field, const char* dir);

/** Reject NaN, out-of-range, and the (0,0) "null island" garble artifact. */
bool gps_coord_sane(float lat, float lon);

/** Split `line` in place into up to `max` comma-separated fields; returns the count. */
int nmea_tokenize(char* line, char** fields, int max);

/**
 * Parse one NMEA line (RMC/GGA/GLL) into `out`. DESTRUCTIVE on `line`. If a "*hh"
 * checksum is present it is XOR-validated and the line is dropped on mismatch.
 * Returns true only for a recognized, well-formed sentence (false on unknown
 * type, too few fields, or bad checksum). Pure: no app/lock/firmware calls.
 */
bool nmea_parse_line(char* line, NmeaFix* out);
