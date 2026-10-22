#include "sr_marauder_upload.h"

#include "sr_marauder_join.h"
#include "sr_types.h"

#include <string.h>

/*
 * One app, one flight. apply_unknown and the Upload scene both run under
 * the app mutex, so this record is not torn between them.
 *
 * Screen lines are at most SR_MAU_COLS. They say what the operator can do.
 * A per-profile "Could not connect to WiFi network" is not terminal:
 * joinSavedWiFi may still reach another profile. "Timeout reached" is only
 * a hint; Upload OK after it is still a send.
 */

enum {
    MauIdle = 0,
    MauSending,
    MauNoWifi,
    MauNoReach,
    MauNoKey,
    MauSent,
    MauAlready,
    MauRejected,
    MauNoRoute,
    MauNoReply,
    MauNoRam,
    MauNoFile,
    MauNoOk,
    MauNoLogs,
    MauUnsupported
};

typedef struct {
    unsigned face;
    bool in_flight;
    bool saw_uploading;
    bool saw_ok;
    bool saw_succeeded;
    bool saw_skip;
    bool saw_fail;
    bool joining;
    bool wifi_ok;
    bool wifi_joined;
    unsigned tries;
    bool fail_route;
    bool fail_reply;
    bool fail_ram;
    bool fail_file;
    bool file_open;
    bool file_ok;
    bool file_fail;
    bool file_200;
    uint32_t n_ok;
    uint32_t n_fail;
    bool wigle200;
    uint32_t rev;
    char name[SR_MAU_COLS + 1u];
    char ok_name[SR_MAU_COLS + 1u];
    char fail_name[SR_MAU_COLS + 1u];
} MauUp;

static MauUp g_mau;

/* Survives sr_mau_up_reset: the import happens once, at board boot. */
static unsigned g_key_seen;

/* SSID of the last selected-network Join on this app run. Never drawn. */
static char g_joined[SR_SSID_MAX + 1u];

_Static_assert(sizeof("Ready") - 1u <= SR_MAU_COLS, "Ready");
_Static_assert(sizeof("press Upload") - 1u <= SR_MAU_COLS, "press Upload");
_Static_assert(sizeof("Joining Wi-Fi") - 1u <= SR_MAU_COLS, "Joining Wi-Fi");
_Static_assert(sizeof("keep card in") - 1u <= SR_MAU_COLS, "keep card in");
_Static_assert(sizeof("No saved Wi-Fi") - 1u <= SR_MAU_COLS, "No saved Wi-Fi");
_Static_assert(sizeof("Join WiFi first") - 1u <= SR_MAU_COLS, "Join WiFi first");
_Static_assert(sizeof("Can't reach Wi-Fi") - 1u <= SR_MAU_COLS, "Can't reach Wi-Fi");
_Static_assert(sizeof("try Join again") - 1u <= SR_MAU_COLS, "try Join again");
_Static_assert(sizeof("put key on card") - 1u <= SR_MAU_COLS, "put key on card");
_Static_assert(sizeof("Sent to WiGLE") - 1u <= SR_MAU_COLS, "Sent to WiGLE");
_Static_assert(sizeof("check account") - 1u <= SR_MAU_COLS, "check account");
_Static_assert(sizeof("Already sent") - 1u <= SR_MAU_COLS, "Already sent");
_Static_assert(sizeof("on WiGLE") - 1u <= SR_MAU_COLS, "on WiGLE");
_Static_assert(sizeof("WiGLE rejected") - 1u <= SR_MAU_COLS, "WiGLE rejected");
_Static_assert(sizeof("try again") - 1u <= SR_MAU_COLS, "try again");
_Static_assert(sizeof("Can't reach WiGLE") - 1u <= SR_MAU_COLS, "Can't reach WiGLE");
_Static_assert(sizeof("try again later") - 1u <= SR_MAU_COLS, "try again later");
_Static_assert(sizeof("WiGLE timed out") - 1u <= SR_MAU_COLS, "WiGLE timed out");
_Static_assert(sizeof("Board low on RAM") - 1u <= SR_MAU_COLS, "Board low on RAM");
_Static_assert(sizeof("Log not on card") - 1u <= SR_MAU_COLS, "Log not on card");
_Static_assert(sizeof("missing") - 1u <= SR_MAU_COLS, "missing");
_Static_assert(sizeof("No confirmation") - 1u <= SR_MAU_COLS, "No confirmation");
_Static_assert(sizeof("nothing sent") - 1u <= SR_MAU_COLS, "nothing sent");
_Static_assert(sizeof("No logs in list") - 1u <= SR_MAU_COLS, "No logs in list");
_Static_assert(sizeof("Upload not in build") - 1u <= SR_MAU_COLS, "Upload not in build");
_Static_assert(sizeof("not supported") - 1u <= SR_MAU_COLS, "not supported");
_Static_assert(sizeof("Wi-Fi connected") - 1u <= SR_MAU_COLS, "Wi-Fi connected");
_Static_assert(sizeof("try N, keep card in") - 1u <= SR_MAU_COLS, "try N");
_Static_assert(sizeof("Used joined Wi-Fi") - 1u <= SR_MAU_COLS, "Used joined Wi-Fi");
_Static_assert(sizeof("Wi-Fi OK") - 1u <= SR_MAU_COLS, "Wi-Fi OK");
_Static_assert(sizeof("Check wigle.net") - 1u <= SR_MAU_COLS, "Check wigle.net");

