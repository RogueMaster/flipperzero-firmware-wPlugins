// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "gps_parser.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/** One hex nibble 0-15, or -1 if `c` is not a hex digit. */
static int nmea_hex(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

float nmea_to_decimal(const char* field, const char* dir) {
    if(!field || field[0] == '\0') return NAN;
    float raw = strtof(field, NULL);
    int deg = (int)(raw / 100.0f);
    float minutes = raw - (deg * 100.0f);
    float dec = deg + minutes / 60.0f;
    if(dir && (dir[0] == 'S' || dir[0] == 'W')) dec = -dec;
    return dec;
}

int nmea_tokenize(char* line, char** fields, int max) {
    int n = 0;
    char* p = line;
    if(max <= 0) return 0;
    fields[n++] = p;
    while(*p && n < max) {
        if(*p == ',') {
            *p = '\0';
            fields[n++] = p + 1;
        }
        p++;
    }
    return n;
}

bool gps_coord_sane(float lat, float lon) {
    if(isnan(lat) || isnan(lon)) return false;
    if(lat < -90.0f || lat > 90.0f || lon < -180.0f || lon > 180.0f) return false;
    // 0,0 is almost always a partial/garbled sentence, not a real fix.
    if(lat > -0.0001f && lat < 0.0001f && lon > -0.0001f && lon < 0.0001f) return false;
    return true;
}

bool nmea_parse_line(char* line, NmeaFix* out) {
    out->valid = false;
    out->lat = NAN;
    out->lon = NAN;
    out->sats = -1;
    out->has_course = false;
    out->course = 0.0f;

    if(line[0] != '$') return false;
    size_t len = strlen(line);
    if(len < 7) return false;

    // Verify + drop the "*hh" checksum: XOR of everything between '$' and '*'.
    // A noise-garbled sentence whose fields happen to land in range is rejected here.
    char* star = strchr(line, '*');
    if(star) {
        uint8_t sum = 0;
        for(const char* p = line + 1; p < star; p++)
            sum ^= (uint8_t)*p;
        int hi = nmea_hex(star[1]);
        int lo = hi < 0 ? -1 : nmea_hex(star[2]); // only read [2] when [1] is a hex digit
        if(hi < 0 || lo < 0 || (uint8_t)((hi << 4) | lo) != sum) return false; // bad checksum
        *star = '\0';
    }

    const char* type = line + 3; // skip "$GP"/"$GN"/...
    char* fields[20];
    int nf = nmea_tokenize(line, fields, 20);

    if(strncmp(type, "RMC", 3) == 0 && nf >= 7) {
        out->valid = (fields[2][0] == 'A');
        out->lat = nmea_to_decimal(fields[3], fields[4]);
        out->lon = nmea_to_decimal(fields[5], fields[6]);
        // Course over ground (deg) = RMC field 8, only meaningful on a valid fix.
        if(out->valid && nf >= 9 && fields[8][0]) {
            out->has_course = true;
            out->course = strtof(fields[8], NULL);
        }
        return true;
    } else if(strncmp(type, "GGA", 3) == 0 && nf >= 8) {
        int fixq = atoi(fields[6]);
        int sats = atoi(fields[7]);
        if(sats < 0) sats = 0;
        if(sats > 64) sats = 64; // clamp: don't let the device drive the count to nonsense
        out->sats = sats;
        out->lat = nmea_to_decimal(fields[2], fields[3]);
        out->lon = nmea_to_decimal(fields[4], fields[5]);
        out->valid = (fixq > 0);
        return true;
    } else if(strncmp(type, "GLL", 3) == 0 && nf >= 7) {
        out->valid = (fields[6][0] == 'A');
        out->lat = nmea_to_decimal(fields[1], fields[2]);
        out->lon = nmea_to_decimal(fields[3], fields[4]);
        return true;
    }
    return false;
}
