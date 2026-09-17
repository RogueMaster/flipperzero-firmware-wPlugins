#include "sr_parse_marauder.h"

#include <limits.h>
#include <string.h>

/*
 * ★ Pure logic; must not include furi (ADR-003).
 *
 * Single-row event parsing. Only [line, line+len) is guaranteed readable; line[len]
 * is not guaranteed to be a NUL (ADR-010). This file must not use strlen / strcmp /
 * atoi / strtol / sr_strlcpy.
 *
 * GPS blocks (T2.3b / ADR-013): normalization and field matching go only through this
 * file's GPS path; ordinary dispatch still uses the raw line / len.
 */

/* ref: V-001 verbatim; there is no space between Starting and Wardrive */
static const char kStart[] = "StartingWardrive. Stop with stopscan";
/* ref: V-003; these three strings are verbatim and must not be rewritten */
static const char kStopWifi[] = "Stopping WiFi tran/recv";
static const char kStopNmea[] = "END OF NMEA STREAM";
static const char kStopGps[] = "Stopping GPS data updates";

/* Card T2.3b / measured verbatim from gpsdata.bin; must not be rewritten */
static const char kGpsHdr[] = "==== GPS Data ====";
static const char kGpsFix[] = "Fix:";
static const char kGpsText[] = "Text:";
static const char kGpsSats[] = "Sats:";
static const char kGpsAcc[] = "Acc:";
static const char kGpsLat[] = "Lat:";
static const char kGpsLon[] = "Lon:";
static const char kGpsAlt[] = "Alt:";
static const char kGpsDT[] = "D/T:";
static const char kGpsYes[] = "Yes";
static const char kGpsNo[] = "No";

/* ref: V-001 / V-003 / V-008. The trailing \n is part of the command. V-046: -serial verified ✅ in source, unverified on hardware */
static const char kCmdStart[] = "wardrive\n";
static const char kCmdStartSerial[] = "wardrive -serial\n";
static const char kCmdStop[] = "stopscan\n";
/* ref: V-004 / V-060. The echo in the hardware fixture tools/host_test/fixtures/gpsdata.bin is #gpsdata. */
static const char kCmdGps[] = "gpsdata\n";
/* ref: V-078. Success receipt is the "POI tagged: " prefix, not an echo of this command. */
static const char kCmdPoi[] = "wardrivepoi\n";
static const char kPoiTagged[] = "POI tagged: ";
/*
 * Must stay consistent with scenes/scene_probe.c:124, pinned by the first line #info of
 * fixtures/startup_info.bin. info has no builder (folding it into the codec is T7.2).
 */
static const char kCmdInfo[] = "info\n";

typedef struct {
    const char* p;
    size_t n;
} Field;

static void emit_unknown(SrEvent* out, const char* line, size_t len) {
    out->kind = SrEventUnknown;
    out->u.unknown.text = line;
    out->u.unknown.len = len;
}

static bool line_eq(const char* line, size_t len, const char* lit, size_t lit_n) {
    return len == lit_n && memcmp(line, lit, lit_n) == 0;
}

static bool line_ends(const char* line, size_t len, const char* suf, size_t suf_n) {
    return len >= suf_n && memcmp(line + (len - suf_n), suf, suf_n) == 0;
}

static bool line_starts(const char* line, size_t len, const char* pre, size_t pre_n) {
    return len >= pre_n && memcmp(line, pre, pre_n) == 0;
}

static void copy_cap(char* dst, size_t cap, const char* src, size_t n) {
    size_t c;

    if(dst == NULL || cap == 0) {
        return;
    }
    c = n;
    if(c >= cap) {
        c = cap - 1U;
    }
    if(c > 0U && src != NULL) {
        memcpy(dst, src, c);
    }
    dst[c] = '\0';
}

static bool is_hex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

/* XX:XX:XX:XX:XX:XX, 17 chars, hex accepted in either case. ref: V-002 */
static bool is_mac(const char* s, size_t n) {
    size_t i;

    if(n != (size_t)SR_BSSID_MAX) {
        return false;
    }
    for(i = 0; i < n; i++) {
        if((i % 3U) == 2U) {
            if(s[i] != ':') {
                return false;
            }
        } else if(!is_hex(s[i])) {
            return false;
        }
    }
    return true;
}

