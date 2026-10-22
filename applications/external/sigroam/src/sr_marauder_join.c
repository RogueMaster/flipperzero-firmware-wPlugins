#include "sr_marauder_join.h"

#include "sr_types.h"

#include <stdio.h>
#include <string.h>

/*
 * v1.17.0 CommandLine.cpp / WiFiScan.cpp prints. Kept as literals so a host
 * test can reject a drift away from the fixture.
 */
static const char kStop[] = "stopscan\n";
static const char kScan[] = "scanall\n";
static const char kList[] = "list -a\n";
static const char kListMark[] = "#list -a";
static const char kBanner[] = "Scanning for APs and Stations. Stop with stopscan";
static const char kUsing[] = "Using SSID: ";
static const char kFailNet[] = "Could not connect to WiFi network";
static const char kFailArgs[] = "You did not provide the proper args";
static const char kCheck[] = "channel\n";
static const char kWhyFail[] = "Join failed";
static const char kSelected[] = " (selected)";
static const char kSelectedWord[] = " selected";

const char* sr_join_cmd_stop(void) {
    return kStop;
}

const char* sr_join_cmd_scan(void) {
    return kScan;
}

const char* sr_join_cmd_list(void) {
    return kList;
}

const char* sr_join_cmd_check(void) {
    return kCheck;
}

const char* sr_join_fail_why(unsigned receipt) {
    return receipt == SrJoinReceiptFail ? kWhyFail : NULL;
}

static bool eq(const char* line, size_t len, const char* lit) {
    size_t n = 0u;

    while(lit[n] != '\0') {
        n++;
    }
    if(len != n) {
        return false;
    }
    return memcmp(line, lit, n) == 0;
}

static bool starts(const char* line, size_t len, const char* lit) {
    size_t n = 0u;

    while(lit[n] != '\0') {
        n++;
    }
    if(len < n) {
        return false;
    }
    return memcmp(line, lit, n) == 0;
}

static bool ends(const char* line, size_t len, const char* lit) {
    size_t n = 0u;

    while(lit[n] != '\0') {
        n++;
    }
    if(len < n) {
        return false;
    }
    return memcmp(line + (len - n), lit, n) == 0;
}

bool sr_join_password_ok(const char* password) {
    size_t n = 0u;

    if(password == NULL) {
        return false;
    }
    while(n <= (size_t)SR_JOIN_PASS_MAX && password[n] != '\0') {
        unsigned char c = (unsigned char)password[n];
        if(c < 0x21u || c > 0x7Eu || c == (unsigned char)'#' || c == (unsigned char)'"' ||
           c == (unsigned char)'\'') {
            return false;
        }
        n++;
    }
    if(password[n] != '\0') {
        return false;
    }
    return n >= (size_t)SR_JOIN_PASS_MIN && n <= (size_t)SR_JOIN_PASS_MAX;
}

size_t sr_join_build(unsigned index, const char* password, char* dst, size_t cap) {
    int wrote;

    if(dst == NULL || cap == 0u) {
        return 0u;
    }
    dst[0] = '\0';
    if(index > (unsigned)SR_JOIN_INDEX_MAX || !sr_join_password_ok(password)) {
        return 0u;
    }
    wrote = snprintf(dst, cap, "join -a %u -p %s\n", index, password);
    if(wrote < 0 || (size_t)wrote >= cap) {
        dst[0] = '\0';
        return 0u;
    }
    return (size_t)wrote;
}

static bool take_uint(const char* s, size_t n, size_t* i, unsigned* out) {
    size_t k = *i;
    unsigned v = 0u;
    bool any = false;

    while(k < n && s[k] >= '0' && s[k] <= '9') {
        any = true;
        if(v > 9999u) {
            return false;
        }
        v = v * 10u + (unsigned)(s[k] - '0');
        k++;
    }
    if(!any) {
        return false;
    }
    *out = v;
    *i = k;
    return true;
}

