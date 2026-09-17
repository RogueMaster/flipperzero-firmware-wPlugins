// fap/pmkid_report.c — see pmkid_report.h. Pure logic, no furi deps.
#include "pmkid_report.h"

#include <stdio.h>
#include <string.h>

static int hexval(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Parse "AA:BB:CC:DD:EE:FF" (exactly six colon-separated hex pairs) into out[6].
static bool parse_mac(const char* s, uint8_t out[6]) {
    for(int i = 0; i < 6; ++i) {
        int hi = hexval(s[0]);
        if(hi < 0) return false;  // short-circuit before reading s[1]: a token ending
        int lo = hexval(s[1]);    // right after the high nibble must not read past the NUL
        if(lo < 0) return false;
        out[i] = (uint8_t)((hi << 4) | lo);
        if(i < 5) {
            if(s[2] != ':') return false;
            s += 3;
        }
    }
    return true;
}

// Copy a space/CR/LF/NUL-delimited token from s into out[cap] (always
// NUL-terminated). Returns false when the token was TRUNCATED because it did not
// fit cap (loop stopped mid-token on a real char, not a delimiter). Exact-length
// fields (pmkid/essid) rely on this: an over-long value clipped to the buffer size
// would otherwise pass the 32-hex / even-length checks below as if well-formed.
static bool copy_token(const char* s, char* out, size_t cap) {
    size_t n = 0;
    while(s[n] != '\0' && s[n] != ' ' && s[n] != '\r' && s[n] != '\n' && n + 1 < cap) {
        out[n] = s[n];
        ++n;
    }
    out[n] = '\0';
    return s[n] == '\0' || s[n] == ' ' || s[n] == '\r' || s[n] == '\n';
}

// True when h is non-empty and every char is the same hex nibble as c (case-insensitive).
static bool all_same_hex(const char* h, char c) {
    if(h[0] == '\0') return false;
    int want = hexval(c);
    for(size_t i = 0; h[i] != '\0'; ++i) {
        int a = hexval(h[i]);
        if(a < 0 || a != want) return false;
    }
    return true;
}

static bool is_hex_string(const char* h, size_t len) {
    for(size_t i = 0; i < len; ++i)
        if(hexval(h[i]) < 0) return false;
    return true;
}

static G5PmkidState classify_fail(const char* reason) {
    if(strcmp(reason, "no-beacon") == 0 || strcmp(reason, "no-auth") == 0 ||
       strcmp(reason, "no-assoc") == 0 || strcmp(reason, "no-m1") == 0)
        return G5PmkidTimeout;
    if(strcmp(reason, "auth-rejected") == 0 || strcmp(reason, "assoc-rejected") == 0)
        return G5PmkidPartial;
    // no-pmkid / pmf-required / unsupported-cipher / unsupported-akm / hidden-ssid /
    // no-rates / no-rsn / malformed-rsn / invalid-pmkid
    return G5PmkidUnsupported;
}

bool g5_pmkid_parse_line(const char* line, G5PmkidReport* out) {
    if(line == NULL || out == NULL) return false;
    if(strncmp(line, "PMKID:", 6) != 0) return false;
    const char* p = line + 6;

    if(strncmp(p, "on", 2) == 0 || strncmp(p, "st=", 3) == 0) {
        out->state = G5PmkidPending;
        return false;
    }
    if(strncmp(p, "ok ", 3) == 0) {
        // Order-agnostic field extraction (firmware emits bssid, sta, essid_hex, pmkid).
        const char* b = strstr(p, "bssid=");
        const char* s = strstr(p, "sta=");
        const char* e = strstr(p, "essid_hex=");
        const char* k = strstr(p, "pmkid=");
        if(b == NULL || s == NULL || e == NULL || k == NULL) return false;
        if(!parse_mac(b + 6, out->bssid)) return false;
        if(!parse_mac(s + 4, out->sta)) return false;
        // Reject a truncated (over-long) essid_hex/pmkid_hex as a malformed line so
        // it can't be clipped to a value that sneaks past the length checks below.
        if(!copy_token(e + 10, out->essid_hex, sizeof(out->essid_hex))) return false;
        if(!copy_token(k + 6, out->pmkid_hex, sizeof(out->pmkid_hex))) return false;
        out->reason[0] = '\0';
        out->ok = true;
        out->state = G5PmkidValid;  // provisional — refine via g5_pmkid_classify_ok
        return true;
    }
    if(strncmp(p, "fail=", 5) == 0) {
        copy_token(p + 5, out->reason, sizeof(out->reason));
        out->ok = false;
        out->state = classify_fail(out->reason);
        return true;
    }
    if(strncmp(p, "err=", 4) == 0) {
        copy_token(p + 4, out->reason, sizeof(out->reason));
        out->ok = false;
        out->state = G5PmkidError;
        return true;
    }
    return false;
}

G5PmkidState g5_pmkid_classify_ok(
    const G5PmkidReport* report, const uint8_t requested_bssid[6], bool already_seen) {
    if(report == NULL || !report->ok) return G5PmkidError;
    // Wrong target first — a mismatch means this isn't our AP regardless of content.
    if(requested_bssid != NULL && memcmp(report->bssid, requested_bssid, 6) != 0)
        return G5PmkidWrongTarget;
    // Secondary degenerate-pmkid guard (defense in depth behind the firmware's own check).
    if(strlen(report->pmkid_hex) != 32 || !is_hex_string(report->pmkid_hex, 32) ||
       all_same_hex(report->pmkid_hex, '0') || all_same_hex(report->pmkid_hex, 'f'))
        return G5PmkidUnsupported;
    size_t elen = strlen(report->essid_hex);
    if(elen == 0 || (elen % 2) != 0 || !is_hex_string(report->essid_hex, elen))
        return G5PmkidUnsupported;
    if(already_seen) return G5PmkidDuplicate;
    return G5PmkidValid;
}

size_t g5_pmkid_build_22000(const G5PmkidReport* report, char* out, size_t cap) {
    if(report == NULL || out == NULL || cap == 0) return 0;
    if(!report->ok) return 0;
    if(strlen(report->pmkid_hex) != 32 || !is_hex_string(report->pmkid_hex, 32)) return 0;
    if(all_same_hex(report->pmkid_hex, '0') || all_same_hex(report->pmkid_hex, 'f')) return 0;
    size_t elen = strlen(report->essid_hex);
    if(elen == 0 || (elen % 2) != 0 || !is_hex_string(report->essid_hex, elen)) return 0;

    int n = snprintf(
        out, cap,
        "WPA*01*%s*%02x%02x%02x%02x%02x%02x*%02x%02x%02x%02x%02x%02x*%s***",
        report->pmkid_hex,
        (unsigned)report->bssid[0], (unsigned)report->bssid[1], (unsigned)report->bssid[2],
        (unsigned)report->bssid[3], (unsigned)report->bssid[4], (unsigned)report->bssid[5],
        (unsigned)report->sta[0], (unsigned)report->sta[1], (unsigned)report->sta[2],
        (unsigned)report->sta[3], (unsigned)report->sta[4], (unsigned)report->sta[5],
        report->essid_hex);
    if(n < 0 || (size_t)n >= cap) return 0;
    return (size_t)n;
}
