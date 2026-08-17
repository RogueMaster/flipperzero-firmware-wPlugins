// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
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

    // The companion scores SSIDs with a LOOSER matcher than flock_db.c: it
    // substring-matches "flock-", we anchor on ^flock-[0-9a-f]{6}$. Never take
    // its word for a CONFIRMED -- re-derive from the SSID we were sent and keep
    // the stricter answer. Lower rungs still come from the ESP, which knows
    // things this line does not (probe behaviour, the silent receiver's OUI).
    //
    // This is a TRUST BOUNDARY, not a redundant check. Companion firmware is
    // flashed separately and can lag the app by releases -- that drift is why
    // the proto version handshake exists -- so the Flipper must not inherit an
    // over-claim from a build it did not ship with. Before this guard existed,
    // a v0.46 companion reported "Flock-Guest" as CONFIRMED and the app printed
    // it verbatim; flock_score() has no production caller, so nothing else in
    // the pipeline was ever going to catch that.
    //
    // Guarded on a non-empty SSID: our firmware cannot emit conf=3 without one
    // (its ssid_score needs len > 0), so an empty SSID here means a corrupted
    // line, and we have no basis to overrule the ESP's OUI/probe reasoning.
    if(conf == FlockConfidenceConfirmed && ssid[0] != '\0') {
        FlockConfidence by_ssid = flock_ssid_confidence(ssid);
        if(by_ssid < conf) conf = by_ssid;
    }

    // Trailing key=value fields. Older firmware omits them and newer firmware may
    // add more, so unknown keys are skipped rather than treated as an error.
    //   fp=<hex32>  B1 IE-skeleton fingerprint (probe requests only)
    //   cls=a       device class: acoustic (SoundThinking).
    //   cls=x       device class: Axon body-worn / in-car police equipment.
    //               Absent, or a letter this build does not know, means fall
    //               back to the class implied by the MAC's own OUI.
    //   hid=1       the AP beacons but withholds its SSID.
    // Start at f[7] (AFTER the ssid at f[6]) so an SSID that literally begins
    // "fp=" or "cls=" can't be misread as one of these fields.
    uint32_t fp = 0;
    bool hidden = false;
    // Default the class from the MAC's own OUI rather than assuming ALPR: that
    // keeps classification right when an older companion sends no cls= field.
    // An explicit cls= below still wins.
    FlockDevClass dev_class = flock_class_from_mac(mac);
    for(int i = 7; i < n; i++) {
        if(strncmp(f[i], "fp=", 3) == 0) {
            fp = (uint32_t)strtoul(f[i] + 3, NULL, 16);
        } else if(strncmp(f[i], "cls=", 4) == 0) {
            // Unknown letters fall back to the MAC-derived class rather than to
            // ALPR: a newer companion may name a class this build predates, and
            // guessing "camera" for something we cannot identify is the exact
            // over-claim the class field exists to prevent.
            if(f[i][4] == 'a')
                dev_class = FlockClassAcoustic;
            else if(f[i][4] == 'x')
                dev_class = FlockClassBodycam;
            else if(f[i][4] == 'c')
                dev_class = FlockClassAlpr;
        } else if(strncmp(f[i], "hid=", 4) == 0) {
            hidden = (f[i][4] == '1');
        }
    }

    // DELIBERATELY NOT SCORED. A hidden SSID is WatchFlock's headline finding --
    // Flock moved their cameras to hidden SSIDs and probe requests, which is why
    // scanning for a broadcast name stopped working. But hiding an SSID is also
    // ordinary consumer-router behaviour, and our OUI tables are shared
    // silicon-vendor prefixes, so "Flock OUI + hidden -> Likely" would promote
    // every hidden ESP32-based AP in range. Precision over recall: report the
    // attribute, let the operator weigh it, and revisit the scoring rule only
    // once bench/field captures justify one.
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
    out->u.flock.dev_class = dev_class;
    out->u.flock.hidden = hidden;
    return EspMsgFlock;
}