bool sr_join_parse_ap(const char* line, size_t len, uint16_t* index, char* ssid, size_t ssid_cap) {
    size_t i = 0u;
    unsigned idx = 0u;
    unsigned channel = 0u;
    size_t rest;
    size_t rssi_at;
    size_t ssid_n;
    size_t copy_n;
    static const char kCh[] = "][CH:";

    if(line == NULL || index == NULL || ssid == NULL || ssid_cap == 0u) {
        return false;
    }
    ssid[0] = '\0';
    if(len < 8u || line[0] != '[') {
        return false;
    }
    i = 1u;
    if(!take_uint(line, len, &i, &idx) || idx > (unsigned)SR_JOIN_INDEX_MAX) {
        return false;
    }
    if(i + sizeof(kCh) - 1u > len || memcmp(line + i, kCh, sizeof(kCh) - 1u) != 0) {
        return false;
    }
    i += sizeof(kCh) - 1u;
    if(!take_uint(line, len, &i, &channel)) {
        return false;
    }
    (void)channel;
    if(i >= len || line[i] != ']') {
        return false;
    }
    i++;
    if(i >= len || line[i] != ' ') {
        return false;
    }
    i++;
    rest = i;
    if(ends(line + rest, len - rest, kSelected)) {
        len -= sizeof(kSelected) - 1u;
    }
    if(len < rest) {
        return false;
    }
    rssi_at = len;
    while(rssi_at > rest && line[rssi_at - 1u] >= '0' && line[rssi_at - 1u] <= '9') {
        rssi_at--;
    }
    if(rssi_at == len) {
        return false;
    }
    if(rssi_at > rest && line[rssi_at - 1u] == '-') {
        rssi_at--;
    }
    if(rssi_at == rest) {
        *index = (uint16_t)idx;
        return true;
    }
    if(line[rssi_at - 1u] != ' ') {
        return false;
    }
    ssid_n = rssi_at - 1u - rest;
    copy_n = ssid_n;
    if(copy_n >= ssid_cap) {
        copy_n = ssid_cap - 1u;
    }
    if(copy_n > 0u) {
        memcpy(ssid, line + rest, copy_n);
    }
    ssid[copy_n] = '\0';
    *index = (uint16_t)idx;
    return true;
}

bool sr_join_is_list_end(const char* line, size_t len) {
    size_t i = 0u;
    unsigned n = 0u;
    size_t word;

    if(line == NULL) {
        return false;
    }
    if(!take_uint(line, len, &i, &n)) {
        return false;
    }
    (void)n;
    word = sizeof(kSelectedWord) - 1u;
    if(i + word > len || memcmp(line + i, kSelectedWord, word) != 0) {
        return false;
    }
    i += word;
    if(i == len) {
        return true;
    }
    return line[i] == ',';
}

bool sr_join_is_scan_banner(const char* line, size_t len) {
    if(line == NULL) {
        return false;
    }
    return eq(line, len, kBanner);
}

bool sr_join_is_using(const char* line, size_t len) {
    if(line == NULL) {
        return false;
    }
    return starts(line, len, kUsing);
}

bool sr_join_is_fail(const char* line, size_t len) {
    if(line == NULL) {
        return false;
    }
    return eq(line, len, kFailNet) || eq(line, len, kFailArgs);
}

static bool find_span(const char* line, size_t len, const char* lit, size_t* at) {
    size_t n = 0u;
    size_t i;

    while(lit[n] != '\0') {
        n++;
    }
    if(n == 0u || len < n) {
        return false;
    }
    for(i = 0u; i + n <= len; i++) {
        if(memcmp(line + i, lit, n) == 0) {
            *at = i;
            return true;
        }
    }
    return false;
}

bool sr_join_redact_echo(char* line, size_t* len, size_t cap) {
    size_t n;
    size_t join_at = 0u;
    size_t p_at = 0u;
    size_t secret;
    static const char kMark[] = "***";

    if(line == NULL || len == NULL || cap == 0u) {
        return false;
    }
    n = *len;
    if(n > cap) {
        n = cap;
    }
    if(!find_span(line, n, "#join", &join_at)) {
        return false;
    }
    if(!find_span(line + join_at, n - join_at, " -p ", &p_at)) {
        return false;
    }
    p_at += join_at;
    secret = p_at + 4u;
    if(secret + sizeof(kMark) - 1u >= cap) {
        return false;
    }
    memcpy(line + secret, kMark, sizeof(kMark) - 1u);
    *len = secret + sizeof(kMark) - 1u;
    if(*len < cap) {
        line[*len] = '\0';
    }
    return true;
}

/* v1.17.0 WiFiScan.cpp boot import prints the stored value after these. */
static const char* const kKeyMarks[] = {
    "Saved WiGLE API Name: ",
    "Saved WiGLE API Token: ",
    "Saved WDG API Token: ",
};