static void mau_bump(void) {
    g_mau.rev++;
}

static bool mau_eq(const char* line, size_t len, const char* lit) {
    size_t n = 0u;

    while(lit[n] != '\0') {
        n++;
    }
    if(len != n) {
        return false;
    }
    for(n = 0u; n < len; n++) {
        if(line[n] != lit[n]) {
            return false;
        }
    }
    return true;
}

/*
 * joinWiFi prints "Connecting to WiFi" plus dots with no newline
 * (WiFiScan.cpp 2432-2435, v1.18.0 b2b9f92). joinSavedWiFi then prints
 * "Connected to saved WiFi: " on the same line (2586). Lab tape 2026-10-15.
 * A line that is only the receipt, with no prefix, still matches.
 */
static bool mau_saved_wifi(const char* line, size_t len, size_t* out) {
    static const char kJoin[] = "Connecting to WiFi";
    static const char kConn[] = "Connected to saved WiFi: ";
    size_t i = 0u;
    size_t n;

    n = sizeof(kJoin) - 1u;
    if(len >= n && memcmp(line, kJoin, n) == 0) {
        i = n;
        while(i < len && line[i] == '.') {
            i++;
        }
    }
    n = sizeof(kConn) - 1u;
    if(len < i + n) {
        return false;
    }
    if(memcmp(line + i, kConn, n) != 0) {
        return false;
    }
    if(out != NULL) {
        *out = i + n;
    }
    return true;
}

static bool mau_starts(const char* line, size_t len, const char* lit, size_t* out) {
    size_t n = 0u;

    while(lit[n] != '\0') {
        n++;
    }
    if(len < n) {
        return false;
    }
    for(size_t i = 0u; i < n; i++) {
        if(line[i] != lit[i]) {
            return false;
        }
    }
    if(out != NULL) {
        *out = n;
    }
    return true;
}

