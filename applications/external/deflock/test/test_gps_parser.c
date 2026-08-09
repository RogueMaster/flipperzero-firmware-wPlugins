// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
//
// NMEA parser fixtures. Locks in the shipped GPS fixes: B10 (checksum verified
// before a sentence is trusted), B23 (satellite count clamped), B7 (a 'V'/void
// or fixq==0 sentence reports no fix -> the adapter clears the stale location),
// plus the ddmm.mmmm -> decimal-degree conversion and coordinate sanity gate.
#include "gps_parser.h"
#include "test.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

static bool nearf(float a, float b, float tol) {
    return fabsf(a - b) <= tol;
}

// Parse a copy of `s` (parser is destructive) into `out`; returns recognized.
static bool parse(const char* s, NmeaFix* out) {
    char buf[128];
    size_t n = strlen(s);
    if(n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, s, n);
    buf[n] = '\0';
    return nmea_parse_line(buf, out);
}

// Append the correct "*hh" NMEA checksum to `body` ("$GP..." without it).
static void with_cksum(const char* body, char* out, size_t n) {
    uint8_t sum = 0;
    for(const char* p = body + 1; *p; p++)
        sum ^= (uint8_t)*p;
    snprintf(out, n, "%s*%02X", body, sum);
}

void suite_gps_parser(void) {
    printf("[gps_parser]\n");
    NmeaFix fix;

    // --- nmea_to_decimal ----------------------------------------------------
    CHECK(nearf(nmea_to_decimal("4807.038", "N"), 48.1173f, 0.001f)); // 48 deg 07.038'
    CHECK(nearf(nmea_to_decimal("4807.038", "S"), -48.1173f, 0.001f)); // southern -> negative
    CHECK(nearf(nmea_to_decimal("01131.000", "E"), 11.5167f, 0.001f));
    CHECK(nearf(nmea_to_decimal("01131.000", "W"), -11.5167f, 0.001f));
    CHECK(isnan(nmea_to_decimal("", "N"))); // empty field -> NAN
    CHECK(isnan(nmea_to_decimal(NULL, NULL)));

    // --- gps_coord_sane -----------------------------------------------------
    CHECK(gps_coord_sane(48.1f, 11.5f));
    CHECK(gps_coord_sane(-89.0f, -179.0f));
    CHECK(!gps_coord_sane(NAN, 11.0f));
    CHECK(!gps_coord_sane(91.0f, 0.0f)); // lat out of range
    CHECK(!gps_coord_sane(0.0f, 181.0f)); // lon out of range
    CHECK(!gps_coord_sane(0.0f, 0.0f)); // null island garble
    CHECK(!gps_coord_sane(0.00005f, 0.00005f)); // near null island

    // --- nmea_tokenize ------------------------------------------------------
    {
        char s[] = "a,b,c";
        char* f[4];
        CHECK_INT_EQ(nmea_tokenize(s, f, 4), 3);
        CHECK_STR_EQ(f[2], "c");
    }

    // --- RMC: valid fix + course -------------------------------------------
    CHECK(parse("$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W", &fix));
    CHECK(fix.valid);
    CHECK(nearf(fix.lat, 48.1173f, 0.001f));
    CHECK(nearf(fix.lon, 11.5167f, 0.001f));
    CHECK(fix.has_course);
    CHECK(nearf(fix.course, 84.4f, 0.01f));
    CHECK_INT_EQ(fix.sats, -1); // RMC doesn't report satellite count

    // B7: an RMC with status 'V' (void) reports NO fix -> the adapter clears the
    // stale location instead of tagging detections with it.
    CHECK(parse("$GPRMC,123519,V,4807.038,N,01131.000,E,,,230394,,,N", &fix));
    CHECK(!fix.valid);
    CHECK(!fix.has_course); // no course on a void fix

    // --- GGA: fix quality + satellite count --------------------------------
    CHECK(parse("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,", &fix));
    CHECK(fix.valid); // fix quality 1 > 0
    CHECK_INT_EQ(fix.sats, 8);
    CHECK(nearf(fix.lat, 48.1173f, 0.001f));

    // B23: a bogus satellite count is clamped to a sane max, not shown as nonsense.
    CHECK(parse("$GPGGA,123519,4807.038,N,01131.000,E,1,999,0.9,545.4,M,,,,", &fix));
    CHECK_INT_EQ(fix.sats, 64);

    // GGA with fix quality 0 -> no fix (B7 companion of the RMC 'V' case).
    CHECK(parse("$GPGGA,123519,4807.038,N,01131.000,E,0,00,,,,,,,", &fix));
    CHECK(!fix.valid);

    // --- GLL ----------------------------------------------------------------
    CHECK(parse("$GPGLL,4807.038,N,01131.000,E,123519,A", &fix));
    CHECK(fix.valid);
    CHECK(nearf(fix.lat, 48.1173f, 0.001f));
    CHECK(parse("$GPGLL,4807.038,N,01131.000,E,123519,V", &fix)); // void
    CHECK(!fix.valid);

    // --- checksum (B10) + junk ---------------------------------------------
    char line[128], buf[128];
    with_cksum("$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394", line, sizeof(line));
    strcpy(buf, line);
    CHECK(nmea_parse_line(buf, &fix)); // correct checksum -> accepted
    CHECK(fix.valid);
    strcpy(buf, line);
    buf[1] = 'X'; // corrupt a body byte -> stored checksum no longer matches
    CHECK(!nmea_parse_line(buf, &fix)); // bad checksum -> rejected

    CHECK(!parse("$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K", &fix)); // unknown sentence
    CHECK(!parse("hello", &fix)); // not NMEA
    CHECK(!parse("$GP", &fix)); // too short
}
