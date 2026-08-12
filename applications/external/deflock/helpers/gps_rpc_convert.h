// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
/**
 * @file gps_rpc_convert.h
 * Pure conversion of an Unleashed RPC GPS location into the app's fix record.
 *
 * Split out of gps_rpc.c for the same reason gps_parser is split out of
 * gps_link: the firmware side needs <gps/gps.h>, which exists only in the
 * Unleashed SDK, so nothing that includes it can be compiled by the host tests.
 * Everything with a decision in it lives here instead, and takes plain integers
 * rather than a GpsLocation, so the ofw/Momentum builds and the host tests both
 * compile it unconditionally.
 *
 * No app/lock/firmware dependencies.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * Worst reported accuracy still treated as a usable fix, in millimetres (100 m).
 *
 * A phone's location provider is FUSED, not a GNSS receiver: with no sky view it
 * will happily return a cell-tower or Wi-Fi estimate, kilometres wide, reported
 * with exactly the same shape as a real satellite fix. The NMEA path cannot hit
 * this -- a receiver with no lock says so (RMC 'V') rather than guessing -- so
 * this gate is specific to the RPC source.
 *
 * Geotagging a camera 2 km from where it actually is is worse than leaving it
 * ungeotagged, which is the precision-over-recall rule applied to position. A
 * real outdoor GNSS fix is 3-10 m and an urban-canyon one rarely exceeds 50 m,
 * so 100 m separates "degraded but real" from "network-located guess" without
 * discarding usable fixes.
 */
#define GPS_RPC_ACCURACY_MAX_MM 100000u

/** One converted RPC location, in the units the app stores. */
typedef struct {
    bool valid; /**< usable as a current fix (sane coords AND accuracy within the gate) */
    float lat; /**< decimal degrees, signed */
    float lon; /**< decimal degrees, signed */
    bool has_course; /**< a meaningful course value is present -- see gps_rpc_convert() */
    float course; /**< course over ground, degrees 0..360 */
    int sats; /**< satellite count, or -1 when the companion did not report one */
} GpsRpcFix;

/**
 * Convert one RPC location to a GpsRpcFix.
 *
 * Field units are the ones gps.h documents: coordinates in degrees * 1e7,
 * heading in degrees * 100, speed in mm/s, accuracy in mm.
 *
 * Returns false (and leaves *out untouched) only for a NULL out. A location that
 * fails the sanity or accuracy gates still returns true with `valid` false, so
 * the caller can tell "the phone answered with something unusable" from "the
 * phone did not answer", which are different faults with different fixes.
 *
 * Two protobuf-shaped ambiguities are resolved conservatively, because an unset
 * field and a genuine zero are indistinguishable on the wire:
 *
 *  - `satellites == 0` becomes sats -1 ("unknown"), not zero. A phone's fused
 *    provider usually cannot report a satellite count at all, and a badge
 *    reading "GPS 0" next to a valid fix reads as a failure.
 *  - `heading == 0` is only trusted when `speed` is non-zero. A stationary phone
 *    reports no bearing, which arrives as 0 and is indistinguishable from due
 *    north; tagging every stationary detection as north-facing is a confident
 *    wrong answer, and heading is recorded per detection.
 *
 * `has_course` additionally implies `valid`: a position that failed the gates
 * leaves no heading behind it either.
 */
bool gps_rpc_convert(
    int32_t latitude_e7,
    int32_t longitude_e7,
    uint32_t heading_centideg,
    uint32_t speed_mm_s,
    uint32_t accuracy_mm,
    uint32_t satellites,
    GpsRpcFix* out);