/** Fill out->u.ble from a split BLE-line. */
static EspMsgType parse_ble(char** f, int n, EspMsg* out) {
    if(n < 5) return EspMsgIgnore;
    uint8_t addr[6];
    if(strlen(f[1]) < 12 || !parse_mac_compact(f[1], addr)) return EspMsgIgnore;

    // Walk the trailing fields after <name> (f[6..n]). Each is either the raw
    // mfg-data hex (Flock 0x09C8), the rv=1 Raven-GATT flag, or sep=1 for an
    // Apple Find My tracker in separated state. Distinguish key=value trailers
    // from mfghex by the presence of '='.
    uint8_t mfg[32];
    size_t mfg_len = 0;
    bool raven_gatt = false;
    bool tracker_separated = false;
    for(int fi = 6; fi < n; fi++) {
        const char* t = f[fi];
        if(strchr(t, '=')) {
            if(strcmp(t, "rv=1") == 0)
                raven_gatt = true;
            else if(strcmp(t, "sep=1") == 0)
                tracker_separated = true;
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
    out->u.ble.tracker_separated = tracker_separated;
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
    if(strncmp(line, "ACT,", 4) == 0) {
        // ACT,<op>,<status>[,<rssi>]  result of an explicit tracker action.
        // Status is intentionally a bounded token: it is displayed on the
        // Flipper but never treated as a command or a confidence signal.
        char* f[5];
        int n = esp_split_fields(line, f, 5);
        if(n < 3 || f[1][0] == '\0' || f[2][0] == '\0') return EspMsgIgnore;
        out->u.action.op = f[1];
        out->u.action.status = f[2];
        out->u.action.have_rssi = n >= 4 && f[3][0] != '\0';
        out->u.action.rssi = out->u.action.have_rssi ? (int8_t)atoi(f[3]) : 0;
        out->type = EspMsgAction;
        return out->type;
    }
    // G,<nmea>  one sentence relayed from a GPS wired to the ESP board. Passed
    // through verbatim from the '$' for nmea_parse_line() to decode -- see
    // EspMsg.u.gps.nmea for why it is not parsed here. Guard the payload: an
    // empty or non-'$' body is not a sentence, and forwarding it would only make
    // the NMEA parser reject it one layer further on.
    // GPSCFG,<on>,<pin>,<baud>  the companion's echo of its relay state. Tested
    // BEFORE "G," would ever be reached anyway, but kept adjacent to it since the
    // two are the same feature. A malformed echo is ignored rather than guessed
    // at: reporting the relay as off when we simply could not read the line would
    // send the operator hunting a configuration problem that does not exist.
    // CHIP,<target>,<gpio_count>,<mask_hi>,<mask_lo>,<has5g>  the board telling
    // the app what it physically is, so the GPS pin picker stops offering a
    // classic ESP32's pinout on every chip.
    if(strncmp(line, "CHIP,", 5) == 0) {
        char* f[6];
        int n = esp_split_fields(line, f, 6);
        if(n < 6) return EspMsgIgnore;
        out->u.chip.target = f[1];
        out->u.chip.gpio_count = (uint8_t)atoi(f[2]);
        uint64_t hi = strtoul(f[3], NULL, 16);
        uint64_t lo = strtoul(f[4], NULL, 16);
        out->u.chip.gps_pin_mask = (hi << 32) | lo;
        out->u.chip.has_5ghz = (atoi(f[5]) != 0);
        out->type = EspMsgChip;
        return out->type;
    }
    // BAND,<2g|5g|all>,<channels>  the band actually in force. On a 2.4-only
    // radio this always answers 2g whatever was asked, which is the point: it
    // reports coverage that exists rather than coverage that was requested.
    if(strncmp(line, "BAND,", 5) == 0) {
        char* f[3];
        int n = esp_split_fields(line, f, 3);
        if(n < 3) return EspMsgIgnore;
        if(strcmp(f[1], "5g") == 0) {
            out->u.band.sel = 1;
        } else if(strcmp(f[1], "all") == 0) {
            out->u.band.sel = 2;
        } else if(strcmp(f[1], "2g") == 0) {
            out->u.band.sel = 0;
        } else {
            return EspMsgIgnore; // an unknown band name is not a band
        }
        out->u.band.channels = (uint16_t)atoi(f[2]);
        out->type = EspMsgBand;
        return out->type;
    }
    if(strncmp(line, "GPSCFG,", 7) == 0) {
        char* f[4];
        int n = esp_split_fields(line, f, 4);
        if(n < 4) return EspMsgIgnore;
        out->u.gpscfg.on = (atoi(f[1]) != 0);
        out->u.gpscfg.pin = (int16_t)atoi(f[2]);
        out->u.gpscfg.baud = strtoul(f[3], NULL, 10);
        out->type = EspMsgGpsCfg;
        return out->type;
    }
    if(strncmp(line, "G,", 2) == 0) {
        if(line[2] != '$') return EspMsgIgnore;
        out->u.gps.nmea = line + 2;
        out->type = EspMsgGpsNmea;
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
        // BLE,<addr>,<rssi>,<cat>,<company>,<name>[,<mfghex>][,rv=1][,sep=1].
        // 9 slots hold the 6 base fields plus all three optional trailers
        // (either order, either absent), so no trailer gets folded back into
        // <name>.
        char* f[9];
        int n = esp_split_fields(line, f, 9);
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
        // D,<mac>,<rssi>,<ch>,<type>,<conf>,<ssid>[,fp=<hex32>][,cls=a|x][,hid=1]
        // 10 slots = 7 base fields + ALL optional trailers. esp_split_fields
        // stops splitting once it hits `max`, so a short array does not drop the
        // extra token -- it silently glues it onto the previous one, where the
        // key= prefix check then misses it. Grow this in step with the trailers.
        char* f[10];
        int n = esp_split_fields(line, f, 10);
        return (out->type = parse_flock(f, n, out));
    }
    return out->type; // EspMsgIgnore
}