static bool parse_u32(const char* s, size_t n, uint32_t* out) {
    uint32_t acc = 0;
    size_t i;

    if(n == 0 || s == NULL || out == NULL) {
        return false;
    }
    for(i = 0; i < n; i++) {
        unsigned d;

        if(s[i] < '0' || s[i] > '9') {
            return false;
        }
        d = (unsigned)(s[i] - '0');
        if(acc > (0xFFFFFFFFu / 10u)) {
            return false;
        }
        if(acc == (0xFFFFFFFFu / 10u) && d > (0xFFFFFFFFu % 10u)) {
            return false;
        }
        acc = acc * 10u + (uint32_t)d;
    }
    *out = acc;
    return true;
}

/* An optional leading '-', then at least one decimal digit. Overflowing int fails. Syntax only. */
static bool parse_int(const char* s, size_t n, int* out) {
    size_t i = 0;
    int sign = 1;
    unsigned long acc = 0;

    if(n == 0 || s == NULL || out == NULL) {
        return false;
    }
    if(s[0] == '-') {
        sign = -1;
        i = 1;
        if(i == n) {
            return false;
        }
    }
    for(; i < n; i++) {
        unsigned d;

        if(s[i] < '0' || s[i] > '9') {
            return false;
        }
        d = (unsigned)(s[i] - '0');
        if(acc > (unsigned long)INT_MAX / 10UL) {
            return false;
        }
        if(acc == (unsigned long)INT_MAX / 10UL && d > (unsigned)(INT_MAX % 10)) {
            if(!(sign < 0 && acc == (unsigned long)INT_MAX / 10UL &&
                 d == (unsigned)(INT_MAX % 10) + 1U)) {
                return false;
            }
        }
        acc = acc * 10UL + (unsigned long)d;
    }
    if(sign < 0) {
        if(acc > (unsigned long)INT_MAX + 1UL) {
            return false;
        }
        if(acc == (unsigned long)INT_MAX + 1UL) {
            *out = INT_MIN;
        } else {
            *out = -(int)acc;
        }
    } else {
        if(acc > (unsigned long)INT_MAX) {
            return false;
        }
        *out = (int)acc;
    }
    return true;
}

/*
 * Count 10 commas backwards from end of line to obtain 11 fields.
 * Fewer than 10 commas -> failure. Anything beyond 10 stays in field[0].
 */
static bool split_right10(const char* line, size_t len, Field f[11]) {
    size_t pos[10];
    size_t found = 0;
    size_t i;
    size_t k;

    i = len;
    while(i > 0U && found < 10U) {
        i--;
        if(line[i] == ',') {
            pos[found] = i;
            found++;
        }
    }
    if(found < 10U) {
        return false;
    }

    f[0].p = line;
    f[0].n = pos[9];
    for(k = 1; k <= 9U; k++) {
        size_t start = pos[10U - k] + 1U;
        size_t end = pos[9U - k];

        f[k].p = line + start;
        f[k].n = end - start;
    }
    f[10].p = line + pos[0] + 1U;
    f[10].n = len - (pos[0] + 1U);
    return true;
}

static bool parse_wifi_f0(Field f, SrApRecord* rec) {
    size_t i;
    size_t sep = (size_t)-1;
    size_t right_off;
    size_t right_n;

    if(f.n < 3U) {
        return false;
    }
    for(i = 0; i + 3U <= f.n; i++) {
        if(f.p[i] == ' ' && f.p[i + 1U] == '|' && f.p[i + 2U] == ' ') {
            sep = i;
            break;
        }
    }
    if(sep == (size_t)-1) {
        return false;
    }
    if(!parse_u32(f.p, sep, &rec->cursor)) {
        return false;
    }
    right_off = sep + 3U;
    right_n = f.n - right_off;
    if(!is_mac(f.p + right_off, right_n)) {
        return false;
    }
    copy_cap(rec->bssid, sizeof(rec->bssid), f.p + right_off, right_n);
    return true;
}

