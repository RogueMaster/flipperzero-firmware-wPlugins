// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "flock_store.h"
#include "report_escape.h" // csv_field_escape (the write direction)
#include "report_fmt.h" // fmt_mac / fmt_coord

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Worst-case escaped SSID: every one of 33 chars is a quote (doubled), plus the
// wrapping quote pair and the NUL.
#define ESC_SSID_MAX (FLOCK_STORE_SSID_LEN * 2 + 3)

/** Hex nibble value, or -1 if `c` is not a hex digit. */
static int fs_hex_nibble(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/** Strict "AA:BB:CC:DD:EE:FF" -> 6 bytes. Anything else fails. */
static bool fs_parse_mac(const char* s, uint8_t out[6]) {
    if(strlen(s) != 17) return false;
    for(int b = 0; b < 6; b++) {
        int at = b * 3;
        if(b > 0 && s[at - 1] != ':') return false;
        int hi = fs_hex_nibble(s[at]);
        int lo = fs_hex_nibble(s[at + 1]);
        if(hi < 0 || lo < 0) return false;
        out[b] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

/**
 * Read the next RFC-4180 field starting at *p into `out`, unwrapping a quoted
 * field and collapsing doubled quotes. Advances *p past the field's comma (or to
 * the terminating NUL on the last field). Returns false on an unterminated
 * quote. Content longer than `out_len` is truncated, not overflowed.
 *
 * `*had_comma` reports whether a separator followed, which is how the caller
 * counts columns exactly: a short line would otherwise read as trailing empty
 * fields, and empty trailing fields are legal here (an entry with no GPS fix).
 *
 * report_escape.h only has the escape direction -- this is its inverse, and the
 * round trip is what the host tests actually assert.
 */
static bool fs_next_field(const char** p, char* out, size_t out_len, bool* had_comma) {
    const char* s = *p;
    size_t w = 0;

    if(*s == '"') {
        s++;
        for(;;) {
            if(*s == '\0') return false; // unterminated quote
            if(*s == '"') {
                if(s[1] == '"') { // escaped quote -> one literal "
                    if(w + 1 < out_len) out[w++] = '"';
                    s += 2;
                    continue;
                }
                s++; // closing quote
                break;
            }
            if(w + 1 < out_len) out[w++] = *s;
            s++;
        }
        // Only a comma or end-of-line may follow a closing quote.
        if(*s != ',' && *s != '\0') return false;
    } else {
        while(*s != ',' && *s != '\0') {
            if(w + 1 < out_len) out[w++] = *s;
            s++;
        }
    }

    out[w] = '\0';
    *had_comma = (*s == ',');
    *p = *had_comma ? s + 1 : s;
    return true;
}

/** Parse a whole unsigned field; false if empty or not entirely numeric. */
static bool fs_parse_u32(const char* s, uint32_t* out) {
    if(*s == '\0') return false;
    char* end = NULL;
    unsigned long v = strtoul(s, &end, 10);
    if(!end || *end != '\0') return false;
    *out = (uint32_t)v;
    return true;
}

/** Parse a whole signed field; false if empty or not entirely numeric. */
static bool fs_parse_i32(const char* s, long* out) {
    if(*s == '\0') return false;
    char* end = NULL;
    long v = strtol(s, &end, 10);
    if(!end || *end != '\0') return false;
    *out = v;
    return true;
}

/** Parse a coordinate field; an EMPTY field means "no fix" and yields NAN. */
static bool fs_parse_coord(const char* s, float* out) {
    if(*s == '\0') {
        *out = NAN;
        return true;
    }
    char* end = NULL;
    float v = strtof(s, &end);
    if(!end || *end != '\0') return false;
    *out = v;
    return true;
}

size_t flock_store_fmt_line(char* out, size_t out_len, const FlockStoreRec* r) {
    char mac_s[18];
    fmt_mac(mac_s, sizeof(mac_s), r->mac);

    // An SSID is arbitrary bytes, and csv_field_escape preserves an embedded
    // CR/LF inside quotes (correct RFC-4180). The loader reads one record per
    // physical line, so such a record would split in two and neither half would
    // parse -- flatten control characters to spaces BEFORE escaping. Lossy by
    // design, and only for bytes that could never be read on the 128 px display
    // anyway.
    char ssid_flat[FLOCK_STORE_SSID_LEN];
    size_t si = 0;
    // Bounds test FIRST: the old order dereferenced r->ssid[si] before
    // checking si was in range. In bounds as written, but backwards.
    for(; si < FLOCK_STORE_SSID_LEN - 1 && r->ssid[si]; si++) {
        unsigned char c = (unsigned char)r->ssid[si];
        ssid_flat[si] = (c < 0x20 || c == 0x7F) ? ' ' : (char)c;
    }
    ssid_flat[si] = '\0';

    char ssid_esc[ESC_SSID_MAX];
    csv_field_escape(ssid_flat, ssid_esc, sizeof(ssid_esc));

    // Empty field for "no fix", so a reader can tell an absent coordinate from
    // a real 0,0 out in the Gulf of Guinea.
    char lat_s[16], lon_s[16], head_s[16];
    fmt_coord(lat_s, sizeof(lat_s), r->lat, "");
    fmt_coord(lon_s, sizeof(lon_s), r->lon, "");
    fmt_coord(head_s, sizeof(head_s), r->heading, "");

    // ftype is one of a small known set; anything else is written as empty
    // rather than risking a comma or quote landing mid-record.
    char ft[2] = {0, 0};
    if(r->ftype && strchr("PBROFL", r->ftype)) ft[0] = r->ftype;

    int n = snprintf(
        out,
        out_len,
        "%s,%s,%d,%u,%s,%u,%08lx,%s,%s,%s,%lu,%u,%lu,%u,%u\n",
        mac_s,
        ssid_esc,
        r->rssi,
        r->channel,
        ft,
        r->conf,
        (unsigned long)r->ie_fp,
        lat_s,
        lon_s,
        head_s,
        (unsigned long)r->count,
        r->marked ? 1u : 0u,
        (unsigned long)r->epoch,
        r->dev_class,
        r->hidden ? 1u : 0u);

    if(n < 0 || (size_t)n >= out_len) {
        if(out_len) out[0] = '\0';
        return 0;
    }
    return (size_t)n;
}

bool flock_store_parse_line(const char* line, FlockStoreRec* out) {
    if(!line || !out) return false;

    // Build into a scratch record so a failure part-way leaves *out untouched.
    FlockStoreRec r;
    memset(&r, 0, sizeof(r));

    // Strip a trailing CRLF/LF without mutating the caller's buffer: the field
    // reader stops at NUL, so copy into a bounded local first.
    char buf[FLOCK_STORE_LINE_MAX];
    size_t len = strlen(line);
    while(len && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        len--;
    if(len == 0 || len >= sizeof(buf)) return false;
    memcpy(buf, line, len);
    buf[len] = '\0';

    char f[FLOCK_STORE_COLS][ESC_SSID_MAX];
    const char* p = buf;
    int ncols = 0;
    for(int i = 0; i < FLOCK_STORE_COLS; i++) {
        bool had_comma = false;
        if(!fs_next_field(&p, f[i], sizeof(f[i]), &had_comma)) return false;
        ncols++;
        if(!had_comma) break; // that was the last column
    }
    if(*p != '\0') return false; // more columns than the schema allows
    // Exactly a v2 line, or exactly a v1 line (v2 minus the trailing class).
    // Any other count is a malformed record, not a version we tolerate.
    if(ncols != FLOCK_STORE_COLS && ncols != FLOCK_STORE_COLS_V1) return false;

    if(!fs_parse_mac(f[0], r.mac)) return false;

    strncpy(r.ssid, f[1], FLOCK_STORE_SSID_LEN - 1);
    r.ssid[FLOCK_STORE_SSID_LEN - 1] = '\0';

    long v;
    if(!fs_parse_i32(f[2], &v) || v < -128 || v > 127) return false;
    r.rssi = (int8_t)v;

    uint32_t u;
    if(!fs_parse_u32(f[3], &u) || u > 255) return false;
    r.channel = (uint8_t)u;

    if(f[4][0] != '\0') {
        if(f[4][1] != '\0' || !strchr("PBROFL", f[4][0])) return false;
        r.ftype = f[4][0];
    }

    if(!fs_parse_u32(f[5], &u) || u > 4) return false; // FlockConfidence rungs
    r.conf = (uint8_t)u;

    // ie_fp is written as %08lx to match the "IE-fp:" readout on the detail
    // screen (which is what a user copies into signatures.json), so read it back
    // as hex -- base 10 here would silently mangle every fingerprint with a
    // letter in it.
    if(f[6][0] == '\0') return false;
    {
        char* end = NULL;
        unsigned long h = strtoul(f[6], &end, 16);
        if(!end || *end != '\0') return false;
        r.ie_fp = (uint32_t)h;
    }

    if(!fs_parse_coord(f[7], &r.lat)) return false;
    if(!fs_parse_coord(f[8], &r.lon)) return false;
    if(!fs_parse_coord(f[9], &r.heading)) return false;

    if(!fs_parse_u32(f[10], &r.count)) return false;

    if(!fs_parse_u32(f[11], &u) || u > 1) return false;
    r.marked = (u != 0);

    if(!fs_parse_u32(f[12], &r.epoch)) return false;

    // v2 only. A v1 line stops at 13 columns and keeps the memset defaults of 0
    // -- FlockClassAlpr and hidden-never-observed, which is what every v1
    // detection actually was.
    if(ncols == FLOCK_STORE_COLS) {
        if(!fs_parse_u32(f[13], &u) || u > 1) return false; // FlockDevClass values
        r.dev_class = (uint8_t)u;
        if(!fs_parse_u32(f[14], &u) || u > 1) return false;
        r.hidden = (u != 0);
    }

    *out = r;
    return true;
}

bool flock_store_schema_supported(const char* line) {
    if(!line) return false;
    return strcmp(line, FLOCK_STORE_SCHEMA) == 0 || strcmp(line, FLOCK_STORE_SCHEMA_V1) == 0;
}

bool flock_store_evict_better(uint8_t conf_a, uint32_t epoch_a, uint8_t conf_b, uint32_t epoch_b) {
    if(conf_a != conf_b) return conf_a < conf_b; // weaker evidence goes first
    return epoch_a < epoch_b; // same rung -> drop the older sighting
}
