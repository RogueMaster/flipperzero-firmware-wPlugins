// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "gps_rpc_convert.h"
#include "gps_parser.h"

/**
 * Scaled integer -> degrees, in single precision, without losing the low digits.
 *
 * THE OBVIOUS `(float)scaled / 1e7f` IS WRONG. A coordinate at 1e7 scale reaches
 * ~9e8, which needs 30 bits of mantissa; float has 24. So the int->float
 * conversion rounds the input by up to +-16 counts BEFORE the divide and bakes
 * that in -- ~1.7 m at the equator, on a value the app then stores to ~0.4 m
 * float resolution. An error four times the storage precision, for nothing.
 *
 * Splitting first fixes it in single precision. The whole degrees are exact (at
 * most 180), and the remainder is under 1e7, so BOTH it and the 1e7 divisor are
 * below 2^24 and convert to float exactly -- leaving one rounding on the divide
 * and one on the add, which is the best float can do.
 *
 * Done this way rather than in double on purpose: the STM32WB has no double FPU,
 * the SDK builds with -Werror=double-promotion to keep soft-float arithmetic out
 * of the image, and this app is already budgeting against ~256 KB. Correct to the
 * same precision, with no __aeabi_ddiv pulled in behind it.
 */
static float gps_rpc_deg_from_e7(int32_t scaled) {
    int32_t whole = scaled / 10000000;
    // C99: the remainder takes the sign of the dividend, so a southern latitude or
    // western longitude stays negative in both halves and the sum is still correct.
    int32_t frac = scaled % 10000000;
    return (float)whole + (float)frac / 1e7f;
}

bool gps_rpc_convert(
    int32_t latitude_e7,
    int32_t longitude_e7,
    uint32_t heading_centideg,
    uint32_t speed_mm_s,
    uint32_t accuracy_mm,
    uint32_t satellites,
    GpsRpcFix* out) {
    if(!out) return false;

    out->lat = gps_rpc_deg_from_e7(latitude_e7);
    out->lon = gps_rpc_deg_from_e7(longitude_e7);

    // Same sanity rule the NMEA path uses -- range plus the (0,0) null-island
    // artifact -- so a garbled fix is rejected identically whichever source it
    // came in on. INT32_MIN lands at -214.7 degrees and is caught here rather
    // than needing a special case (negating it would be undefined).
    bool sane = gps_coord_sane(out->lat, out->lon);

    // accuracy 0 means "not reported", not "perfect": an unset protobuf uint32
    // is 0. The NMEA source carries no accuracy at all and is trusted on its own
    // validity flag, so an unreported accuracy is treated the same way rather
    // than rejecting every companion that omits the field.
    bool accurate = (accuracy_mm == 0) || (accuracy_mm <= GPS_RPC_ACCURACY_MAX_MM);

    out->valid = sane && accurate;

    // gps.h documents 0..36000; anything above that is a malformed heading and
    // is dropped rather than wrapped into a plausible-looking direction.
    //
    // Gated on `valid` as well, so a rejected position cannot leave a heading
    // behind it. The NMEA path gets this for free -- RMC only carries a course on
    // a valid sentence -- and the RPC path has to state it, because the two
    // arrive as independent fields. A heading derived from a 3 km fused estimate
    // is not a bearing worth keeping.
    bool heading_in_range = heading_centideg <= 36000u;
    out->has_course = out->valid && heading_in_range &&
                      (heading_centideg != 0u || speed_mm_s != 0u);
    out->course = out->has_course ? (float)heading_centideg / 100.0f : 0.0f;

    // 0 = "did not report", which is the normal case for a phone's fused
    // location provider. -1 tells the publisher to leave the last count alone.
    out->sats = (satellites == 0u) ? -1 : (int)(satellites > 64u ? 64u : satellites);

    return true;
}