static bool parse_ble_f0(Field f, SrApRecord* rec) {
    const char* mac;
    size_t name_n;

    rec->cursor = 0;
    if(f.n < (size_t)SR_BSSID_MAX) {
        return false;
    }
    mac = f.p + (f.n - (size_t)SR_BSSID_MAX);
    if(!is_mac(mac, (size_t)SR_BSSID_MAX)) {
        return false;
    }
    copy_cap(rec->bssid, sizeof(rec->bssid), mac, (size_t)SR_BSSID_MAX);
    name_n = f.n - (size_t)SR_BSSID_MAX;
    /* Unnamed device: name is byte-for-byte identical to the MAC. ref: V-002, corrected against hardware */
    if(name_n == (size_t)SR_BSSID_MAX && memcmp(f.p, mac, (size_t)SR_BSSID_MAX) == 0) {
        rec->ssid[0] = '\0';
    } else {
        copy_cap(rec->ssid, sizeof(rec->ssid), f.p, name_n);
    }
    return true;
}

static SrParseResult parse_csv(SrParser* parser, const char* line, size_t len, SrEvent* out) {
    Field f[11];
    SrApRecord rec;

    memset(&rec, 0, sizeof(rec));
    if(!split_right10(line, len, f)) {
        parser->lines_malformed++;
        emit_unknown(out, line, len);
        return SrParseMalformed;
    }

    if(f[10].n == 4U && memcmp(f[10].p, "WIFI", 4) == 0) {
        rec.radio = SrRadioWifi;
    } else if(f[10].n == 3U && memcmp(f[10].p, "BLE", 3) == 0) {
        rec.radio = SrRadioBle;
    } else {
        parser->lines_malformed++;
        emit_unknown(out, line, len);
        return SrParseMalformed;
    }

    if(f[2].n < 2U || f[2].p[0] != '[' || f[2].p[f[2].n - 1U] != ']') {
        parser->lines_malformed++;
        emit_unknown(out, line, len);
        return SrParseMalformed;
    }

    if(rec.radio == SrRadioWifi) {
        if(!parse_wifi_f0(f[0], &rec)) {
            parser->lines_malformed++;
            emit_unknown(out, line, len);
            return SrParseMalformed;
        }
        copy_cap(rec.ssid, sizeof(rec.ssid), f[1].p, f[1].n);
    } else if(!parse_ble_f0(f[0], &rec)) {
        parser->lines_malformed++;
        emit_unknown(out, line, len);
        return SrParseMalformed;
    }

    copy_cap(rec.auth, sizeof(rec.auth), f[2].p, f[2].n);
    copy_cap(rec.datetime, sizeof(rec.datetime), f[3].p, f[3].n);
    if(!parse_int(f[4].p, f[4].n, &rec.channel) || !parse_int(f[5].p, f[5].n, &rec.rssi)) {
        parser->lines_malformed++;
        emit_unknown(out, line, len);
        return SrParseMalformed;
    }
    copy_cap(rec.lat, sizeof(rec.lat), f[6].p, f[6].n);
    copy_cap(rec.lon, sizeof(rec.lon), f[7].p, f[7].n);
    copy_cap(rec.alt, sizeof(rec.alt), f[8].p, f[8].n);
    copy_cap(rec.acc, sizeof(rec.acc), f[9].p, f[9].n);

    if(rec.radio == SrRadioWifi) {
        out->kind = SrEventApFound;
        out->u.ap = rec;
    } else {
        out->kind = SrEventBleFound;
        out->u.ble = rec;
    }
    return SrParseOk;
}

static size_t marauder_start(const SrScanCfg* cfg, char* buf, size_t cap) {
    const char* cmd = kCmdStart;

    if(cfg != NULL && cfg->mirror_to_serial) {
        cmd = kCmdStartSerial;
    }
    return sr_cmd_write(buf, cap, cmd);
}

