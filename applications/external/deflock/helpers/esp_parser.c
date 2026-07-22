// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "esp_parser.h"

#include <stdlib.h>
#include <string.h>

int esp_hexval(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/** Parse "aabbccddeeff" (no separators, 12 hex chars) into 6 bytes. */
static bool parse_mac_compact(const char* s, uint8_t mac[6]) {
    for(int i = 0; i < 6; i++) {
        int hi = esp_hexval(s[i * 2]);
        int lo = esp_hexval(s[i * 2 + 1]);
        if(hi < 0 || lo < 0) return false;
        mac[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

static FlockConfidence conf_from_int(int c) {
    switch(c) {
    case 3:
        return FlockConfidenceConfirmed;
    case 2:
        return FlockConfidenceLikely;
    case 1:
        return FlockConfidencePossible;
    default:
        return FlockConfidenceNone;
    }
}

int esp_split_fields(char* line, char** fields, int max) {
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

/** Fill out->u.flock from a split D-line (f[0..n]); returns the (possibly
 *  fingerprint-upgraded) message type. Mirrors the original inline logic. */
static EspMsgType parse_flock(char** f, int n, EspMsg* out) {
    if(n < 6) return EspMsgIgnore;
    uint8_t mac[6];
    if(strlen(f[1]) < 12 || !parse_mac_compact(f[1], mac)) return EspMsgIgnore;

    int8_t rssi = (int8_t)atoi(f[2]);
    uint8_t ch = (uint8_t)atoi(f[3]);
    char ftype = f[4][0] ? f[4][0] : 'O';
    FlockConfidence conf = conf_from_int(atoi(f[5]));
    const char* ssid = (n >= 7) ? f[6] : "";

    // B1: trailing IE-fingerprint field "fp=<hex32>" (probe requests only). Older
    // firmware omits it. Start at f[7] (AFTER the ssid at f[6]) so an SSID that
    // literally begins "fp=" can't be misread as the IE-fingerprint.
    uint32_t fp = 0;
    for(int i = 7; i < n; i++) {
        if(strncmp(f[i], "fp=", 3) == 0) {
            fp = (uint32_t)strtoul(f[i] + 3, NULL, 16);
            break;
        }
    }
    FlockIeFp fp_src = flock_ie_fp_match(fp);
    if(fp_src == FlockIeFpBuiltin) {
        // Verified compiled-in class fp. + Flock OUI -> CONFIRMED; otherwise (e.g. a
        // wildcard probe from a randomized/unknown MAC) -> a candidate device-CLASS
        // match. Never weaker than the ESP's own score.
        FlockConfidence fp_conf = flock_oui_match(mac) ? FlockConfidenceConfirmed :
                                                         FlockConfidenceProbeFp;
        if(fp_conf > conf) conf = fp_conf;
        ftype = 'F'; // source label "probe-fp" in the detail scene
    } else if(fp_src == FlockIeFpUser) {
        // UNVERIFIED user fp (signatures.json): a candidate device-CLASS match ONLY
        // -- capped at "Class?", never Confirmed even with a Flock OUI.
        if(FlockConfidenceProbeFp > conf) conf = FlockConfidenceProbeFp;
        ftype = 'F';
    }

    memcpy(out->u.flock.mac, mac, 6);
    out->u.flock.ssid = ssid;
    out->u.flock.rssi = rssi;
    out->u.flock.channel = ch;
    out->u.flock.ftype = ftype;
    out->u.flock.conf = conf;
    out->u.flock.fp = fp; // raw fp passed through for the detail screen (seeding)
    return EspMsgFlock;
}

/** Fill out->u.ble from a split BLE-line. */
static EspMsgType parse_ble(char** f, int n, EspMsg* out) {
    if(n < 5) return EspMsgIgnore;
    uint8_t addr[6];
    if(strlen(f[1]) < 12 || !parse_mac_compact(f[1], addr)) return EspMsgIgnore;

    // Walk the trailing fields after <name> (f[6..n]). Each is either the raw
    // mfg-data hex (Flock 0x09C8) or the rv=1 Raven-GATT flag; distinguish by the
    // presence of '=': rv=1 has one, mfghex (pure hex) never does.
    uint8_t mfg[32];
    size_t mfg_len = 0;
    bool raven_gatt = false;
    for(int fi = 6; fi < n; fi++) {
        const char* t = f[fi];
        if(strchr(t, '=')) {
            if(strcmp(t, "rv=1") == 0) raven_gatt = true;
        } else if(mfg_len == 0) {
            for(size_t i = 0; mfg_len < sizeof(mfg); i += 2) {
                int hi = esp_hexval(t[i]);
                if(hi < 0) break;
                int lo = esp_hexval(t[i + 1]);
                if(lo < 0) break;
                mfg[mfg_len++] = (uint8_t)((hi << 4) | lo);
            }
        }
    }

    memcpy(out->u.ble.addr, addr, 6);
    out->u.ble.name = (n >= 6) ? f[5] : "";
    out->u.ble.rssi = (int8_t)atoi(f[2]);
    out->u.ble.cat = (uint8_t)atoi(f[3]);
    out->u.ble.company = (uint16_t)atoi(f[4]);
    memcpy(out->u.ble.mfg, mfg, mfg_len);
    out->u.ble.mfg_len = mfg_len;
    out->u.ble.raven_gatt = raven_gatt;
    return EspMsgBleDev;
}

EspMsgType esp_parse_companion_line(char* line, EspMsg* out) {
    memset(out, 0, sizeof(*out));
    out->type = EspMsgIgnore;

    if(strncmp(line, "FLOCKCO", 7) == 0) {
        // FLOCKCO,<ver> -- the companion's wire-protocol version (absent on old FW).
        char* f[2];
        int n = esp_split_fields(line, f, 2);
        out->u.banner.version = (n >= 2) ? (uint8_t)atoi(f[1]) : 0;
        out->type = EspMsgBanner;
        return out->type;
    }
    // ---- WiFi security scan: WBEGIN / W,... / WEND ----
    if(strncmp(line, "WBEGIN", 6) == 0) {
        out->type = EspMsgWifiBegin;
        return out->type;
    }
    if(strncmp(line, "WEND", 4) == 0) {
        out->type = EspMsgWifiEnd;
        return out->type;
    }
    if(strncmp(line, "DA,", 3) == 0) {
        // DA,<bssid>,<ch>  deauth/disassoc attack target attribution
        char* f[3];
        int n = esp_split_fields(line, f, 3);
        uint8_t bssid[6];
        if(n >= 3 && strlen(f[1]) >= 12 && parse_mac_compact(f[1], bssid)) {
            memcpy(out->u.deauth.bssid, bssid, 6);
            out->u.deauth.channel = (uint8_t)atoi(f[2]);
            out->type = EspMsgDeauthTarget;
        }
        return out->type;
    }
    if(strncmp(line, "ATK,", 4) == 0) {
        // ATK,<kind>,<value>  active attack-tool signature from the companion.
        char* f[3];
        int n = esp_split_fields(line, f, 3);
        if(n >= 2) {
            out->u.attack.kind = f[1];
            out->u.attack.value = (n >= 3) ? strtoul(f[2], NULL, 10) : 0;
            out->type = EspMsgAttack;
        }
        return out->type;
    }
    if(strncmp(line, "LOC,", 4) == 0) {
        // LOC,<rssi>[,<mac>]  live signal strength for the active Locator target.
        out->u.locate.rssi = (int8_t)atoi(line + 4);
        out->type = EspMsgLocate;
        return out->type;
    }
    // ---- BLE scan: BBEGIN / BLE,... / BEND ----
    if(strncmp(line, "BBEGIN", 6) == 0) {
        out->type = EspMsgBleBegin;
        return out->type;
    }
    if(strncmp(line, "BEND", 4) == 0) {
        out->type = EspMsgBleEnd;
        return out->type;
    }
    if(strncmp(line, "BLE,", 4) == 0) {
        // BLE,<addr>,<rssi>,<cat>,<company>,<name>[,<mfghex>][,rv=1]. 8 slots hold
        // the 6 base fields plus both optional trailers (either order, either
        // absent), so neither trailer gets folded back into <name>.
        char* f[8];
        int n = esp_split_fields(line, f, 8);
        return (out->type = parse_ble(f, n, out));
    }
    if(line[0] == 'W' && line[1] == ',') {
        // W,<bssid>,<rssi>,<ch>,<auth>,<pair>,<grp>,<wps>,<ssid>
        char* f[9];
        int n = esp_split_fields(line, f, 9);
        if(n < 8) return out->type;
        uint8_t bssid[6];
        if(strlen(f[1]) < 12 || !parse_mac_compact(f[1], bssid)) return out->type;
        memcpy(out->u.wifi.bssid, bssid, 6);
        out->u.wifi.ssid = (n >= 9) ? f[8] : "";
        out->u.wifi.rssi = (int8_t)atoi(f[2]);
        out->u.wifi.channel = (uint8_t)atoi(f[3]);
        out->u.wifi.auth = (uint8_t)atoi(f[4]);
        out->u.wifi.pairwise = (uint8_t)atoi(f[5]);
        out->u.wifi.wps = atoi(f[7]) != 0; // f[6] (group cipher) is not consumed downstream
        out->type = EspMsgWifiAp;
        return out->type;
    }
    if(line[0] == 'S' && line[1] == ',') {
        // S,<frames>,<hits>,<ch>[,<deauths>]
        char* f[5];
        int n = esp_split_fields(line, f, 5);
        if(n >= 4) {
            out->u.status.frames = strtoul(f[1], NULL, 10);
            out->u.status.hits = strtoul(f[2], NULL, 10);
            out->u.status.channel = (uint8_t)atoi(f[3]);
            out->u.status.have_deauths = (n >= 5);
            out->u.status.deauths = (n >= 5) ? strtoul(f[4], NULL, 10) : 0;
            out->type = EspMsgStatus;
        }
        return out->type;
    }
    if(line[0] == 'D' && line[1] == ',') {
        // D,<mac>,<rssi>,<ch>,<type>,<conf>,<ssid>[,fp=<hex32>]
        char* f[8];
        int n = esp_split_fields(line, f, 8);
        return (out->type = parse_flock(f, n, out));
    }
    return out->type; // EspMsgIgnore
}
