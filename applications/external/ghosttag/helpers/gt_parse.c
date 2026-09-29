#include "gt_parse.h"

#include <string.h>

#define MAC_HEX_LEN 12

static int hex_nibble(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Bounded, sign-aware, and saturating. The previous implementation used atoi()
 * and cast the result straight to int8_t, so "GT1,...,999,1" arrived as a
 * signal strength of -25 dBm and "-999" as +25 - both of which sail past every
 * range check downstream because they are perfectly ordinary int8_t values. */
static bool parse_rssi(const char* s, size_t len, int8_t* out) {
    if(len == 0) return false;
    size_t i = 0;
    bool neg = false;
    if(s[0] == '-' || s[0] == '+') {
        neg = (s[0] == '-');
        i = 1;
        if(len == 1) return false;
    }
    long v = 0;
    for(; i < len; i++) {
        if(s[i] < '0' || s[i] > '9') return false;
        v = v * 10 + (s[i] - '0');
        if(v > 100000L) v = 100000L; /* saturate rather than overflow */
    }
    if(neg) v = -v;
    /* A real BLE receiver cannot report outside this, so anything else is a
     * malformed or hostile line rather than a weak signal. */
    if(v > 20 || v < -128) return false;
    *out = (int8_t)v;
    return true;
}

bool gt_line_is_detection(const char* line) {
    return strncmp(line, "GT1,", 4) == 0;
}

bool gt_line_is_hello(const char* line, const char** version_out) {
    if(strncmp(line, "GTHELLO,", 8) != 0) return false;
    if(version_out) *version_out = line + 8;
    return true;
}

bool gt_line_is_alive(const char* line) {
    return strncmp(line, "GTALIVE", 7) == 0;
}

bool gt_parse_detection(const char* line, GtDetection* out) {
    if(!line || !out) return false;

    /* --- field 1: the address, exactly twelve hex digits --- */
    const char* c1 = strchr(line, ',');
    if(!c1) return false;
    size_t mac_len = (size_t)(c1 - line);
    /* Exact, not "at least": accepting a longer field let a 20-character
     * address through while only the first twelve were read, so two different
     * devices could collapse into one record. */
    if(mac_len != MAC_HEX_LEN) return false;

    uint8_t mac[6];
    for(int i = 0; i < 6; i++) {
        int hi = hex_nibble(line[i * 2]);
        int lo = hex_nibble(line[i * 2 + 1]);
        if(hi < 0 || lo < 0) return false;
        mac[i] = (uint8_t)((hi << 4) | lo);
    }

    /* --- field 2: RSSI --- */
    const char* rssi_s = c1 + 1;
    const char* c2 = strchr(rssi_s, ',');
    if(!c2) return false;
    int8_t rssi;
    if(!parse_rssi(rssi_s, (size_t)(c2 - rssi_s), &rssi)) return false;

    /* --- field 3: type code, and an optional name --- */
    const char* type_s = c2 + 1;
    const char* c3 = strchr(type_s, ',');
    size_t type_len = c3 ? (size_t)(c3 - type_s) : strlen(type_s);
    if(type_len == 0 || type_len > 3) return false;
    unsigned code = 0;
    for(size_t i = 0; i < type_len; i++) {
        if(type_s[i] < '0' || type_s[i] > '9') return false;
        code = code * 10 + (unsigned)(type_s[i] - '0');
    }
    if(code > 255) return false;

    memcpy(out->mac, mac, 6);
    out->rssi = rssi;
    out->type = tracker_type_from_code((uint8_t)code);

    out->name[0] = '\0';
    if(c3) {
        const char* name = c3 + 1;
        size_t n = strlen(name);
        if(n > sizeof(out->name) - 1) n = sizeof(out->name) - 1;
        memcpy(out->name, name, n);
        out->name[n] = '\0';
    }
    return true;
}