static size_t marauder_stop(char* buf, size_t cap) {
    return sr_cmd_write(buf, cap, kCmdStop);
}

static size_t marauder_gps(char* buf, size_t cap) {
    return sr_cmd_write(buf, cap, kCmdGps);
}

static size_t marauder_poi(char* buf, size_t cap) {
    return sr_cmd_write(buf, cap, kCmdPoi);
}

static bool probe_n(const char* line, size_t len, SrFirmwareInfo* out) {
    static const char kFw[] = "Firmware: ";
    static const char kVer[] = "Version: ";
    static const char kHw[] = "Hardware: ";
    static const char kIdf[] = "ESP-IDF: ";
    size_t plen;

    if(line == NULL || out == NULL) {
        return false;
    }

    plen = sizeof(kFw) - 1U;
    if(len >= plen && memcmp(line, kFw, plen) == 0) {
        size_t fn = 0;

        copy_cap(out->firmware, sizeof(out->firmware), line + plen, len - plen);
        while(out->firmware[fn] != '\0') {
            fn++;
        }
        if(fn == 8U && memcmp(out->firmware, "Marauder", 8) == 0) {
            out->kind = SrSourceMarauder;
        }
        return true;
    }
    plen = sizeof(kVer) - 1U;
    if(len >= plen && memcmp(line, kVer, plen) == 0) {
        copy_cap(out->version, sizeof(out->version), line + plen, len - plen);
        return true;
    }
    plen = sizeof(kHw) - 1U;
    if(len >= plen && memcmp(line, kHw, plen) == 0) {
        copy_cap(out->hardware, sizeof(out->hardware), line + plen, len - plen);
        return true;
    }
    plen = sizeof(kIdf) - 1U;
    if(len >= plen && memcmp(line, kIdf, plen) == 0) {
        copy_cap(out->esp_idf, sizeof(out->esp_idf), line + plen, len - plen);
        return true;
    }
    return false;
}

static bool marauder_probe(const char* line, SrFirmwareInfo* out) {
    size_t n = 0;
    if(line == NULL || out == NULL) return false;
    while(line[n] != '\0') n++;
    return probe_n(line, n, out);
}

/* Normalization specific to the GPS path: an optional "> " prefix, then skip all leading 0x20. Must not be used for ordinary dispatch. */
static void gps_normalize(const char* line, size_t len, const char** out_p, size_t* out_n) {
    const char* p = line;
    size_t n = len;

    if(n >= 2U && p[0] == '>' && p[1] == ' ') {
        p += 2;
        n -= 2;
    }
    while(n > 0U && *p == ' ') {
        p++;
        n--;
    }
    *out_p = p;
    *out_n = n;
}

static void gps_value_after_prefix(
    const char* p, size_t n, size_t pref_n, const char** vp, size_t* vn) {
    p += pref_n;
    n -= pref_n;
    while(n > 0U && *p == ' ') {
        p++;
        n--;
    }
    *vp = p;
    *vn = n;
}

static bool gps_has_prefix(const char* p, size_t n, const char* pref, size_t pref_n) {
    return n >= pref_n && memcmp(p, pref, pref_n) == 0;
}

/*
 * A normalized field row. Returns true once matched and filled (*r is NeedMore, or Ok when it lands).
 * Returns false on a prefix mismatch or an illegal Fix value; the caller voids the whole block per D3 item 4.
 */