static void mau_put(char* dst, const char* src) {
    size_t i = 0u;

    if(dst == NULL) {
        return;
    }
    if(src == NULL) {
        src = "";
    }
    while(i < (size_t)SR_MAU_COLS && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void mau_clear(char* dst) {
    size_t i;

    for(i = 0u; i < (size_t)SR_MAU_COLS + 1u; i++) {
        dst[i] = '\0';
    }
}

static void mau_close_file(void) {
    if(!g_mau.file_open) {
        return;
    }
    if(g_mau.file_fail) {
        if(g_mau.n_fail < UINT32_MAX) {
            g_mau.n_fail++;
        }
    } else if(g_mau.file_ok) {
        if(g_mau.n_ok < UINT32_MAX) {
            g_mau.n_ok++;
        }
    }
    if(g_mau.file_200) {
        g_mau.wigle200 = true;
    }
    g_mau.file_open = false;
    g_mau.file_ok = false;
    g_mau.file_fail = false;
    g_mau.file_200 = false;
}

static void mau_open_file(void) {
    mau_close_file();
    g_mau.file_open = true;
}

static void mau_keep_ok(void) {
    g_mau.file_ok = true;
    if(g_mau.name[0] != '\0') {
        mau_put(g_mau.ok_name, g_mau.name);
    }
}

static void mau_keep_fail(void) {
    g_mau.file_fail = true;
    g_mau.saw_fail = true;
    if(g_mau.fail_name[0] == '\0' && g_mau.name[0] != '\0') {
        mau_put(g_mau.fail_name, g_mau.name);
    }
}

static void mau_set_name(const char* line, size_t len, size_t prefix) {
    size_t name_len;
    size_t i;
    size_t n;

    if(len < prefix + 3u) {
        return;
    }
    if(line[len - 3u] != '.' || line[len - 2u] != '.' || line[len - 1u] != '.') {
        return;
    }
    name_len = len - prefix - 3u;
    n = name_len;
    if(n > (size_t)SR_MAU_COLS) {
        n = (size_t)SR_MAU_COLS;
    }
    for(i = 0u; i < n; i++) {
        g_mau.name[i] = line[prefix + i];
    }
    g_mau.name[n] = '\0';
}

static void mau_finish_complete(void) {
    mau_close_file();
    g_mau.in_flight = false;
    if(!g_mau.saw_uploading) {
        g_mau.face = MauNoLogs;
    } else if(g_mau.saw_fail) {
        if(g_mau.fail_route) {
            g_mau.face = MauNoRoute;
        } else if(g_mau.fail_reply) {
            g_mau.face = MauNoReply;
        } else if(g_mau.fail_ram) {
            g_mau.face = MauNoRam;
        } else if(g_mau.fail_file) {
            g_mau.face = MauNoFile;
        } else {
            g_mau.face = MauRejected;
        }
    } else if(g_mau.saw_succeeded || (g_mau.saw_ok && !g_mau.saw_skip)) {
        g_mau.face = MauSent;
    } else if(g_mau.saw_skip && g_mau.saw_ok) {
        g_mau.face = MauAlready;
    } else if(g_mau.saw_ok) {
        g_mau.face = MauSent;
    } else {
        g_mau.face = MauNoOk;
    }
    mau_bump();
}

static void mau_end(unsigned face) {
    mau_close_file();
    g_mau.in_flight = false;
    g_mau.face = face;
    mau_bump();
}

static void mau_clear_flight(void) {
    g_mau.face = MauIdle;
    g_mau.in_flight = false;
    g_mau.saw_uploading = false;
    g_mau.saw_ok = false;
    g_mau.saw_succeeded = false;
    g_mau.saw_skip = false;
    g_mau.saw_fail = false;
    g_mau.joining = false;
    g_mau.wifi_ok = false;
    g_mau.wifi_joined = false;
    g_mau.tries = 0u;
    g_mau.fail_route = false;
    g_mau.fail_reply = false;
    g_mau.fail_ram = false;
    g_mau.fail_file = false;
    g_mau.file_open = false;
    g_mau.file_ok = false;
    g_mau.file_fail = false;
    g_mau.file_200 = false;
    g_mau.n_ok = 0u;
    g_mau.n_fail = 0u;
    g_mau.wigle200 = false;
    mau_clear(g_mau.name);
    mau_clear(g_mau.ok_name);
    mau_clear(g_mau.fail_name);
}

void sr_mau_up_reset(void) {
    mau_clear_flight();
    g_mau.rev = 0u;
}

void sr_mau_up_on_send(void) {
    uint32_t rev = g_mau.rev;

    mau_clear_flight();
    g_mau.rev = rev;
    g_mau.in_flight = true;
    g_mau.face = MauSending;
    mau_bump();
}

void sr_mau_up_feed(const char* line, size_t len) {
    size_t prefix = 0u;
    unsigned key;

    if(line == NULL) {
        return;
    }
    key = sr_join_key_kind(line, len);
    if(key != SrJoinKeyNone) {
        g_key_seen |= 1u << key;
        mau_bump();
        return;
    }
    if(!g_mau.in_flight) {
        return;
    }
    if(mau_eq(line, len, "There are no saved WiFi credentials")) {
        mau_end(MauNoWifi);
        return;
    }
    if(mau_eq(line, len, "Failed to connect to saved WiFi") ||
       mau_eq(line, len, "Could not connect to any saved WiFi network")) {
        mau_end(MauNoReach);
        return;
    }
    if(mau_eq(line, len, "Missing wigle credentials")) {
        mau_end(MauNoKey);
        return;
    }
    if(mau_eq(line, len, "Direct upload not supported")) {
        mau_end(MauUnsupported);
        return;
    }
    if(mau_starts(line, len, "Trying saved WiFi: ", &prefix)) {
        (void)prefix;
        g_mau.joining = true;
        if(g_mau.tries < 9u) {
            g_mau.tries++;
        }
        mau_bump();
        return;
    }
    if(mau_saved_wifi(line, len, &prefix)) {
        size_t n = len - prefix;

        g_mau.joining = false;
        g_mau.wifi_ok = true;
        g_mau.wifi_joined = g_joined[0] != '\0' && len < (size_t)SR_RAW_LINE_MAX &&
                            strlen(g_joined) == n && memcmp(g_joined, line + prefix, n) == 0;
        mau_bump();
        return;
    }
    if(mau_starts(line, len, "Uploading ", &prefix)) {
        g_mau.saw_uploading = true;
        g_mau.joining = false;
        mau_open_file();
        mau_set_name(line, len, prefix);
        g_mau.face = MauSending;
        mau_bump();
        return;
    }
    if(mau_eq(line, len, "[UPLOAD] WiGLE already uploaded, skipping")) {
        g_mau.saw_skip = true;
        mau_bump();
        return;
    }
    if(mau_eq(line, len, "Upload OK") || mau_eq(line, len, "[UPLOAD] WiGLE upload succeeded")) {
        g_mau.saw_ok = true;
        if(mau_eq(line, len, "[UPLOAD] WiGLE upload succeeded")) {
            g_mau.saw_succeeded = true;
            g_mau.file_200 = true;
        }
        mau_keep_ok();
        mau_bump();
        return;
    }
    if(mau_eq(line, len, "Failed to connected to api.wigle.net")) {
        g_mau.fail_route = true;
        mau_keep_fail();
        mau_bump();
        return;
    }
    if(mau_eq(line, len, "[WIGLE] No response bytes received") ||
       mau_eq(line, len, "Timeout reached")) {
        g_mau.fail_reply = true;
        mau_bump();
        return;
    }
    if(mau_eq(line, len, "[WIGLE] Could not allocate upload buffer")) {
        g_mau.fail_ram = true;
        mau_keep_fail();
        mau_bump();
        return;
    }
    if(mau_starts(line, len, "File does not exist: ", &prefix) ||
       mau_starts(line, len, "Could not open file: ", &prefix)) {
        (void)prefix;
        g_mau.fail_file = true;
        mau_keep_fail();
        mau_bump();
        return;
    }
    if(mau_eq(line, len, "WiGLE failed") || mau_eq(line, len, "[UPLOAD] WiGLE upload failed")) {
        mau_keep_fail();
        mau_bump();
        return;
    }
    if(mau_eq(line, len, "Upload complete")) {
        mau_finish_complete();
    }
}

void sr_mau_up_context(char* line) {
    if(line == NULL) {
        return;
    }
    line[0] = '\0';
    if(g_mau.in_flight || g_mau.face == MauIdle || !g_mau.wifi_ok) {
        return;
    }
    mau_put(line, g_mau.wifi_joined ? "Used joined Wi-Fi" : "Wi-Fi OK");
}

void sr_mau_up_hint(char* line) {
    if(line == NULL) {
        return;
    }
    line[0] = '\0';
    if(g_mau.in_flight || g_mau.face != MauSent || !g_mau.saw_succeeded) {
        return;
    }
    mau_put(line, "Check wigle.net");
}

bool sr_mau_up_empty_list(void) {
    return !g_mau.in_flight && g_mau.face == MauNoLogs;
}

void sr_mau_up_note_joined(const char* ssid) {
    size_t n = 0u;

    memset(g_joined, 0, sizeof(g_joined));
    if(ssid == NULL) {
        return;
    }
    while(n < (size_t)SR_SSID_MAX && ssid[n] != '\0') {
        n++;
    }
    if(ssid[n] != '\0') {
        return;
    }
    memcpy(g_joined, ssid, n);
}

void sr_mau_up_forget_joined(void) {
    memset(g_joined, 0, sizeof(g_joined));
}

unsigned sr_mau_up_key_state(void) {
    unsigned both = (1u << SrJoinKeyWigleName) | (1u << SrJoinKeyWigleToken);

    if((g_key_seen & both) == both) {
        return SrMauKeyImported;
    }
    return (g_key_seen & both) != 0u ? SrMauKeyPartial : SrMauKeyUnknown;
}

void sr_mau_up_key_forget(void) {
    g_key_seen = 0u;
}

bool sr_mau_up_tally(SrMauUpTally* out) {
    if(out == NULL) {
        return false;
    }
    out->ok_n = 0u;
    out->fail_n = 0u;
    out->wigle200 = false;
    out->ready = false;
    if(g_mau.in_flight || g_mau.face == MauIdle || g_mau.face == MauSending) {
        return false;
    }
    out->ok_n = g_mau.n_ok;
    out->fail_n = g_mau.n_fail;
    out->wigle200 = g_mau.wigle200;
    out->ready = true;
    return true;
}

bool sr_mau_up_in_flight(void) {
    return g_mau.in_flight;
}

uint32_t sr_mau_up_rev(void) {
    return g_mau.rev;
}

static const char* mau_named(const char* preferred, const char* fallback) {
    if(preferred != NULL && preferred[0] != '\0') {
        return preferred;
    }
    if(g_mau.name[0] != '\0') {
        return g_mau.name;
    }
    return fallback;
}

void sr_mau_up_lines(unsigned session, char* line1, char* line2) {
    if(!g_mau.in_flight && session == 1u) {
        mau_put(line1, "Stop scan first");
        mau_put(line2, "then Upload");
        return;
    }
    switch(g_mau.face) {
    case MauSending:
        if(g_mau.name[0] != '\0') {
            mau_put(line1, "Sending");
            mau_put(line2, g_mau.name);
        } else if(g_mau.wifi_ok) {
            mau_put(line1, "Wi-Fi connected");
            mau_put(line2, "keep card in");
        } else if(g_mau.joining) {
            char tries[] = "try N, keep card in";

            tries[4] = (char)('0' + (g_mau.tries > 9u ? 9u : g_mau.tries));
            mau_put(line1, "Joining Wi-Fi");
            mau_put(line2, tries);
        } else {
            mau_put(line1, "Sending");
            mau_put(line2, "keep card in");
        }
        break;
    case MauNoWifi:
        mau_put(line1, "No saved Wi-Fi");
        mau_put(line2, "Join WiFi first");
        break;
    case MauNoReach:
        mau_put(line1, "Can't reach Wi-Fi");
        mau_put(line2, "try Join again");
        break;
    case MauNoKey:
        mau_put(line1, "No WiGLE key");
        mau_put(line2, "put key on card");
        break;
    case MauSent:
        mau_put(line1, "Sent to WiGLE");
        mau_put(line2, mau_named(g_mau.ok_name, "check account"));
        break;
    case MauAlready:
        mau_put(line1, "Already sent");
        mau_put(line2, mau_named(g_mau.ok_name, "on WiGLE"));
        break;
    case MauRejected:
        mau_put(line1, "WiGLE rejected");
        mau_put(line2, mau_named(g_mau.fail_name, "try again"));
        break;
    case MauNoRoute:
        mau_put(line1, "Can't reach WiGLE");
        mau_put(line2, "try again later");
        break;
    case MauNoReply:
        mau_put(line1, "WiGLE timed out");
        mau_put(line2, "try again");
        break;
    case MauNoRam:
        mau_put(line1, "Board low on RAM");
        mau_put(line2, "try again");
        break;
    case MauNoFile:
        mau_put(line1, "Log not on card");
        mau_put(line2, mau_named(g_mau.fail_name, "missing"));
        break;
    case MauNoOk:
        mau_put(line1, "No confirmation");
        mau_put(line2, g_mau.name[0] != '\0' ? g_mau.name : "nothing sent");
        break;
    case MauNoLogs:
        mau_put(line1, "No logs in list");
        mau_put(line2, "nothing sent");
        break;
    case MauUnsupported:
        mau_put(line1, "Upload not in build");
        mau_put(line2, "not supported");
        break;
    default:
        mau_put(line1, "Ready");
        mau_put(line2, "press Upload");
        break;
    }
}