static unsigned key_find(const char* line, size_t len, size_t* value_at) {
    size_t k;
    size_t at = 0u;

    if(line == NULL) {
        return SrJoinKeyNone;
    }
    for(k = 0u; k < sizeof(kKeyMarks) / sizeof(kKeyMarks[0]); k++) {
        if(find_span(line, len, kKeyMarks[k], &at)) {
            if(value_at != NULL) {
                *value_at = at + strlen(kKeyMarks[k]);
            }
            return (unsigned)(SrJoinKeyWigleName + k);
        }
    }
    return SrJoinKeyNone;
}

unsigned sr_join_key_kind(const char* line, size_t len) {
    return key_find(line, len, NULL);
}

bool sr_join_redact_key(char* line, size_t* len, size_t cap) {
    size_t n;
    size_t value = 0u;
    static const char kMark[] = "***";

    if(line == NULL || len == NULL || cap == 0u) {
        return false;
    }
    n = *len;
    if(n > cap) {
        n = cap;
    }
    if(key_find(line, n, &value) == SrJoinKeyNone) {
        return false;
    }
    if(value + sizeof(kMark) - 1u < cap) {
        memcpy(line + value, kMark, sizeof(kMark) - 1u);
        n = value + sizeof(kMark) - 1u;
    } else {
        n = value;
    }
    memset(line + n, 0, cap - n);
    *len = n;
    return true;
}

typedef struct {
    unsigned mode;
    bool saw_scan;
    bool saw_end;
    bool saw_list_echo;
    bool saw_join_echo;
    bool expect_check;
    bool expected_index_set;
    uint16_t expected_index;
    bool saw_using;
    bool saw_fail;
    bool marked;
    bool have_essid;
    bool essid_rejected;
    unsigned receipt;
    size_t essid_len;
    char essid[SR_SSID_MAX + 1u];
    size_t n;
    uint16_t index[SR_JOIN_AP_CAP];
    char ssid[SR_JOIN_AP_CAP][SR_JOIN_SSID_MAX + 1u];
} JoinListen;

static JoinListen g_listen;

void sr_join_listen(unsigned mode) {
    size_t i;

    g_listen.mode = mode;
    g_listen.saw_scan = false;
    g_listen.saw_end = false;
    g_listen.saw_list_echo = false;
    g_listen.saw_join_echo = false;
    g_listen.expect_check = false;
    g_listen.expected_index_set = false;
    g_listen.expected_index = 0u;
    g_listen.saw_using = false;
    g_listen.saw_fail = false;
    g_listen.marked = false;
    g_listen.have_essid = false;
    g_listen.essid_rejected = false;
    g_listen.receipt = SrJoinReceiptPending;
    g_listen.essid_len = 0u;
    g_listen.essid[0] = '\0';
    g_listen.n = 0u;
    for(i = 0u; i < (size_t)SR_JOIN_AP_CAP; i++) {
        g_listen.index[i] = 0u;
        g_listen.ssid[i][0] = '\0';
    }
}

bool sr_join_listening(void) {
    return g_listen.mode != (unsigned)SrJoinListenOff;
}

static bool rest_after(
    const char* line,
    size_t len,
    const char* lit,
    char* out,
    size_t cap,
    size_t* out_len) {
    size_t n = 0u;
    size_t rest;

    while(lit[n] != '\0') {
        n++;
    }
    if(len < n || memcmp(line, lit, n) != 0) {
        return false;
    }
    rest = len - n;
    if(rest >= cap) {
        return false;
    }
    if(rest > 0u) {
        memcpy(out, line + n, rest);
    }
    out[rest] = '\0';
    *out_len = rest;
    return true;
}

static void take_essid(const char* line, size_t len) {
    size_t n = 0u;

    if(g_listen.have_essid || g_listen.essid_rejected) {
        return;
    }
    if(len >= (size_t)SR_RAW_LINE_MAX ||
       !rest_after(line, len, kUsing, g_listen.essid, sizeof(g_listen.essid), &n) ||
       n > (size_t)SR_SSID_MAX) {
        g_listen.essid_rejected = true;
        g_listen.essid[0] = '\0';
        g_listen.essid_len = 0u;
        return;
    }
    g_listen.essid_len = n;
    g_listen.have_essid = true;
}

static bool is_join_echo(const char* line, size_t len) {
    char mark[40];
    int n;

    if(!g_listen.expected_index_set) {
        return false;
    }
    if(starts(line, len, "> ")) {
        line += 2;
        len -= 2u;
    }
    n = snprintf(mark, sizeof(mark), "#join -a %u -p ", (unsigned)g_listen.expected_index);
    return n > 0 && (size_t)n < sizeof(mark) && starts(line, len, mark);
}