static bool gps_fill_field(
    SrParser* parser, const char* p, size_t n, SrEvent* out, SrParseResult* r) {
    const char* vp;
    size_t vn;

    if(gps_has_prefix(p, n, kGpsFix, sizeof(kGpsFix) - 1U)) {
        gps_value_after_prefix(p, n, sizeof(kGpsFix) - 1U, &vp, &vn);
        if(vn == sizeof(kGpsYes) - 1U && memcmp(vp, kGpsYes, sizeof(kGpsYes) - 1U) == 0) {
            parser->gps_partial.fix = true;
            *r = SrParseNeedMore;
            return true;
        }
        if(vn == sizeof(kGpsNo) - 1U && memcmp(vp, kGpsNo, sizeof(kGpsNo) - 1U) == 0) {
            parser->gps_partial.fix = false;
            *r = SrParseNeedMore;
            return true;
        }
        return false;
    }
    if(gps_has_prefix(p, n, kGpsText, sizeof(kGpsText) - 1U)) {
        gps_value_after_prefix(p, n, sizeof(kGpsText) - 1U, &vp, &vn);
        copy_cap(parser->gps_partial.text, sizeof(parser->gps_partial.text), vp, vn);
        *r = SrParseNeedMore;
        return true;
    }
    if(gps_has_prefix(p, n, kGpsSats, sizeof(kGpsSats) - 1U)) {
        gps_value_after_prefix(p, n, sizeof(kGpsSats) - 1U, &vp, &vn);
        copy_cap(parser->gps_partial.sats, sizeof(parser->gps_partial.sats), vp, vn);
        *r = SrParseNeedMore;
        return true;
    }
    if(gps_has_prefix(p, n, kGpsAcc, sizeof(kGpsAcc) - 1U)) {
        gps_value_after_prefix(p, n, sizeof(kGpsAcc) - 1U, &vp, &vn);
        copy_cap(parser->gps_partial.acc, sizeof(parser->gps_partial.acc), vp, vn);
        *r = SrParseNeedMore;
        return true;
    }
    if(gps_has_prefix(p, n, kGpsLat, sizeof(kGpsLat) - 1U)) {
        gps_value_after_prefix(p, n, sizeof(kGpsLat) - 1U, &vp, &vn);
        copy_cap(parser->gps_partial.lat, sizeof(parser->gps_partial.lat), vp, vn);
        *r = SrParseNeedMore;
        return true;
    }
    if(gps_has_prefix(p, n, kGpsLon, sizeof(kGpsLon) - 1U)) {
        gps_value_after_prefix(p, n, sizeof(kGpsLon) - 1U, &vp, &vn);
        copy_cap(parser->gps_partial.lon, sizeof(parser->gps_partial.lon), vp, vn);
        *r = SrParseNeedMore;
        return true;
    }
    if(gps_has_prefix(p, n, kGpsAlt, sizeof(kGpsAlt) - 1U)) {
        gps_value_after_prefix(p, n, sizeof(kGpsAlt) - 1U, &vp, &vn);
        copy_cap(parser->gps_partial.alt, sizeof(parser->gps_partial.alt), vp, vn);
        *r = SrParseNeedMore;
        return true;
    }
    if(gps_has_prefix(p, n, kGpsDT, sizeof(kGpsDT) - 1U)) {
        gps_value_after_prefix(p, n, sizeof(kGpsDT) - 1U, &vp, &vn);
        copy_cap(
            parser->gps_partial.datetime, sizeof(parser->gps_partial.datetime), vp, vn);
        out->kind = SrEventGps;
        out->u.gps = parser->gps_partial;
        parser->in_gps_block = false;
        *r = SrParseOk;
        return true;
    }
    return false;
}

/*
 * Echo detection: the line must end with '#' + cmd (cmd excludes the trailing \n).
 * A suffix match, not whole-line equality; trailing spaces do not match. The expected string is derived from kCmd*, never copied as a separate literal.
 */
static bool ends_with_echo(const char* line, size_t len, const char* cmd, size_t cmd_n) {
    if(cmd_n + 1U > len) {
        return false;
    }
    if(line[len - cmd_n - 1U] != '#') {
        return false;
    }
    return line_ends(line, len, cmd, cmd_n);
}

static SrCmdAckClass cmdack_class(const char* line, size_t len) {
    if(ends_with_echo(line, len, kCmdStartSerial, sizeof(kCmdStartSerial) - 2U)) {
        return SrCmdAckStart;
    }
    if(ends_with_echo(line, len, kCmdStart, sizeof(kCmdStart) - 2U)) {
        return SrCmdAckStart;
    }
    if(ends_with_echo(line, len, kCmdStop, sizeof(kCmdStop) - 2U)) {
        return SrCmdAckStop;
    }
    if(ends_with_echo(line, len, kCmdGps, sizeof(kCmdGps) - 2U)) {
        return SrCmdAckGps;
    }
    if(ends_with_echo(line, len, kCmdInfo, sizeof(kCmdInfo) - 2U)) {
        return SrCmdAckInfo;
    }
    if(line_starts(line, len, kPoiTagged, sizeof(kPoiTagged) - 1U)) {
        return SrCmdAckPoi;
    }
    return SrCmdAckNone;
}

