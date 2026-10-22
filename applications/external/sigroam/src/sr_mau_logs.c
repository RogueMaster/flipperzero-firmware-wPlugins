#include "sr_mau_logs.h"

#include "sr_marauder_upload.h"
#include "sr_types.h"

#include <string.h>

/*
 * One app, one query. apply_unknown and the Upload scene both take the app
 * mutex on the paths that call this record, same as the upload receipt.
 *
 * A delivered line of SR_RAW_LINE_MAX bytes is refused. The worker does not
 * pass sr_line's truncated flag, and a line that reached the assembler cap
 * is delivered at that length. An exact-cap line is refused with it. The
 * screen shows the cut. It is not parsed as a file name.
 *
 * The name/size split keeps a trailing decimal only when the character
 * before those digits is a letter. Official wardrive_N.log, .wigle, .gpx,
 * and .wdg names end on a letter. A name that itself ends on a digit cannot
 * be separated from the glued size, so it is not counted.
 */

enum {
    LogsIdle = 0,
    LogsOpen,
    LogsClosed,
    LogsCard,
    LogsDropped
};

typedef struct {
    char name[SR_MAU_LOG_NAME_MAX + 1u];
    uint8_t nlen;
    bool sent;
    uint32_t size;
} MauLogSlot;

typedef struct {
    unsigned phase;
    bool armed;
    bool saw_echo;
    bool barrier_due;
    bool barrier_sent;
    bool cut;
    bool overflow;
    uint32_t matches;
    uint32_t rev;
    unsigned n_slots;
    unsigned n_side;
    MauLogSlot slots[SR_MAU_LOG_SLOTS];
    char side[SR_MAU_LOG_SLOTS][SR_MAU_LOG_NAME_MAX + 1u];
    uint8_t side_len[SR_MAU_LOG_SLOTS];
} MauLogs;

static MauLogs g_logs;

_Static_assert(sizeof(SR_MAU_LOGS_HELP_1) - 1u <= SR_MAU_COLS, "help 1");
_Static_assert(sizeof(SR_MAU_LOGS_HELP_2) - 1u <= SR_MAU_COLS, "help 2");
_Static_assert(sizeof(SR_MAU_LOGS_HELP_3) - 1u <= SR_MAU_COLS, "help 3");
_Static_assert(sizeof(SR_MAU_LOGS_HELP_4) - 1u <= SR_MAU_COLS, "help 4");
_Static_assert(sizeof(SR_MAU_LOGS_SEEN_2) - 1u <= SR_MAU_COLS, "seen 2");
_Static_assert(sizeof(SR_MAU_KEY_HELP_1) - 1u <= SR_MAU_COLS, "key 1");
_Static_assert(sizeof(SR_MAU_KEY_HELP_2) - 1u <= SR_MAU_COLS, "key 2");
_Static_assert(sizeof(SR_MAU_KEY_HELP_3) - 1u <= SR_MAU_COLS, "key 3");
_Static_assert(sizeof(SR_MAU_KEY_HELP_4) - 1u <= SR_MAU_COLS, "key 4");
/* "Card has " + 6 digits + " logs" fits 20 columns. */
_Static_assert(sizeof("Card has ") - 1u + 6u + sizeof(" logs") - 1u <= SR_MAU_COLS, "card line");
_Static_assert(sizeof("Card has many logs") - 1u <= SR_MAU_COLS, "card many");
_Static_assert(sizeof("press Logs") - 1u <= SR_MAU_COLS, "press Logs");
_Static_assert(sizeof("not a listing") - 1u <= SR_MAU_COLS, "not a listing");
_Static_assert(sizeof("Listing") - 1u <= SR_MAU_COLS, "Listing");
_Static_assert(sizeof("not finished") - 1u <= SR_MAU_COLS, "not finished");
_Static_assert(sizeof("list not closed") - 1u <= SR_MAU_COLS, "list not closed");
_Static_assert(sizeof("list finished") - 1u <= SR_MAU_COLS, "list finished");
_Static_assert(sizeof("No name matched") - 1u <= SR_MAU_COLS, "No name matched");
_Static_assert(sizeof("Line cut off") - 1u <= SR_MAU_COLS, "Line cut off");
_Static_assert(sizeof("name unknown") - 1u <= SR_MAU_COLS, "name unknown");
_Static_assert(sizeof("Card not ready") - 1u <= SR_MAU_COLS, "Card not ready");
_Static_assert(sizeof("list unknown") - 1u <= SR_MAU_COLS, "list unknown");
_Static_assert(sizeof("Scan started") - 1u <= SR_MAU_COLS, "Scan started");
_Static_assert(sizeof("list dropped") - 1u <= SR_MAU_COLS, "list dropped");
_Static_assert(sizeof("Saw many names") - 1u <= SR_MAU_COLS, "Saw many names");
_Static_assert(sizeof("Saw many, cut") - 1u <= SR_MAU_COLS, "Saw many, cut");
_Static_assert(sizeof("1 log, 0 sent") - 1u <= SR_MAU_COLS, "1 log sent");
_Static_assert(sizeof("16 logs, 16 sent") - 1u <= SR_MAU_COLS, "16 logs sent");
_Static_assert(sizeof("Upload on PC") - 1u <= SR_MAU_COLS, "Upload on PC");
_Static_assert(sizeof("Not sent yet:") - 1u <= SR_MAU_COLS, "Not sent yet");
_Static_assert(sizeof("wigle.net/uploads") - 1u <= SR_MAU_COLS, "wigle url");
_Static_assert(sizeof("Stop scan first") - 1u <= SR_MAU_COLS, "Stop scan first");
_Static_assert(sizeof("Logs not sent") - 1u <= SR_MAU_COLS, "Logs not sent");
_Static_assert(sizeof("Upload is active") - 1u <= SR_MAU_COLS, "Upload is active");
_Static_assert(sizeof("Join is active") - 1u <= SR_MAU_COLS, "Join is active");
_Static_assert(sizeof("Command busy") - 1u <= SR_MAU_COLS, "Command busy");
/* "Saw " + 6 digits + " log names" is the longest count that fits 20 columns. */
_Static_assert(
    sizeof("Saw ") - 1u + 6u + sizeof(" log names") - 1u == SR_MAU_COLS,
    "count columns");

