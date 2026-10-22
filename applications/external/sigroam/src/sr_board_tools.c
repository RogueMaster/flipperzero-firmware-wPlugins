#include "sr_board_tools.h"

#include <stdio.h>
#include <string.h>

enum {
    BtOff = 0,
    BtList,
    BtReply
};

typedef struct {
    uint8_t listen;
    bool saw_echo;
    bool barrier_due;
    bool barrier_sent;
    bool closed;
    bool busy;
    unsigned count;
    unsigned ord[SR_BT_LIST_MAX];
    char ssid[SR_BT_LIST_MAX][SR_BT_SSID_MAX + 1u];
    uint32_t reply_seq;
    unsigned reply_kind;
    uint32_t reply_n;
    char reply_text[40];
    uint32_t split_seq;
    uint32_t split_kb;
    bool split_ok;
} BtState;

static BtState g_bt;

static const char* const k_split_label[] = {
    "Off",
    "256 KB",
    "512 KB",
    "1024 KB (default)",
    "2048 KB",
    "4096 KB",
};

static const uint32_t k_split_kb[] = {0u, 256u, 512u, 1024u, 2048u, 4096u};

static bool bt_eq(const char* line, size_t len, const char* lit) {
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

static bool bt_starts(const char* line, size_t len, const char* lit, size_t* out) {
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
    if(out != NULL) {
        *out = n;
    }
    return true;
}

static bool bt_digits(const char* line, size_t len, size_t* i, uint32_t* out) {
    size_t p;
    uint32_t n = 0u;
    bool any = false;

    if(line == NULL || i == NULL || out == NULL) {
        return false;
    }
    p = *i;
    while(p < len && line[p] >= '0' && line[p] <= '9') {
        unsigned d = (unsigned)(line[p] - '0');

        if(n > (UINT32_MAX - d) / 10u) {
            return false;
        }
        n = n * 10u + d;
        any = true;
        p++;
    }
    if(!any) {
        return false;
    }
    *i = p;
    *out = n;
    return true;
}

static bool bt_tail(const char* line, size_t len, size_t i, const char* lit) {
    size_t n = 0u;
    size_t k;

    while(lit[n] != '\0') {
        n++;
    }
    if(i + n != len) {
        return false;
    }
    for(k = 0u; k < n; k++) {
        if(line[i + k] != lit[k]) {
            return false;
        }
    }
    return true;
}

static void bt_copy(char* dst, size_t cap, const char* src, size_t n) {
    size_t i;

    if(dst == NULL || cap == 0u) {
        return;
    }
    if(src == NULL) {
        dst[0] = '\0';
        return;
    }
    if(n >= cap) {
        n = cap - 1u;
    }
    for(i = 0u; i < n; i++) {
        dst[i] = src[i];
    }
    dst[n] = '\0';
}

static void bt_note(unsigned kind, uint32_t n, const char* text, size_t text_n) {
    g_bt.reply_kind = kind;
    g_bt.reply_n = n;
    bt_copy(g_bt.reply_text, sizeof(g_bt.reply_text), text, text_n);
    g_bt.reply_seq++;
}

static void bt_clear_list(void) {
    unsigned i;

    g_bt.saw_echo = false;
    g_bt.barrier_due = false;
    g_bt.barrier_sent = false;
    g_bt.closed = false;
    g_bt.busy = false;
    g_bt.count = 0u;
    for(i = 0u; i < (unsigned)SR_BT_LIST_MAX; i++) {
        g_bt.ord[i] = 0u;
        g_bt.ssid[i][0] = '\0';
    }
}

SrBtGate sr_bt_gate(unsigned session, bool upload_flight, bool join_busy) {
    if(session == 1u) {
        return SrBtGateScan;
    }
    if(upload_flight) {
        return SrBtGateUpload;
    }
    if(join_busy) {
        return SrBtGateJoin;
    }
    return SrBtGateOk;
}

SrBtPeer sr_bt_peer(bool version_nonempty, bool lite) {
    if(!version_nonempty) {
        return SrBtPeerEmpty;
    }
    if(lite) {
        return SrBtPeerLite;
    }
    return SrBtPeerOther;
}

SrBtQuote sr_bt_quote_add_cmd(const char* ssid, size_t n, char* dst, size_t cap) {
    static const char k_pre[] = "sigroam ignore add ";
    bool dq = false;
    bool sq = false;
    char q;
    size_t i;
    size_t need;
    size_t pre = sizeof(k_pre) - 1u;

    if(dst != NULL && cap > 0u) {
        dst[0] = '\0';
    }
    if(ssid == NULL || dst == NULL || n == 0u || n > (size_t)SR_BT_SSID_MAX) {
        return SrBtQuoteReject;
    }
    for(i = 0u; i < n; i++) {
        if(ssid[i] == '"') {
            dq = true;
        } else if(ssid[i] == '\'') {
            sq = true;
        }
    }
    if(dq && sq) {
        return SrBtQuoteReject;
    }
    q = dq ? '\'' : '"';
    need = pre + 1u + n + 1u + 1u;
    if(cap < need + 1u) {
        return SrBtQuoteReject;
    }
    memcpy(dst, k_pre, pre);
    dst[pre] = q;
    memcpy(dst + pre + 1u, ssid, n);
    dst[pre + 1u + n] = q;
    dst[pre + 1u + n + 1u] = '\n';
    dst[need] = '\0';
    return SrBtQuoteOk;
}

bool sr_bt_ignore_rm_cmd(unsigned ord, char* dst, size_t cap) {
    int n;

    if(dst == NULL || cap == 0u) {
        return false;
    }
    dst[0] = '\0';
    if(ord == 0u || ord > 999u) {
        return false;
    }
    n = snprintf(dst, cap, "sigroam ignore rm %u\n", ord);
    if(n < 0 || (size_t)n >= cap) {
        dst[0] = '\0';
        return false;
    }
    return true;
}

bool sr_bt_split_cmd(uint32_t kb, char* dst, size_t cap) {
    int n;

    if(dst == NULL || cap == 0u) {
        return false;
    }
    n = snprintf(dst, cap, "sigroam split %lu\n", (unsigned long)kb);
    if(n < 0 || (size_t)n >= cap) {
        dst[0] = '\0';
        return false;
    }
    return true;
}

bool sr_bt_rm_cmd(const char* name, char* dst, size_t cap) {
    size_t n = 0u;
    size_t i;
    int wr;

    if(dst != NULL && cap > 0u) {
        dst[0] = '\0';
    }
    if(name == NULL || dst == NULL) {
        return false;
    }
    while(name[n] != '\0') {
        char c = name[n];
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                  c == '.' || c == '_' || c == '-';

        if(!ok || n >= 31u) {
            return false;
        }
        n++;
    }
    if(n == 0u) {
        return false;
    }
    wr = snprintf(dst, cap, "sigroam rm ");
    if(wr < 0 || (size_t)wr >= cap || (size_t)wr + n + 2u > cap) {
        dst[0] = '\0';
        return false;
    }
    for(i = 0u; i < n; i++) {
        dst[(size_t)wr + i] = name[i];
    }
    dst[(size_t)wr + n] = '\n';
    dst[(size_t)wr + n + 1u] = '\0';
    return true;
}

bool sr_bt_offer_home(uint8_t home, const char* ssid, bool listed) {
    if(home != 1u || listed || ssid == NULL || ssid[0] == '\0') {
        return false;
    }
    if(ssid[0] == '-' && ssid[1] == '\0') {
        return false;
    }
    return true;
}

bool sr_bt_list_full(unsigned n) {
    return n >= (unsigned)SR_BT_LIST_MAX;
}

bool sr_bt_del_all_detail(uint32_t n_files, uint32_t n_unsent, char* dst, size_t cap) {
    int n;

    if(dst != NULL && cap > 0u) {
        dst[0] = '\0';
    }
    if(n_unsent == UINT32_MAX || dst == NULL || cap == 0u) {
        return false;
    }
    n = snprintf(
        dst, cap, "%lu files, %lu not uploaded", (unsigned long)n_files, (unsigned long)n_unsent);
    if(n < 0 || (size_t)n >= cap) {
        dst[0] = '\0';
        return false;
    }
    return true;
}

void sr_bt_del_reset(SrBtDel* d) {
    if(d == NULL) {
        return;
    }
    d->phase = (uint8_t)SrBtDelIdle;
    d->opened_ms = 0u;
}

void sr_bt_del_open_one(SrBtDel* d) {
    if(d == NULL) {
        return;
    }
    d->phase = (uint8_t)SrBtDelOne;
    d->opened_ms = 0u;
}

void sr_bt_del_open_all(SrBtDel* d, uint32_t now_ms) {
    if(d == NULL) {
        return;
    }
    d->phase = (uint8_t)SrBtDelAll1;
    d->opened_ms = now_ms;
}

SrBtDelAct sr_bt_del_on_key(SrBtDel* d, unsigned key, uint32_t now_ms, bool blocked) {
    if(d == NULL || d->phase == (uint8_t)SrBtDelIdle) {
        return SrBtDelActNone;
    }
    if(key == (unsigned)SrBtKeyBack || key == (unsigned)SrBtKeyLeft) {
        sr_bt_del_reset(d);
        return SrBtDelActCancel;
    }
    if(blocked && (key == (unsigned)SrBtKeyCenter || key == (unsigned)SrBtKeyRight)) {
        return SrBtDelActNone;
    }
    if(d->phase == (uint8_t)SrBtDelOne) {
        if(key == (unsigned)SrBtKeyCenter) {
            sr_bt_del_reset(d);
            return SrBtDelActSendOne;
        }
        return SrBtDelActNone;
    }
    if(d->phase == (uint8_t)SrBtDelAll1) {
        if(key == (unsigned)SrBtKeyCenter) {
            d->phase = (uint8_t)SrBtDelAll2;
            d->opened_ms = now_ms;
            return SrBtDelActNext;
        }
        return SrBtDelActNone;
    }
    if(d->phase == (uint8_t)SrBtDelAll2) {
        if(key == (unsigned)SrBtKeyRight) {
            if((uint32_t)(now_ms - d->opened_ms) < (uint32_t)SR_BT_DEL_ARM_MS) {
                return SrBtDelActNone;
            }
            sr_bt_del_reset(d);
            return SrBtDelActSendAll;
        }
        return SrBtDelActNone;
    }
    return SrBtDelActNone;
}

const char* sr_bt_ignore_cmd(void) {
    return "sigroam ignore list\n";
}

const char* sr_bt_barrier_cmd(void) {
    return "channel\n";
}

const char* sr_bt_split_query_cmd(void) {
    return "sigroam split\n";
}

const char* sr_bt_clear_cmd(void) {
    return "sigroam ignore clear\n";
}

const char* sr_bt_rm_all_cmd(void) {
    return "sigroam rm all\n";
}

void sr_bt_reset(void) {
    memset(&g_bt, 0, sizeof(g_bt));
}

void sr_bt_arm_list(void) {
    uint32_t reply_seq = g_bt.reply_seq;
    uint32_t split_seq = g_bt.split_seq;
    uint32_t split_kb = g_bt.split_kb;
    bool split_ok = g_bt.split_ok;
    unsigned kind = g_bt.reply_kind;
    uint32_t reply_n = g_bt.reply_n;
    char text[40];

    memcpy(text, g_bt.reply_text, sizeof(text));
    bt_clear_list();
    g_bt.listen = (uint8_t)BtList;
    g_bt.reply_seq = reply_seq;
    g_bt.split_seq = split_seq;
    g_bt.split_kb = split_kb;
    g_bt.split_ok = split_ok;
    g_bt.reply_kind = kind;
    g_bt.reply_n = reply_n;
    memcpy(g_bt.reply_text, text, sizeof(text));
}

void sr_bt_arm_reply(void) {
    g_bt.listen = (uint8_t)BtReply;
    g_bt.saw_echo = false;
    g_bt.barrier_due = false;
    g_bt.barrier_sent = false;
    g_bt.closed = false;
}

bool sr_bt_ignore_barrier_due(void) {
    return g_bt.listen == (uint8_t)BtList && g_bt.barrier_due && !g_bt.barrier_sent &&
           !g_bt.closed && !g_bt.busy;
}

void sr_bt_ignore_barrier_sent(void) {
    if(!sr_bt_ignore_barrier_due()) {
        return;
    }
    g_bt.barrier_sent = true;
}

static void bt_classify(const char* line, size_t len) {
    char name[40];
    unsigned ord = 0u;
    uint32_t n = 0u;
    uint32_t kb = 0u;
    size_t pre = 0u;

    if(sr_bt_line_busy(line, len)) {
        bt_note((unsigned)SrBtReplyBusy, 0u, NULL, 0u);
        return;
    }
    if(sr_bt_line_sd_missing(line, len)) {
        bt_note((unsigned)SrBtReplySd, 0u, NULL, 0u);
        return;
    }
    if(sr_bt_line_split(line, len, &kb)) {
        g_bt.split_kb = kb;
        g_bt.split_ok = true;
        g_bt.split_seq++;
        return;
    }
    if(sr_bt_line_split_invalid(line, len)) {
        bt_note((unsigned)SrBtReplySplitBad, 0u, NULL, 0u);
        return;
    }
    if(sr_bt_line_removed_n(line, len, &n)) {
        bt_note((unsigned)SrBtReplyFiles, n, NULL, 0u);
        return;
    }
    if(sr_bt_line_removed_file(line, len, name, sizeof(name))) {
        bt_note((unsigned)SrBtReplyFile, 0u, name, strlen(name));
        return;
    }
    if(sr_bt_line_remove_fail(line, len)) {
        bt_note((unsigned)SrBtReplyRemoveFail, 0u, NULL, 0u);
        return;
    }
    if(sr_bt_line_ignore_added(line, len)) {
        bt_starts(line, len, "Ignore added: ", &pre);
        bt_note((unsigned)SrBtReplyAdded, 0u, line + pre, len - pre);
        return;
    }
    if(sr_bt_line_ignore_add_fail(line, len)) {
        bt_note((unsigned)SrBtReplyAddFail, 0u, NULL, 0u);
        return;
    }
    if(sr_bt_line_ignore_removed(line, len, &ord)) {
        bt_note((unsigned)SrBtReplyRemoved, ord, NULL, 0u);
        return;
    }
    if(sr_bt_line_ignore_bad_index(line, len)) {
        bt_note((unsigned)SrBtReplyBadIndex, 0u, NULL, 0u);
        return;
    }
    if(sr_bt_line_ignore_cleared(line, len)) {
        bt_note((unsigned)SrBtReplyCleared, 0u, NULL, 0u);
    }
}

void sr_bt_feed(const char* line, size_t len) {
    unsigned ord = 0u;
    char ssid[SR_BT_SSID_MAX + 1u];

    if(g_bt.listen == (uint8_t)BtOff || line == NULL) {
        return;
    }
    if(g_bt.listen == (uint8_t)BtList && !g_bt.closed) {
        if(!g_bt.saw_echo) {
            if(bt_eq(line, len, "#sigroam ignore list") ||
               bt_eq(line, len, "> #sigroam ignore list")) {
                g_bt.saw_echo = true;
                g_bt.barrier_due = true;
            }
            return;
        }
        if(sr_bt_line_busy(line, len)) {
            g_bt.busy = true;
            g_bt.closed = true;
            bt_note((unsigned)SrBtReplyBusy, 0u, NULL, 0u);
            return;
        }
        if(g_bt.barrier_sent && (bt_eq(line, len, "#channel") || bt_eq(line, len, "> #channel"))) {
            g_bt.closed = true;
            return;
        }
        if(sr_bt_line_ignore_empty(line, len)) {
            return;
        }
        if(sr_bt_line_ignore_item(line, len, &ord, ssid, sizeof(ssid)) &&
           g_bt.count < (unsigned)SR_BT_LIST_MAX) {
            unsigned i = g_bt.count++;

            g_bt.ord[i] = ord;
            memcpy(g_bt.ssid[i], ssid, strlen(ssid) + 1u);
            return;
        }
        return;
    }
    if(g_bt.listen == (uint8_t)BtReply) {
        bt_classify(line, len);
    }
}

bool sr_bt_ignore_closed(void) {
    return g_bt.closed;
}

bool sr_bt_ignore_busy(void) {
    return g_bt.busy;
}

unsigned sr_bt_ignore_count(void) {
    return g_bt.count;
}

bool sr_bt_ignore_item(unsigned i, unsigned* ord, char* ssid, size_t cap) {
    size_t n;

    if(ssid != NULL && cap > 0u) {
        ssid[0] = '\0';
    }
    if(i >= g_bt.count || ord == NULL || ssid == NULL || cap == 0u) {
        return false;
    }
    n = strlen(g_bt.ssid[i]);
    if(n + 1u > cap) {
        return false;
    }
    memcpy(ssid, g_bt.ssid[i], n + 1u);
    *ord = g_bt.ord[i];
    return true;
}

uint32_t sr_bt_reply_seq(void) {
    return g_bt.reply_seq;
}

unsigned sr_bt_reply_kind(void) {
    return g_bt.reply_kind;
}

uint32_t sr_bt_reply_n(void) {
    return g_bt.reply_n;
}

bool sr_bt_reply_text(char* dst, size_t cap) {
    size_t n;

    if(dst != NULL && cap > 0u) {
        dst[0] = '\0';
    }
    if(dst == NULL || cap == 0u || g_bt.reply_text[0] == '\0') {
        return false;
    }
    n = strlen(g_bt.reply_text);
    if(n + 1u > cap) {
        return false;
    }
    memcpy(dst, g_bt.reply_text, n + 1u);
    return true;
}

uint32_t sr_bt_split_seq(void) {
    return g_bt.split_seq;
}

bool sr_bt_split_value(uint32_t* kb) {
    if(kb == NULL || !g_bt.split_ok) {
        return false;
    }
    *kb = g_bt.split_kb;
    return true;
}

bool sr_bt_line_busy(const char* line, size_t len) {
    if(line == NULL) {
        return false;
    }
    return bt_eq(line, len, "Busy: stopscan first");
}

bool sr_bt_line_ignore_empty(const char* line, size_t len) {
    if(line == NULL) {
        return false;
    }
    return bt_eq(line, len, "Ignore list empty");
}

bool sr_bt_line_ignore_item(const char* line, size_t len, unsigned* ord, char* ssid, size_t cap) {
    size_t i = 0u;
    uint32_t n = 0u;
    size_t ssid_n;

    if(ssid != NULL && cap > 0u) {
        ssid[0] = '\0';
    }
    if(line == NULL || ord == NULL || ssid == NULL || cap == 0u) {
        return false;
    }
    if(!bt_digits(line, len, &i, &n) || n == 0u || n > 999u) {
        return false;
    }
    if(i >= len || line[i] != ':') {
        return false;
    }
    i++;
    if(i >= len || line[i] != ' ') {
        return false;
    }
    i++;
    ssid_n = len - i;
    if(ssid_n == 0u || ssid_n > (size_t)SR_BT_SSID_MAX || ssid_n + 1u > cap) {
        return false;
    }
    memcpy(ssid, line + i, ssid_n);
    ssid[ssid_n] = '\0';
    *ord = (unsigned)n;
    return true;
}

bool sr_bt_line_ignore_added(const char* line, size_t len) {
    size_t pre = 0u;

    if(line == NULL) {
        return false;
    }
    return bt_starts(line, len, "Ignore added: ", &pre) && len > pre;
}

bool sr_bt_line_ignore_add_fail(const char* line, size_t len) {
    if(line == NULL) {
        return false;
    }
    return bt_starts(line, len, "Ignore add failed", NULL);
}

bool sr_bt_line_ignore_removed(const char* line, size_t len, unsigned* ord) {
    size_t i = 0u;
    uint32_t n = 0u;

    if(line == NULL || ord == NULL) {
        return false;
    }
    if(!bt_starts(line, len, "Ignore ", &i)) {
        return false;
    }
    if(!bt_digits(line, len, &i, &n) || n == 0u || n > 999u) {
        return false;
    }
    if(!bt_tail(line, len, i, " removed")) {
        return false;
    }
    *ord = (unsigned)n;
    return true;
}

bool sr_bt_line_ignore_bad_index(const char* line, size_t len) {
    if(line == NULL) {
        return false;
    }
    return bt_eq(line, len, "Invalid index");
}

bool sr_bt_line_ignore_cleared(const char* line, size_t len) {
    if(line == NULL) {
        return false;
    }
    return bt_eq(line, len, "Ignore list cleared");
}

bool sr_bt_line_removed_file(const char* line, size_t len, char* name, size_t cap) {
    size_t pre = 0u;
    size_t n;

    if(name != NULL && cap > 0u) {
        name[0] = '\0';
    }
    if(line == NULL || name == NULL || cap == 0u) {
        return false;
    }
    if(!bt_starts(line, len, "Removed file: ", &pre) || len <= pre) {
        return false;
    }
    n = len - pre;
    if(n + 1u > cap) {
        return false;
    }
    memcpy(name, line + pre, n);
    name[n] = '\0';
    return true;
}

bool sr_bt_line_removed_n(const char* line, size_t len, uint32_t* n) {
    size_t i = 0u;
    uint32_t v = 0u;

    if(line == NULL || n == NULL) {
        return false;
    }
    if(!bt_starts(line, len, "Removed ", &i)) {
        return false;
    }
    if(!bt_digits(line, len, &i, &v)) {
        return false;
    }
    if(!bt_tail(line, len, i, " files")) {
        return false;
    }
    *n = v;
    return true;
}

bool sr_bt_line_remove_fail(const char* line, size_t len) {
    if(line == NULL) {
        return false;
    }
    return bt_starts(line, len, "Remove failed", NULL) ||
           bt_starts(line, len, "Could not remove file: ", NULL);
}

bool sr_bt_line_sd_missing(const char* line, size_t len) {
    if(line == NULL) {
        return false;
    }
    return bt_eq(line, len, "SD not available");
}

bool sr_bt_line_split(const char* line, size_t len, uint32_t* kb) {
    size_t i = 0u;
    uint32_t v = 0u;

    if(line == NULL || kb == NULL) {
        return false;
    }
    if(!bt_starts(line, len, "Split: ", &i)) {
        return false;
    }
    if(!bt_digits(line, len, &i, &v)) {
        return false;
    }
    if(i + 3u > len || line[i] != ' ' || line[i + 1u] != 'K' || line[i + 2u] != 'B') {
        return false;
    }
    *kb = v;
    return true;
}

bool sr_bt_line_split_invalid(const char* line, size_t len) {
    if(line == NULL) {
        return false;
    }
    return bt_eq(line, len, "Invalid size (0=off, 4-102400 KB)");
}

bool sr_bt_split_is_preset(uint32_t kb) {
    unsigned i;

    for(i = 0u; i < 6u; i++) {
        if(k_split_kb[i] == kb) {
            return true;
        }
    }
    return false;
}

const char* sr_bt_split_label(unsigned i) {
    if(i >= 6u) {
        return NULL;
    }
    return k_split_label[i];
}

uint32_t sr_bt_split_kb(unsigned i) {
    if(i >= 6u) {
        return 0u;
    }
    return k_split_kb[i];
}

int sr_bt_split_cursor(bool known, uint32_t kb, int pick) {
    unsigned i;

    if(known) {
        for(i = 0u; i < 6u; i++) {
            if(k_split_kb[i] == kb) {
                return (int)i;
            }
        }
        return SR_BT_SPLIT_CUR_NOW;
    }
    if(pick >= 0 && pick < 6) {
        return pick;
    }
    return 0;
}

static void bt_size(uint32_t bytes, char* dst, size_t cap, size_t* out_n) {
    char unit;
    uint32_t n;
    int wr;

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
    wr = snprintf(dst, cap, "%lu%c", (unsigned long)n, unit);
    if(wr < 0 || (size_t)wr >= cap) {
        *out_n = 0u;
        dst[0] = '\0';
        return;
    }
    *out_n = (size_t)wr;
}

bool sr_bt_log_row(const char* name, uint32_t bytes, bool sent, char* dst, size_t cap) {
    char sz[12];
    size_t sz_n = 0u;
    size_t name_n;
    size_t mark;
    size_t i;
    size_t pos;

    if(dst != NULL && cap > 0u) {
        dst[0] = '\0';
    }
    if(name == NULL || dst == NULL || cap < 21u) {
        return false;
    }
    bt_size(bytes, sz, sizeof(sz), &sz_n);
    if(sz_n == 0u) {
        return false;
    }
    name_n = strlen(name);
    mark = sent ? 1u : 0u;
    if(sz_n + 1u + mark >= 20u) {
        return false;
    }
    if(name_n + 1u + sz_n + mark > 20u) {
        name_n = 20u - 1u - sz_n - mark;
        if(name_n == 0u) {
            return false;
        }
        memcpy(dst, name, name_n - 1u);
        dst[name_n - 1u] = '~';
    } else {
        memcpy(dst, name, name_n);
    }
    pos = name_n;
    dst[pos++] = ' ';
    for(i = 0u; i < sz_n; i++) {
        dst[pos++] = sz[i];
    }
    if(sent) {
        dst[pos++] = '*';
    }
    dst[pos] = '\0';
    return true;
}

bool sr_bt_log_more(uint32_t extra, char* dst, size_t cap) {
    int n;

    if(dst != NULL && cap > 0u) {
        dst[0] = '\0';
    }
    if(extra == 0u || dst == NULL || cap == 0u) {
        return false;
    }
    if(extra > 99999u) {
        n = snprintf(dst, cap, "+many more (use PC)");
    } else {
        n = snprintf(dst, cap, "+%lu more (use PC)", (unsigned long)extra);
    }
    if(n < 0 || (size_t)n >= cap || (size_t)n > 20u) {
        dst[0] = '\0';
        return false;
    }
    return true;
}

void sr_bt_show_ssid(const char* ssid, char* dst, size_t cap) {
    size_t lim;
    size_t n;
    size_t i;

    if(dst == NULL || cap == 0u) {
        return;
    }
    dst[0] = '\0';
    if(ssid == NULL || cap < 2u) {
        return;
    }
    lim = cap - 1u;
    if(lim > 20u) {
        lim = 20u;
    }
    n = strlen(ssid);
    if(n <= lim) {
        memcpy(dst, ssid, n + 1u);
        return;
    }
    for(i = 0u; i + 1u < lim; i++) {
        dst[i] = ssid[i];
    }
    dst[lim - 1u] = '~';
    dst[lim] = '\0';
}