static bool is_check_echo(const char* line, size_t len) {
    return eq(line, len, "#channel") || eq(line, len, "> #channel") ||
           (starts(line, len, "Connecting to WiFi") && ends(line, len, "> #channel"));
}

void sr_join_observe(const char* line, size_t len) {
    uint16_t index = 0u;

    if(line == NULL || g_listen.mode == SrJoinListenOff) {
        return;
    }
    if(g_listen.mode == SrJoinListenScan && sr_join_is_scan_banner(line, len)) {
        g_listen.saw_scan = true;
        return;
    }
    if(g_listen.mode == SrJoinListenList && !g_listen.saw_end) {
        if(!g_listen.saw_list_echo) {
            if(ends(line, len, kListMark)) {
                g_listen.saw_list_echo = true;
            }
            return;
        }
        if(sr_join_is_list_end(line, len)) {
            g_listen.saw_end = true;
            return;
        }
        if(g_listen.n < (size_t)SR_JOIN_AP_CAP &&
           sr_join_parse_ap(
               line, len, &index, g_listen.ssid[g_listen.n], sizeof(g_listen.ssid[0]))) {
            g_listen.index[g_listen.n] = index;
            g_listen.n++;
        }
        return;
    }
    if(g_listen.mode != SrJoinListenJoin) {
        return;
    }
    if(!g_listen.saw_join_echo) {
        g_listen.saw_join_echo = is_join_echo(line, len);
        return;
    }
    if(sr_join_is_using(line, len)) {
        g_listen.saw_using = true;
        take_essid(line, len);
        return;
    }
    if(g_listen.marked) {
        return;
    }
    if(sr_join_is_fail(line, len)) {
        g_listen.saw_fail = true;
        g_listen.receipt = SrJoinReceiptFail;
        return;
    }
    if(g_listen.expect_check && is_check_echo(line, len)) {
        g_listen.marked = true;
        if(g_listen.have_essid && !g_listen.essid_rejected &&
           g_listen.receipt == SrJoinReceiptPending) {
            g_listen.receipt = SrJoinReceiptJoined;
        }
    }
}

bool sr_join_saw_scan(void) {
    return g_listen.saw_scan;
}

bool sr_join_saw_list_end(void) {
    return g_listen.saw_end;
}

bool sr_join_saw_using(void) {
    return g_listen.saw_using;
}

bool sr_join_saw_fail(void) {
    return g_listen.saw_fail;
}

unsigned sr_join_receipt(void) {
    return g_listen.receipt;
}

void sr_join_expect_index(uint16_t index) {
    g_listen.expected_index = index;
    g_listen.expected_index_set = true;
}

void sr_join_expect_check(void) {
    g_listen.expect_check = true;
}

size_t sr_join_ap_count(void) {
    return g_listen.n;
}

bool sr_join_ap_at(size_t i, uint16_t* index, char* ssid, size_t ssid_cap) {
    size_t n;
    size_t c;

    if(i >= g_listen.n || index == NULL || ssid == NULL || ssid_cap == 0u) {
        return false;
    }
    *index = g_listen.index[i];
    n = 0u;
    while(g_listen.ssid[i][n] != '\0' && n < (size_t)SR_JOIN_SSID_MAX) {
        n++;
    }
    c = n;
    if(c >= ssid_cap) {
        c = ssid_cap - 1u;
    }
    if(c > 0u) {
        memcpy(ssid, g_listen.ssid[i], c);
    }
    ssid[c] = '\0';
    return true;
}

bool sr_join_ap_matches(uint16_t index, const char* ssid) {
    size_t i;
    size_t found = 0u;

    if(!g_listen.saw_list_echo || !g_listen.saw_end || ssid == NULL || ssid[0] == '\0') {
        return false;
    }
    for(i = 0u; i < g_listen.n; i++) {
        if(g_listen.index[i] == index) {
            found++;
            if(strcmp(g_listen.ssid[i], ssid) != 0) {
                return false;
            }
        }
    }
    return found == 1u;
}

bool sr_join_using_matches(const char* ssid) {
    return ssid != NULL && g_listen.have_essid && !g_listen.essid_rejected &&
           strcmp(g_listen.essid, ssid) == 0;
}
