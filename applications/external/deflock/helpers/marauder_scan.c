// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "marauder_scan.h"
#include "esp_parser.h" // esp_hexval

#include <string.h>

/**
 * Try to read a "hh:hh:hh:hh:hh:hh" MAC starting at p.
 *
 * The companion path's compact-MAC parsing lives in esp_parser.c; this is the
 * colon-separated form that human-readable firmware output prints.
 */
static bool parse_mac_colon(const char* p, uint8_t mac[6]) {
    for(int i = 0; i < 6; i++) {
        int hi = esp_hexval(p[i * 3]);
        int lo = esp_hexval(p[i * 3 + 1]);
        if(hi < 0 || lo < 0) return false;
        if(i < 5 && p[i * 3 + 2] != ':') return false;
        mac[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

/**
 * Pull the network name out of a labelled line, into `out`.
 *
 * Covers scanap/sniffbeacon ("ESSID: "), sniffraw ("SSID: ") and sniffprobe
 * ("Requesting: "). Bounded to an SSID length (preserving embedded spaces) so
 * trailing fields on the line are not absorbed and a far-away "flock" token
 * cannot spuriously raise confidence.
 */
static bool extract_ssid(const char* line, char* out, size_t cap) {
    static const char* const labels[] = {"ESSID: ", "SSID: ", "Requesting: "};
    out[0] = '\0';
    for(size_t k = 0; k < sizeof(labels) / sizeof(labels[0]); k++) {
        const char* p = strstr(line, labels[k]);
        if(!p) continue;
        p += strlen(labels[k]);
        size_t i = 0;
        for(; i < cap - 1 && p[i] && p[i] != '\r' && p[i] != '\n'; i++) {
            out[i] = p[i];
        }
        out[i] = '\0';
        return true;
    }
    return false;
}

void marauder_scan_line(const char* line, MarauderScan* out) {
    memset(out, 0, sizeof(*out));
    if(!line) return;

    // Prefer the labelled SSID the firmware prints over a whole-line scan, so the
    // confidence is attributed to the actual network name and we can display it.
    out->have_ssid = extract_ssid(line, out->ssid, sizeof(out->ssid));
    FlockConfidence ssid_conf = flock_ssid_confidence(out->have_ssid ? out->ssid : line);

    size_t len = strlen(line);
    if(len < 17) return; // can't hold even one "hh:hh:hh:hh:hh:hh"

    // ONE pass over the line. The single-MAC attribution rule needs the total MAC
    // count before any hit can be scored, which used to mean scanning the whole
    // line twice -- and each scan attempts a 17-byte parse at EVERY byte offset,
    // on every line the board prints. Instead, collect the candidates as we go
    // and score them afterwards: mac_count is still exact, and the expensive
    // walk happens once.
    //
    // Only MARAUDER_MAX_HITS candidates are retained (a real line names one or
    // two); mac_count keeps counting past that so the attribution rule stays
    // correct, and the surplus is reported via `dropped`.
    uint8_t cand[MARAUDER_MAX_HITS][6];
    int cand_n = 0;
    for(size_t i = 0; i + 17 <= len; i++) {
        uint8_t mac[6];
        if(!parse_mac_colon(line + i, mac)) continue;
        out->mac_count++;
        if(cand_n < MARAUDER_MAX_HITS) {
            memcpy(cand[cand_n++], mac, 6);
        } else {
            out->dropped++;
        }
        i += 16;
    }

    // A line-wide SSID match can only be safely attributed to a specific MAC when
    // the line names exactly one; otherwise an unrelated "flock" substring would
    // promote every MAC on a multi-record log line.
    bool single = (out->mac_count == 1);

    for(int c = 0; c < cand_n; c++) {
        const uint8_t* mac = cand[c];

        // Either surveillance-vendor table; the class is derived from the OUI.
        bool oui = flock_oui_match(mac) || soundthinking_oui_match(mac);
        FlockConfidence conf;
        if(oui) {
            // OUI vendor prefix; SSID naming on the same line can raise it, but
            // an OUI alone never exceeds "possible" -- these are shared vendor
            // ranges, not Flock-exclusive ones.
            conf = (ssid_conf > FlockConfidencePossible) ? ssid_conf : FlockConfidencePossible;
        } else if(single && ssid_conf != FlockConfidenceNone) {
            // Sole MAC on a line that names a Flock SSID -> attribute to it.
            conf = ssid_conf;
        } else {
            continue;
        }

        MarauderHit* h = &out->hits[out->hit_count++];
        memcpy(h->mac, mac, 6);
        h->conf = conf;
        h->dev_class = flock_class_from_mac(mac);
    }
}
