// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "esp_parser.h"

#include <stdlib.h>
#include <stdio.h>
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
    // A NAME IN A PROBE REQUEST BELONGS TO THE NETWORK SOUGHT, NOT THE SENDER.
    //
    // Beacons and probe responses carry the transmitter's OWN SSID, so an
    // anchored "Flock-XXXXXX" there names the device. A probe REQUEST carries
    // the SSID the sender is LOOKING FOR: a phone that once joined a camera's
    // provisioning AP, or an installer's laptop, probes for "Flock-A1B2C3" from
    // its own address, and until this cap that phone was CONFIRMED as a camera.
    // It is the same attribution error the companion fixed for its rx side,
    // from the other direction. Likely keeps it on the list and alertable; it
    // still says a Flock network is known to something nearby.
    if(conf == FlockConfidenceConfirmed && ftype == 'P') conf = FlockConfidenceLikely;

    // Trailing key=value fields. Older firmware omits them and newer firmware may
    // add more, so unknown keys are skipped rather than treated as an error.
    //   fp=<hex32>  B1 IE-skeleton fingerprint (probe requests only)
    //   cls=a       device class: acoustic (SoundThinking).
    //   cls=x       device class: Axon police equipment (body, in-car OR the
    //               fixed Outpost/Lightpost ALPR -- one OUI covers all of them).
    //   cls=g       device class: vendor-exclusive competitor gear -- vendor
    //               known (Ubicquia / Motorola Solutions / Verkada / Genetec /
    //               Avigilon), KIND not determined. The vendor itself is NOT on
    //               the wire: this side re-derives it from the MAC, so it cannot
    //               inherit an attribution from firmware that lags the app.
    //               Absent, or a letter this build does not know, means fall
    //               back to the class implied by the MAC's own OUI.
    //   hid=1       the AP beacons but withholds its SSID.
    //   pr=<n>      probes this transmitter sent inside the companion's sliding
    //               window. An OBSERVATION, not a score: the companion has
    //               already applied its own gate. Surfaced so a threshold set
    //               without field data can be tuned against real captures.
    // Start at f[7] (AFTER the ssid at f[6]) so an SSID that literally begins
    // "fp=" or "cls=" can't be misread as one of these fields.
    uint32_t fp = 0;
    bool sig_hit = false;
    bool hidden = false;
    uint8_t probe_rate = 0;
    // Default the class from the MAC's own OUI rather than assuming ALPR: that
    // keeps classification right when an older companion sends no cls= field.
    // An explicit cls= below still wins.
    FlockDevClass dev_class = flock_class_from_mac(mac);
    for(int i = 7; i < n; i++) {
        if(strncmp(f[i], "fp=", 3) == 0) {
            // "fp2=" is NOT read here, deliberately. A detection is scored by
            // sg= and fp=; the content hash is a COLLECTION field and its only
            // consumer is survey.csv, which gets it from the SV line. Parsing it
            // here too would store a value nothing reads, on every row of a
            // 64-entry table, for a table of fp2 signatures that ships empty.
            fp = (uint32_t)strtoul(f[i] + 3, NULL, 16);
        } else if(strncmp(f[i], "sg=", 3) == 0) {
            sig_hit = (f[i][3] == '1');
        } else if(strncmp(f[i], "cls=", 4) == 0) {
            // Unknown letters fall back to the MAC-derived class rather than to
            // ALPR: a newer companion may name a class this build predates, and
            // guessing "camera" for something we cannot identify is the exact
            // over-claim the class field exists to prevent.
            if(f[i][4] == 'a')
                dev_class = FlockClassAcoustic;
            else if(f[i][4] == 'x')
                dev_class = FlockClassBodycam;
            else if(f[i][4] == 'g')
                dev_class = FlockClassGear;
            else if(f[i][4] == 'c')
                dev_class = FlockClassAlpr;
        } else if(strncmp(f[i], "hid=", 4) == 0) {
            hidden = (f[i][4] == '1');
        } else if(strncmp(f[i], "pr=", 3) == 0) {
            unsigned long v = strtoul(f[i] + 3, NULL, 10);
            probe_rate = (uint8_t)(v > 255 ? 255 : v);
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
    } else if(fp_src == FlockIeFpCandidate || fp_src == FlockIeFpUser) {
        // Single-source built-in candidate, or an UNVERIFIED user fp
        // (signatures.json). Either way a candidate device-CLASS match ONLY --
        // capped at "Class?", never Confirmed even with a Flock OUI. Promotion of
        // a candidate to the auto-confirming builtin table requires a second
        // independent capture (see flock_ie_fps_candidate[] in flock_db.c).
        if(FlockConfidenceProbeFp > conf) conf = FlockConfidenceProbeFp;
        ftype = 'F';
    }

    // COMMUNITY PROBE SIGNATURE. The companion matched the whole IE signature
    // against its published table and said so with sg=1; it deliberately did not
    // raise its own score, because conf=3 on that wire means CONFIRMED.
    //
    // THIS IS THE ONE TELL THAT FIRES ON A RANDOMISED ADDRESS, so it is also the
    // only rung that can produce a detection with no OUI behind it at all -- the
    // companion now lets such a frame past its conf==0 gate purely on this.
    //
    // Capped at "Class?" for the same reason a candidate fingerprint is: it is
    // single-source (one contributor, one geography, drive-tested by one
    // project), and being right 11 times out of 12 is not the same as being
    // proof. A Flock OUI underneath does NOT lift it, because then the OUI rungs
    // above have already had their say.
    if(sig_hit) {
        if(FlockConfidenceProbeFp > conf) conf = FlockConfidenceProbeFp;
        // 'S', not 'F', and only when a real fingerprint did not already claim
        // the row. Both sit at the same rung, but they are different evidence
        // and the detail screen names them separately -- a hit the operator can
        // only have got from the signature must not say "IE fp", which they
        // could then go looking for in signatures.json and never find.
        if(ftype != 'F') ftype = 'S';
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
    out->u.flock.probe_rate = probe_rate;
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
    bool bwc_tag = false;
    for(int fi = 6; fi < n; fi++) {
        const char* t = f[fi];
        if(strchr(t, '=')) {
            if(strcmp(t, "rv=1") == 0)
                raven_gatt = true;
            else if(strcmp(t, "sep=1") == 0)
                tracker_separated = true;
            else if(strcmp(t, "bwc=1") == 0)
                bwc_tag = true;
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
    out->u.ble.bwc_tag = bwc_tag;
    return EspMsgBleDev;
}

EspMsgType esp_parse_companion_line(char* line, EspMsg* out) {
    memset(out, 0, sizeof(*out));
    out->type = EspMsgIgnore;

    if(strncmp(line, "FLOCKCO", 7) == 0) {
        // FLOCKCO,<proto>[,<build>] -- the wire-protocol version, then the
        // companion's own build version. Both optional: firmware older than
        // either simply sends fewer fields.
        char* f[3];
        int n = esp_split_fields(line, f, 3);
        out->u.banner.version = (n >= 2) ? (uint8_t)atoi(f[1]) : 0;
        if(n >= 3 && f[2][0]) {
            snprintf(out->u.banner.build, sizeof(out->u.banner.build), "%s", f[2]);
        }
        out->type = EspMsgBanner;
        return out->type;
    }
    if(strncmp(line, "SIGREV,", 7) == 0) {
        // One bounded token. Keep the parser forwards-compatible with revision
        // schemes other than hex, but reject an empty or comma-extended value so
        // the UI never displays a partly parsed claim.
        const char* revision = line + 7;
        size_t n = strlen(revision);
        if(n == 0 || n >= 16 || strchr(revision, ',') != NULL) return EspMsgIgnore;
        out->u.sigrev.revision = revision;
        out->type = EspMsgSigRevision;
        return out->type;
    }
    if(strncmp(line, "SIGTEST,", 8) == 0) {
        // SIGTEST,<signature-with-commas>,<hash>,<0|1>. Split FROM THE RIGHT:
        // the production signature is deliberately human-readable and contains
        // commas, so a normal field splitter would silently cut it apart.
        char* signature = line + 8;
        char* pass_field = strrchr(signature, ',');
        if(!pass_field) return EspMsgIgnore;
        *pass_field++ = '\0';
        char* hash_field = strrchr(signature, ',');
        if(!hash_field) return EspMsgIgnore;
        *hash_field++ = '\0';
        if(signature[0] == '\0' ||
           !((pass_field[0] == '0' || pass_field[0] == '1') && pass_field[1] == '\0')) {
            return EspMsgIgnore;
        }
        char* end = NULL;
        unsigned long hash = strtoul(hash_field, &end, 16);
        size_t hash_len = strlen(hash_field);
        if(hash_len == 0 || hash_len > 8 || !end || *end != '\0') return EspMsgIgnore;
        out->u.sigtest.signature = signature;
        out->u.sigtest.hash = (uint32_t)hash;
        out->u.sigtest.pass = pass_field[0] == '1';
        out->type = EspMsgSigTest;
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
    // SV,<mac12>,<rssi>,<ch>,<fp8hex>,<count>  one wildcard-probe transmitter,
    // matched or not. SVBEGIN/SVEND bracket a dump and carry nothing themselves.
    if(strncmp(line, "SV,", 3) == 0) {
        // Split into EIGHT, not six: fields 7 and 8 (fp2 and the printable
        // signature) were appended in v0.96 and older firmware simply stops at
        // six. The signature is LAST on the line because it contains commas --
        // esp_split_fields stops once it has filled the array, so f[7] is the
        // whole remainder rather than one comma-delimited piece of it.
        char* f[8];
        int n = esp_split_fields(line, f, 8);
        if(n < 6) return (out->type = EspMsgIgnore);
        uint8_t mac[6];
        if(!parse_mac_compact(f[1], mac)) return (out->type = EspMsgIgnore);
        memcpy(out->u.survey.mac, mac, 6);
        out->u.survey.rssi = (int8_t)atoi(f[2]);
        out->u.survey.channel = (uint8_t)atoi(f[3]);
        out->u.survey.fp = (uint32_t)strtoul(f[4], NULL, 16);
        out->u.survey.count = (uint16_t)atoi(f[5]);
        out->u.survey.fp2 = (n >= 7) ? (uint32_t)strtoul(f[6], NULL, 16) : 0u;
        out->u.survey.sig = (n >= 8) ? f[7] : "";
        return (out->type = EspMsgSurvey);
    }

    // RID,<addr12>,<rssi>,<hex>  one ASTM F3411 Remote ID broadcast. The hex is
    // the BLE service data starting at the 0x0D application code; it is carried
    // across verbatim and decoded by helpers/open_drone_id.c.
    if(strncmp(line, "RID,", 4) == 0) {
        char* f[4];
        int n = esp_split_fields(line, f, 4);
        if(n < 4) return (out->type = EspMsgIgnore);
        uint8_t addr[6];
        if(!parse_mac_compact(f[1], addr)) return (out->type = EspMsgIgnore);
        memcpy(out->u.rid.addr, addr, 6);
        out->u.rid.rssi = (int8_t)atoi(f[2]);
        size_t len = 0;
        const char* t = f[3];
        // Stop on the first non-hex or odd trailing nibble rather than guessing
        // at it -- a half-decoded advert is worse than a dropped one, because the
        // decoder downstream would read real fields out of invented bytes.
        for(size_t i = 0; len < sizeof(out->u.rid.payload); i += 2) {
            int hi = esp_hexval(t[i]);
            if(hi < 0) break;
            int lo = esp_hexval(t[i + 1]);
            if(lo < 0) break;
            out->u.rid.payload[len++] = (uint8_t)((hi << 4) | lo);
        }
        if(len == 0) return (out->type = EspMsgIgnore);
        out->u.rid.payload_len = len;
        return (out->type = EspMsgRemoteId);
    }

    if(strncmp(line, "BLE,", 4) == 0) {
        // BLE,<addr>,<rssi>,<cat>,<company>,<name>[,<mfghex>][,rv=1][,sep=1][,bwc=1].
        // 10 slots hold the 6 base fields plus all FOUR optional trailers (any
        // order, any absent), so no trailer gets folded back into <name>.
        //
        // GROW THIS WITH EVERY NEW TRAILER. esp_split_fields() stops splitting at
        // `max`, so a short array does not drop the extra token -- it glues it
        // onto the previous one, where the `key=` check then misses it silently.
        // bwc=1 was the fourth trailer and needed this bumped from 9.
        char* f[10];
        int n = esp_split_fields(line, f, 10);
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
        // D,<mac>,<rssi>,<ch>,<type>,<conf>,<ssid>[,fp=][,sg=][,pr=][,cls=][,hid=]
        // 7 base fields + FIVE optional trailers = 12. esp_split_fields stops
        // splitting once it hits `max`, so a short array does not drop the
        // extra token -- it silently glues it onto the previous one, where the
        // key= prefix check then misses it. At 10 slots a probe line carrying
        // all five trailers lost whichever came last (cls= or hid=). Sized with
        // headroom so the next trailer does not repeat that; the test suite
        // pins the all-trailers line.
        char* f[16];
        int n = esp_split_fields(line, f, 16);
        return (out->type = parse_flock(f, n, out));
    }
    return out->type; // EspMsgIgnore
}