static void logs_bump(void) {
    g_logs.rev++;
}

static void logs_put(char* dst, const char* src) {
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

static bool logs_eq(const char* line, size_t len, const char* lit) {
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

static bool logs_starts(const char* line, size_t len, const char* lit) {
    size_t n = 0u;

    while(lit[n] != '\0') {
        n++;
    }
    if(len < n) {
        return false;
    }
    for(n = 0u; n < len && lit[n] != '\0'; n++) {
        if(line[n] != lit[n]) {
            return false;
        }
    }
    return true;
}

static bool logs_ends(const char* line, size_t len, const char* lit) {
    size_t n = 0u;
    size_t i;

    while(lit[n] != '\0') {
        n++;
    }
    if(len < n) {
        return false;
    }
    for(i = 0u; i < n; i++) {
        if(line[len - n + i] != lit[i]) {
            return false;
        }
    }
    return true;
}

static bool logs_letter(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

/* Name length before the glued size. False when the boundary is not proven. */
static bool logs_name_len(const char* line, size_t len, size_t* out) {
    size_t i;

    if(line == NULL || out == NULL || len == 0u) {
        return false;
    }
    i = len;
    while(i > 0u && line[i - 1u] >= '0' && line[i - 1u] <= '9') {
        i--;
    }
    if(i == 0u || i == len || !logs_letter(line[i - 1u])) {
        return false;
    }
    *out = i;
    return true;
}

static bool logs_is_log_name(const char* line, size_t name_len) {
    if(!(logs_starts(line, name_len, "wardrive_") || logs_starts(line, name_len, "wigle-"))) {
        return false;
    }
    if(logs_ends(line, name_len, ".wigle") || logs_ends(line, name_len, ".wdg") ||
       logs_ends(line, name_len, ".gpx")) {
        return false;
    }
    return true;
}

static bool logs_is_sidecar(const char* line, size_t name_len) {
    if(!(logs_starts(line, name_len, "wardrive_") || logs_starts(line, name_len, "wigle-"))) {
        return false;
    }
    return logs_ends(line, name_len, ".wigle");
}

static uint32_t logs_size(const char* line, size_t name_len, size_t len) {
    uint32_t n = 0u;
    size_t i;

    for(i = name_len; i < len; i++) {
        unsigned d = (unsigned)(line[i] - '0');

        if(n > (UINT32_MAX - d) / 10u) {
            return UINT32_MAX;
        }
        n = n * 10u + d;
    }
    return n;
}

static void logs_mark_sent(const char* name, size_t n) {
    unsigned i;

    for(i = 0u; i < g_logs.n_slots; i++) {
        if(g_logs.slots[i].nlen == n && memcmp(g_logs.slots[i].name, name, n) == 0) {
            g_logs.slots[i].sent = true;
            return;
        }
    }
}

static bool logs_saw_side(const char* name, size_t n) {
    unsigned i;

    for(i = 0u; i < g_logs.n_side; i++) {
        if(g_logs.side_len[i] == n && memcmp(g_logs.side[i], name, n) == 0) {
            return true;
        }
    }
    return false;
}

static void logs_note_side(const char* name, size_t n) {
    unsigned i;

    if(n == 0u || n > (size_t)SR_MAU_LOG_NAME_MAX) {
        g_logs.overflow = true;
        return;
    }
    if(logs_saw_side(name, n)) {
        logs_mark_sent(name, n);
        return;
    }
    if(g_logs.n_side >= (unsigned)SR_MAU_LOG_SLOTS) {
        g_logs.overflow = true;
        return;
    }
    i = g_logs.n_side++;
    memcpy(g_logs.side[i], name, n);
    g_logs.side[i][n] = '\0';
    g_logs.side_len[i] = (uint8_t)n;
    logs_mark_sent(name, n);
}

static void logs_note_log(const char* name, size_t n, uint32_t size) {
    unsigned i;

    if(n == 0u || n > (size_t)SR_MAU_LOG_NAME_MAX ||
       g_logs.n_slots >= (unsigned)SR_MAU_LOG_SLOTS) {
        g_logs.overflow = true;
        return;
    }
    i = g_logs.n_slots++;
    memcpy(g_logs.slots[i].name, name, n);
    g_logs.slots[i].name[n] = '\0';
    g_logs.slots[i].nlen = (uint8_t)n;
    g_logs.slots[i].size = size;
    g_logs.slots[i].sent = logs_saw_side(name, n);
}

static void logs_clear(uint32_t rev) {
    g_logs.phase = LogsIdle;
    g_logs.armed = false;
    g_logs.saw_echo = false;
    g_logs.barrier_due = false;
    g_logs.barrier_sent = false;
    g_logs.cut = false;
    g_logs.overflow = false;
    g_logs.matches = 0u;
    g_logs.rev = rev;
    g_logs.n_slots = 0u;
    g_logs.n_side = 0u;
    memset(g_logs.slots, 0, sizeof(g_logs.slots));
    memset(g_logs.side, 0, sizeof(g_logs.side));
    memset(g_logs.side_len, 0, sizeof(g_logs.side_len));
}

static unsigned logs_digits(uint32_t n) {
    unsigned d = 1u;

    while(n >= 10u) {
        n /= 10u;
        d++;
    }
    return d;
}

static void logs_put_u(char* dst, size_t* used, uint32_t n) {
    char rev[10];
    unsigned d;
    unsigned i;
    uint32_t x;

    d = logs_digits(n);
    x = n;
    for(i = 0u; i < d; i++) {
        rev[d - 1u - i] = (char)('0' + (x % 10u));
        x /= 10u;
    }
    for(i = 0u; i < d; i++) {
        dst[(*used)++] = rev[i];
    }
}

static void logs_put_saw(char* dst, uint32_t n, bool cut) {
    char tmp[SR_MAU_COLS + 1u];
    size_t used = 0u;
    const char* tail;
    size_t tail_n;
    unsigned digs;

    if(n > 999999u) {
        logs_put(dst, cut ? "Saw many, cut" : "Saw many names");
        return;
    }
    tail = cut ? ", cut" : (n == 1u ? " log name" : " log names");
    tail_n = 0u;
    while(tail[tail_n] != '\0') {
        tail_n++;
    }
    digs = logs_digits(n);
    if(4u + digs + tail_n > (size_t)SR_MAU_COLS) {
        logs_put(dst, cut ? "Saw many, cut" : "Saw many names");
        return;
    }
    tmp[used++] = 'S';
    tmp[used++] = 'a';
    tmp[used++] = 'w';
    tmp[used++] = ' ';
    logs_put_u(tmp, &used, n);
    for(tail_n = 0u; tail[tail_n] != '\0'; tail_n++) {
        tmp[used++] = tail[tail_n];
    }
    tmp[used] = '\0';
    logs_put(dst, tmp);
}

const char* sr_mau_logs_cmd(void) {
    return "ls /\n";
}

const char* sr_mau_logs_barrier_cmd(void) {
    return "channel\n";
}

bool sr_mau_logs_barrier_due(void) {
    return g_logs.armed && g_logs.barrier_due && !g_logs.barrier_sent;
}

void sr_mau_logs_barrier_sent(void) {
    if(!sr_mau_logs_barrier_due()) {
        return;
    }
    g_logs.barrier_sent = true;
    logs_bump();
}

bool sr_mau_logs_barrier_send(bool generic, bool slot_taken) {
    return generic && !slot_taken && sr_mau_logs_barrier_due();
}

bool sr_mau_logs_allow(unsigned session, bool upload_flight, bool join_busy) {
    if(upload_flight || join_busy) {
        return false;
    }
    return session == 0u || session == 2u;
}

void sr_mau_logs_reset(void) {
    logs_clear(0u);
}

void sr_mau_logs_arm(void) {
    uint32_t rev = g_logs.rev;

    logs_clear(rev);
    g_logs.phase = LogsOpen;
    g_logs.armed = true;
    logs_bump();
}

void sr_mau_logs_invalidate(void) {
    uint32_t rev;

    if(g_logs.phase == LogsIdle && !g_logs.armed) {
        return;
    }
    rev = g_logs.rev;
    logs_clear(rev);
    g_logs.phase = LogsDropped;
    logs_bump();
}

void sr_mau_logs_feed(const char* line, size_t len) {
    size_t name_len = 0u;

    if(!g_logs.armed || line == NULL) {
        return;
    }
    if(len == (size_t)SR_RAW_LINE_MAX) {
        g_logs.cut = true;
        logs_bump();
        return;
    }
    if(!g_logs.saw_echo) {
        if(logs_eq(line, len, "#ls /") || logs_eq(line, len, "> #ls /")) {
            g_logs.saw_echo = true;
            g_logs.barrier_due = true;
            g_logs.phase = LogsOpen;
            logs_bump();
        }
        return;
    }
    if(g_logs.barrier_sent &&
       (logs_eq(line, len, "> #channel") || logs_eq(line, len, "#channel"))) {
        g_logs.phase = LogsClosed;
        g_logs.armed = false;
        logs_bump();
        return;
    }
    if(logs_eq(line, len, ">") || logs_eq(line, len, "> ")) {
        g_logs.phase = LogsClosed;
        g_logs.armed = false;
        logs_bump();
        return;
    }
    if(logs_eq(line, len, "Failed to mount SD Card") ||
       logs_eq(line, len, "SD Card NOT Supported")) {
        g_logs.phase = LogsCard;
        g_logs.armed = false;
        logs_bump();
        return;
    }
    if(!logs_name_len(line, len, &name_len)) {
        return;
    }
    if(logs_is_sidecar(line, name_len)) {
        if(name_len <= 6u) {
            return;
        }
        logs_note_side(line, name_len - 6u);
        logs_bump();
        return;
    }
    if(!logs_is_log_name(line, name_len)) {
        return;
    }
    if(g_logs.matches < UINT32_MAX) {
        g_logs.matches++;
    }
    logs_note_log(line, name_len, logs_size(line, name_len, len));
    logs_bump();
}

bool sr_mau_logs_armed(void) {
    return g_logs.armed;
}

uint32_t sr_mau_logs_rev(void) {
    return g_logs.rev;
}

uint32_t sr_mau_logs_matches(void) {
    return g_logs.matches;
}

bool sr_mau_logs_closed(void) {
    return g_logs.phase == LogsClosed;
}

bool sr_mau_logs_cut(void) {
    return g_logs.cut;
}

bool sr_mau_logs_card(void) {
    return g_logs.phase == LogsCard;
}

bool sr_mau_logs_dropped(void) {
    return g_logs.phase == LogsDropped;
}

bool sr_mau_logs_card_line(char* line) {
    char tmp[SR_MAU_COLS + 1u];
    size_t used = 0u;
    const char* tail;
    size_t i;

    if(line == NULL) {
        return false;
    }
    line[0] = '\0';
    if(g_logs.phase != LogsClosed || g_logs.matches == 0u) {
        return false;
    }
    if(g_logs.matches > 999999u) {
        logs_put(line, "Card has many logs");
        return true;
    }
    for(i = 0u; i < 9u; i++) {
        tmp[used++] = "Card has "[i];
    }
    logs_put_u(tmp, &used, g_logs.matches);
    tail = g_logs.matches == 1u ? " log" : " logs";
    for(i = 0u; tail[i] != '\0'; i++) {
        tmp[used++] = tail[i];
    }
    tmp[used] = '\0';
    logs_put(line, tmp);
    return true;
}

static void logs_fmt_size(uint32_t bytes, char* dst, size_t* used) {
    char unit;
    uint32_t n;

    if(bytes >= 1048576u) {
        n = bytes / 1048576u;
        unit = 'M';
    } else if(bytes >= 1024u) {
        n = bytes / 1024u;
        unit = 'K';
    } else {
        n = bytes;
        unit = 'B';
    }
    logs_put_u(dst, used, n);
    dst[(*used)++] = unit;
}

bool sr_mau_logs_sent_line(char* line) {
    char tmp[SR_MAU_COLS + 1u];
    size_t used = 0u;
    const char* mid;
    const char* tail;
    uint32_t sent = 0u;
    unsigned i;
    unsigned n_d;
    unsigned k_d;

    if(line == NULL) {
        return false;
    }
    line[0] = '\0';
    if(g_logs.phase != LogsClosed || g_logs.matches == 0u) {
        return false;
    }
    if(g_logs.overflow) {
        return sr_mau_logs_card_line(line);
    }
    for(i = 0u; i < g_logs.n_slots; i++) {
        if(g_logs.slots[i].sent) {
            sent++;
        }
    }
    mid = g_logs.matches == 1u ? " log, " : " logs, ";
    tail = " sent";
    n_d = logs_digits(g_logs.matches);
    k_d = logs_digits(sent);
    if(n_d + strlen(mid) + k_d + strlen(tail) > (size_t)SR_MAU_COLS) {
        return sr_mau_logs_card_line(line);
    }
    logs_put_u(tmp, &used, g_logs.matches);
    for(i = 0u; mid[i] != '\0'; i++) {
        tmp[used++] = mid[i];
    }
    logs_put_u(tmp, &used, sent);
    for(i = 0u; tail[i] != '\0'; i++) {
        tmp[used++] = tail[i];
    }
    tmp[used] = '\0';
    logs_put(line, tmp);
    return true;
}

uint32_t sr_mau_logs_unsent(void) {
    uint32_t n = 0u;
    unsigned i;

    if(g_logs.phase != LogsClosed) {
        return SR_MAU_LOGS_UNKNOWN;
    }
    if(g_logs.overflow) {
        return SR_MAU_LOGS_UNKNOWN;
    }
    if(g_logs.matches == 0u) {
        return 0u;
    }
    for(i = 0u; i < g_logs.n_slots; i++) {
        if(!g_logs.slots[i].sent) {
            n++;
        }
    }
    return n;
}

bool sr_mau_logs_unsent_entry(unsigned want, char* line) {
    char tmp[SR_MAU_COLS + 1u];
    char sz[12];
    size_t sz_n = 0u;
    size_t name_n;
    size_t i;
    unsigned seen = 0u;
    unsigned s;
    const MauLogSlot* slot = NULL;

    if(line != NULL) {
        line[0] = '\0';
    }
    if(line == NULL || sr_mau_logs_unsent() == SR_MAU_LOGS_UNKNOWN) {
        return false;
    }
    for(s = 0u; s < g_logs.n_slots; s++) {
        if(g_logs.slots[s].sent) {
            continue;
        }
        if(seen == want) {
            slot = &g_logs.slots[s];
            break;
        }
        seen++;
    }
    if(slot == NULL) {
        return false;
    }
    logs_fmt_size(slot->size, sz, &sz_n);
    name_n = (size_t)slot->nlen;
    if(name_n + 1u + sz_n > (size_t)SR_MAU_COLS) {
        if(sz_n + 2u > (size_t)SR_MAU_COLS) {
            return false;
        }
        name_n = (size_t)SR_MAU_COLS - 1u - sz_n;
        if(name_n == 0u) {
            return false;
        }
        memcpy(tmp, slot->name, name_n - 1u);
        tmp[name_n - 1u] = '~';
    } else {
        memcpy(tmp, slot->name, name_n);
    }
    tmp[name_n] = ' ';
    for(i = 0u; i < sz_n; i++) {
        tmp[name_n + 1u + i] = sz[i];
    }
    tmp[name_n + 1u + sz_n] = '\0';
    logs_put(line, tmp);
    return true;
}

void sr_mau_logs_lines(char* line1, char* line2) {
    if(line1 == NULL || line2 == NULL) {
        return;
    }
    switch(g_logs.phase) {
    case LogsDropped:
        logs_put(line1, "Scan started");
        logs_put(line2, "list dropped");
        return;
    case LogsCard:
        logs_put(line1, "Card not ready");
        logs_put(line2, "list unknown");
        return;
    case LogsOpen:
    case LogsClosed:
        if(g_logs.cut && g_logs.matches == 0u) {
            logs_put(line1, "Line cut off");
            logs_put(line2, g_logs.phase == LogsClosed ? "name unknown" : "list not closed");
            return;
        }
        if(g_logs.matches > 0u) {
            logs_put_saw(line1, g_logs.matches, g_logs.cut);
            logs_put(line2, g_logs.phase == LogsClosed ? "list finished" : "list not closed");
            return;
        }
        if(g_logs.phase == LogsClosed) {
            logs_put(line1, "No name matched");
            logs_put(line2, "list finished");
            return;
        }
        logs_put(line1, "Listing");
        logs_put(line2, "not finished");
        return;
    case LogsIdle:
    default:
        logs_put(line1, "press Logs");
        logs_put(line2, "not a listing");
        return;
    }
}

bool sr_mau_logs_slot(unsigned i, char* name, size_t name_cap, uint32_t* size, bool* sent) {
    size_t n;

    if(name != NULL && name_cap > 0u) {
        name[0] = '\0';
    }
    if(name == NULL || name_cap == 0u || size == NULL || sent == NULL) {
        return false;
    }
    if(g_logs.phase != LogsClosed || i >= g_logs.n_slots) {
        return false;
    }
    n = (size_t)g_logs.slots[i].nlen;
    if(n + 1u > name_cap) {
        return false;
    }
    memcpy(name, g_logs.slots[i].name, n);
    name[n] = '\0';
    *size = g_logs.slots[i].size;
    *sent = g_logs.slots[i].sent;
    return true;
}

uint32_t sr_mau_logs_extra(void) {
    if(g_logs.phase != LogsClosed) {
        return SR_MAU_LOGS_UNKNOWN;
    }
    if(g_logs.matches < g_logs.n_slots) {
        return 0u;
    }
    return g_logs.matches - (uint32_t)g_logs.n_slots;
}

void sr_mau_logs_block_lines(unsigned why, char* line1, char* line2) {
    if(line1 == NULL || line2 == NULL) {
        return;
    }
    switch(why) {
    case SrMauLogsBlockScan:
        logs_put(line1, "Stop scan first");
        logs_put(line2, "Logs not sent");
        return;
    case SrMauLogsBlockUpload:
        logs_put(line1, "Upload is active");
        logs_put(line2, "Logs not sent");
        return;
    case SrMauLogsBlockJoin:
        logs_put(line1, "Join is active");
        logs_put(line2, "Logs not sent");
        return;
    case SrMauLogsBlockBusy:
        logs_put(line1, "Command busy");
        logs_put(line2, "Logs not sent");
        return;
    default:
        logs_put(line1, "press Logs");
        logs_put(line2, "not a listing");
        return;
    }
}