static SrParseResult
    marauder_feed(SrParser* parser, const char* line, size_t len, SrEvent* out) {
    const char* gp;
    size_t gn;

    if(parser == NULL || out == NULL) {
        return SrParseUnknown;
    }
    if(line == NULL) {
        len = 0;
    }

    parser->lines_seen++;
    memset(out, 0, sizeof(*out));

    /* D3 dispatch order; first match returns. */
    if(len == 0) {
        emit_unknown(out, line, 0);
        return SrParseUnknown;
    }

    /* D2: normalization is used for GPS matching only. */
    gps_normalize(line, len, &gp, &gn);

    /* D3.2: block header. A second header while already inside a block restarts it, discarding the partial. */
    if(line_eq(gp, gn, kGpsHdr, sizeof(kGpsHdr) - 1U)) {
        memset(&parser->gps_partial, 0, sizeof(parser->gps_partial));
        parser->in_gps_block = true;
        return SrParseNeedMore;
    }

    /* D3.3 / D3.4: an in-block field, or fall back to ordinary dispatch after voiding. */
    if(parser->in_gps_block) {
        SrParseResult gps_r;

        if(gps_fill_field(parser, gp, gn, out, &gps_r)) {
            return gps_r;
        }
        parser->in_gps_block = false;
    }

    if(line_ends(line, len, ",WIFI", 5) || line_ends(line, len, ",BLE", 4)) {
        return parse_csv(parser, line, len, out);
    }

    if(line_eq(line, len, kStart, sizeof(kStart) - 1U)) {
        out->kind = SrEventScanStarted;
        parser->in_session = true;
        return SrParseOk;
    }
    if(line_eq(line, len, kStopWifi, sizeof(kStopWifi) - 1U)) {
        out->kind = SrEventScanStopped;
        out->u.stop = SrStopWifiTranRecv;
        parser->in_session = false;
        return SrParseOk;
    }
    if(line_eq(line, len, kStopNmea, sizeof(kStopNmea) - 1U)) {
        out->kind = SrEventScanStopped;
        out->u.stop = SrStopEndNmea;
        parser->in_session = false;
        return SrParseOk;
    }
    if(line_eq(line, len, kStopGps, sizeof(kStopGps) - 1U)) {
        out->kind = SrEventScanStopped;
        out->u.stop = SrStopGpsUpdates;
        parser->in_session = false;
        return SrParseOk;
    }

    if(probe_n(line, len, &parser->fw_partial)) {
        out->kind = SrEventFirmware;
        out->u.firmware = parser->fw_partial;
        return SrParseOk;
    }

    /* T4.9: L2 side-channel bookkeeping. Must happen before emit_unknown, and must not
     * change the return value or the event type.
     * Placed here rather than at the top of the function so that any already-recognized
     * row (CSV / kStart / kStop* / probe / GPS block field) can never be stolen by an
     * echo match -- the dispatch order is itself the first guard against misclassification. */
    {
        SrCmdAckClass cls = cmdack_class(line, len);
        if(cls != SrCmdAckNone) {
            parser->cmdack.count[cls]++;
            parser->cmdack.rev++;
        }
    }
    emit_unknown(out, line, len);
    return SrParseUnknown;
}

const SrSourceCodec sr_codec_marauder = {
    .name = "marauder",
    .build_start_cmd = marauder_start,
    .build_stop_cmd = marauder_stop,
    .probe_line = marauder_probe,
    .feed_line = marauder_feed,
    .build_gps_cmd = marauder_gps,
    .build_poi_cmd = marauder_poi,
};
