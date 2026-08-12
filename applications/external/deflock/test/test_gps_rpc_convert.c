// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
//
// Phone-GPS (Unleashed RPC location service) conversion fixtures.
//
// Everything with a decision in it lives in gps_rpc_convert.c precisely so it can
// be driven from here: gps_rpc.c itself includes <gps/gps.h>, which exists only in
// the Unleashed SDK, so it cannot be host-compiled on any machine. What is pinned
// below is therefore the whole of the source's judgement -- the accuracy gate, the
// two protobuf zero-ambiguities, and the coordinate precision -- rather than a
// helper the shipped path happens not to call.
#include "gps_rpc_convert.h"
#include "test.h"

#include <math.h>

static bool nearf(float a, float b, float tol) {
    return fabsf(a - b) <= tol;
}

// A clean outdoor fix: Portland OR, 4.2 m accuracy, moving north-east at 5 m/s.
#define OK_LAT  455230000 // 45.5230 N
#define OK_LON  -1226520000 // 122.6520 W
#define OK_ACC  4200u // mm
#define OK_SPD  5000u // mm/s
#define OK_HDG  4500u // 45.00 deg
#define OK_SATS 9u

void suite_gps_rpc_convert(void) {
    printf("[gps_rpc_convert]\n");
    GpsRpcFix f;

    // --- the ordinary case -------------------------------------------------
    CHECK(gps_rpc_convert(OK_LAT, OK_LON, OK_HDG, OK_SPD, OK_ACC, OK_SATS, &f));
    CHECK(f.valid);
    CHECK(nearf(f.lat, 45.5230f, 0.00001f));
    CHECK(nearf(f.lon, -122.6520f, 0.00001f));
    CHECK(f.has_course);
    CHECK(nearf(f.course, 45.0f, 0.001f));
    CHECK_INT_EQ(f.sats, 9);

    // NULL out is the only hard failure.
    CHECK(!gps_rpc_convert(OK_LAT, OK_LON, OK_HDG, OK_SPD, OK_ACC, OK_SATS, NULL));

    // --- precision: the divide must not happen in float ---------------------
    // 45.5230123 deg = 455230123e-7, which needs 29 bits; a float mantissa holds
    // 24. Converting the INTEGER to float first rounds it by up to +-16 counts
    // (~1.7 m) before the divide, on a value the app then stores to ~0.4 m float
    // resolution. This asserts the result is good to 1e-6 deg (~0.11 m), which a
    // float-first implementation cannot reach.
    CHECK(gps_rpc_convert(455230123, -1226520987, 0, 0, OK_ACC, OK_SATS, &f));
    CHECK(nearf(f.lat, 45.5230123f, 0.000001f));
    CHECK(nearf(f.lon, -122.6520987f, 0.000001f));

    // --- the accuracy gate --------------------------------------------------
    // A phone answers with a fused estimate whether or not it has sky view. This
    // is the gate that keeps a cell-tower guess from geotagging a camera; it has
    // no NMEA equivalent, because a receiver with no lock reports no fix at all.
    CHECK(gps_rpc_convert(OK_LAT, OK_LON, 0, 0, GPS_RPC_ACCURACY_MAX_MM, OK_SATS, &f));
    CHECK(f.valid); // exactly at the limit is still usable
    CHECK(gps_rpc_convert(OK_LAT, OK_LON, 0, 0, GPS_RPC_ACCURACY_MAX_MM + 1, OK_SATS, &f));
    CHECK(!f.valid); // one millimetre past it is not
    CHECK(gps_rpc_convert(OK_LAT, OK_LON, 0, 0, 3000000u, OK_SATS, &f)); // 3 km: cell tower
    CHECK(!f.valid);
    // ...but the coordinates are still converted, so the caller can tell "answered
    // with something unusable" from "did not answer". Those need different fixes.
    CHECK(nearf(f.lat, 45.5230f, 0.00001f));

    // accuracy 0 means "not reported" (unset protobuf field), not "perfect".
    // Rejecting it would reject every companion that omits the field.
    CHECK(gps_rpc_convert(OK_LAT, OK_LON, 0, 0, 0u, OK_SATS, &f));
    CHECK(f.valid);

    // --- coordinate sanity, shared with the NMEA path -----------------------
    CHECK(gps_rpc_convert(0, 0, 0, 0, OK_ACC, OK_SATS, &f));
    CHECK(!f.valid); // null island
    CHECK(gps_rpc_convert(910000000, 0, 0, 0, OK_ACC, OK_SATS, &f));
    CHECK(!f.valid); // 91 N is out of range
    CHECK(gps_rpc_convert(OK_LAT, 1810000000, 0, 0, OK_ACC, OK_SATS, &f));
    CHECK(!f.valid); // 181 E is out of range
    // INT32_MIN is the interesting one: it lands at -214.7 deg and must be caught
    // by the range check rather than needing a negation that would be undefined.
    CHECK(gps_rpc_convert(INT32_MIN, INT32_MIN, 0, 0, OK_ACC, OK_SATS, &f));
    CHECK(!f.valid);

    // --- satellites: 0 is "unknown", not zero -------------------------------
    // A phone's fused provider generally cannot report a count. -1 tells the
    // publisher to leave the previous value alone, which is what keeps a valid
    // fix from rendering as a filled "GPS 0" badge.
    CHECK(gps_rpc_convert(OK_LAT, OK_LON, 0, 0, OK_ACC, 0u, &f));
    CHECK(f.valid);
    CHECK_INT_EQ(f.sats, -1);
    CHECK(gps_rpc_convert(OK_LAT, OK_LON, 0, 0, OK_ACC, 1u, &f));
    CHECK_INT_EQ(f.sats, 1); // a real count of 1 is not "unknown"
    CHECK(gps_rpc_convert(OK_LAT, OK_LON, 0, 0, OK_ACC, 9999u, &f));
    CHECK_INT_EQ(f.sats, 64); // clamped, as the NMEA path clamps (B23)

    // --- heading: 0 is only north when you are moving -----------------------
    // Heading is recorded per detection, so a stationary phone -- which reports no
    // bearing, arriving as 0 -- must not tag every sighting as north-facing.
    CHECK(gps_rpc_convert(OK_LAT, OK_LON, 0u, 0u, OK_ACC, OK_SATS, &f));
    CHECK(!f.has_course); // stopped, heading 0 -> unknown
    CHECK(gps_rpc_convert(OK_LAT, OK_LON, 0u, 1u, OK_ACC, OK_SATS, &f));
    CHECK(f.has_course); // moving, heading 0 -> genuinely due north
    CHECK(nearf(f.course, 0.0f, 0.001f));
    CHECK(gps_rpc_convert(OK_LAT, OK_LON, 18000u, 0u, OK_ACC, OK_SATS, &f));
    CHECK(f.has_course); // stopped but a bearing IS reported -> trust it
    CHECK(nearf(f.course, 180.0f, 0.001f));
    CHECK(gps_rpc_convert(OK_LAT, OK_LON, 36000u, OK_SPD, OK_ACC, OK_SATS, &f));
    CHECK(f.has_course); // 360.00 is the documented top of the range
    CHECK(nearf(f.course, 360.0f, 0.001f));
    CHECK(gps_rpc_convert(OK_LAT, OK_LON, 36001u, OK_SPD, OK_ACC, OK_SATS, &f));
    CHECK(!f.has_course); // past it: malformed, dropped rather than wrapped
    CHECK(gps_rpc_convert(OK_LAT, OK_LON, 0xFFFFFFFFu, OK_SPD, OK_ACC, OK_SATS, &f));
    CHECK(!f.has_course);
    // A dropped heading must not drop the position with it.
    CHECK(f.valid);

    // --- an unusable fix yields no course either ----------------------------
    // A 3 km fused estimate with a perfectly plausible 90.00 deg heading. The
    // position is refused, so the bearing must go with it -- otherwise the caller
    // can still record a heading for a place it declined to record. The NMEA path
    // gets this free (RMC carries no course on an invalid sentence); here the two
    // fields arrive independently, so it has to be asserted.
    CHECK(gps_rpc_convert(OK_LAT, OK_LON, 9000u, OK_SPD, 3000000u, OK_SATS, &f));
    CHECK(!f.valid);
    CHECK(!f.has_course);
    // Same for a coordinate that fails the sanity gate rather than the accuracy one.
    CHECK(gps_rpc_convert(0, 0, 9000u, OK_SPD, OK_ACC, OK_SATS, &f));
    CHECK(!f.valid);
    CHECK(!f.has_course);
}
