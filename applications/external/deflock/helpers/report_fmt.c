// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "report_fmt.h"

#include <math.h>
#include <stdio.h>

void fmt_mac(char* out, size_t out_len, const uint8_t mac[6]) {
    snprintf(
        out,
        out_len,
        "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]);
}

void fmt_coord(char* out, size_t out_len, float value, const char* fallback) {
    if(isnan(value)) {
        snprintf(out, out_len, "%s", fallback);
    } else {
        snprintf(out, out_len, "%.6f", (double)value);
    }
}

void fmt_mac_oui(char* out, size_t out_len, const uint8_t mac[6]) {
    snprintf(out, out_len, "%02X:%02X:%02X:xx:xx:xx", mac[0], mac[1], mac[2]);
}

void fmt_ssid_shape(char* out, size_t out_len, const char* ssid) {
    if(!out || out_len == 0) return;
    if(!ssid || ssid[0] == '\0') {
        snprintf(out, out_len, "(none)");
        return;
    }
    size_t o = 0;
    for(size_t i = 0; ssid[i] != '\0' && o + 1 < out_len; i++) {
        unsigned char c = (unsigned char)ssid[i];
        char m;
        // Deliberately NOT ctype.h: isupper() and friends are locale-dependent
        // and, on a signed char, undefined for bytes over 127. An SSID is 32
        // bytes of ARBITRARY data and routinely holds UTF-8 or junk, so the
        // classification has to be explicit ASCII or it is not a guarantee.
        if(c >= 'A' && c <= 'Z') {
            m = 'A';
        } else if(c >= 'a' && c <= 'z') {
            m = 'a';
        } else if(c >= '0' && c <= '9') {
            m = 'd';
        } else if(c == '-' || c == '_' || c == '.') {
            m = (char)c;
        } else {
            m = '?';
        }
        out[o++] = m;
    }
    out[o] = '\0';
}

int fmt_signal_level(int rssi) {
    if(rssi == 0) return -1; // unknown
    if(rssi >= -50) return 4;
    if(rssi >= -62) return 3;
    if(rssi >= -74) return 2;
    if(rssi >= -86) return 1;
    return 1; // very weak but present -- never 0, so a real reading always shows
}
