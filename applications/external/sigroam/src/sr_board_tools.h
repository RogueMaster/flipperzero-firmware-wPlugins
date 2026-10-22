#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * ★ Scout Lite board tools: ignore list, log delete, split size.
 * No furi. The scene arms this; apply_unknown feeds lines.
 * A listing has no end line. After the command echo the scene sends
 * `channel` once, and that echo closes the list. Silence is not empty.
 * Firmware index N is stored as printed. Callers re-list after a change
 * and must not renumber locally.
 */

enum {
    SR_BT_SSID_MAX = 32,
    SR_BT_LIST_MAX = 10,
    SR_BT_DEL_ARM_MS = 1000
};

typedef enum {
    SrBtGateOk = 0,
    SrBtGateScan,
    SrBtGateUpload,
    SrBtGateJoin,
} SrBtGate;

typedef enum {
    SrBtPeerLite = 0,
    SrBtPeerEmpty,
    SrBtPeerOther,
} SrBtPeer;

typedef enum {
    SrBtQuoteOk = 0,
    SrBtQuoteReject,
} SrBtQuote;

typedef enum {
    SrBtKeyLeft = 1,
    SrBtKeyCenter,
    SrBtKeyRight,
    SrBtKeyBack,
} SrBtKey;

typedef enum {
    SrBtDelIdle = 0,
    SrBtDelOne,
    SrBtDelAll1,
    SrBtDelAll2,
} SrBtDelPhase;

typedef enum {
    SrBtDelActNone = 0,
    SrBtDelActCancel,
    SrBtDelActNext,
    SrBtDelActSendOne,
    SrBtDelActSendAll,
} SrBtDelAct;

typedef enum {
    SrBtReplyNone = 0,
    SrBtReplyAdded,
    SrBtReplyAddFail,
    SrBtReplyRemoved,
    SrBtReplyBadIndex,
    SrBtReplyCleared,
    SrBtReplyFile,
    SrBtReplyFiles,
    SrBtReplyRemoveFail,
    SrBtReplySd,
    SrBtReplyBusy,
    SrBtReplySplitBad,
} SrBtReply;

typedef struct {
    uint8_t phase;
    uint32_t opened_ms;
} SrBtDel;

SrBtGate sr_bt_gate(unsigned session, bool upload_flight, bool join_busy);
/* version_nonempty is firmware.version[0] != 0. Empty wins over lite. */
SrBtPeer sr_bt_peer(bool version_nonempty, bool lite);

/* Writes `sigroam ignore add <quoted>\n`. Both quote marks reject. */
SrBtQuote sr_bt_quote_add_cmd(const char* ssid, size_t n, char* dst, size_t cap);
bool sr_bt_ignore_rm_cmd(unsigned ord, char* dst, size_t cap);
bool sr_bt_split_cmd(uint32_t kb, char* dst, size_t cap);
/* Log names are a single token: letters, digits, dot, underscore, hyphen. */
bool sr_bt_rm_cmd(const char* name, char* dst, size_t cap);

bool sr_bt_offer_home(uint8_t home, const char* ssid, bool listed);
bool sr_bt_list_full(unsigned n);
/* False when n_unsent is unknown: the caller omits the line. */
bool sr_bt_del_all_detail(uint32_t n_files, uint32_t n_unsent, char* dst, size_t cap);

void sr_bt_del_reset(SrBtDel* d);
void sr_bt_del_open_one(SrBtDel* d);
void sr_bt_del_open_all(SrBtDel* d, uint32_t now_ms);
/* blocked drops Center/Right. Back and Left still cancel. */
SrBtDelAct sr_bt_del_on_key(SrBtDel* d, unsigned key, uint32_t now_ms, bool blocked);

const char* sr_bt_ignore_cmd(void);
const char* sr_bt_barrier_cmd(void);
const char* sr_bt_split_query_cmd(void);
const char* sr_bt_clear_cmd(void);
const char* sr_bt_rm_all_cmd(void);

void sr_bt_reset(void);
void sr_bt_arm_list(void);
void sr_bt_arm_reply(void);
bool sr_bt_ignore_barrier_due(void);
void sr_bt_ignore_barrier_sent(void);
void sr_bt_feed(const char* line, size_t len);

bool sr_bt_ignore_closed(void);
bool sr_bt_ignore_busy(void);
unsigned sr_bt_ignore_count(void);
bool sr_bt_ignore_item(unsigned i, unsigned* ord, char* ssid, size_t cap);
uint32_t sr_bt_reply_seq(void);
unsigned sr_bt_reply_kind(void);
uint32_t sr_bt_reply_n(void);
bool sr_bt_reply_text(char* dst, size_t cap);
uint32_t sr_bt_split_seq(void);
bool sr_bt_split_value(uint32_t* kb);

bool sr_bt_line_busy(const char* line, size_t len);
bool sr_bt_line_ignore_empty(const char* line, size_t len);
bool sr_bt_line_ignore_item(const char* line, size_t len, unsigned* ord, char* ssid, size_t cap);
bool sr_bt_line_ignore_added(const char* line, size_t len);
bool sr_bt_line_ignore_add_fail(const char* line, size_t len);
bool sr_bt_line_ignore_removed(const char* line, size_t len, unsigned* ord);
bool sr_bt_line_ignore_bad_index(const char* line, size_t len);
bool sr_bt_line_ignore_cleared(const char* line, size_t len);
bool sr_bt_line_removed_file(const char* line, size_t len, char* name, size_t cap);
bool sr_bt_line_removed_n(const char* line, size_t len, uint32_t* n);
bool sr_bt_line_remove_fail(const char* line, size_t len);
bool sr_bt_line_sd_missing(const char* line, size_t len);
bool sr_bt_line_split(const char* line, size_t len, uint32_t* kb);
bool sr_bt_line_split_invalid(const char* line, size_t len);

bool sr_bt_split_is_preset(uint32_t kb);
const char* sr_bt_split_label(unsigned i);
uint32_t sr_bt_split_kb(unsigned i);

/* Cursor after the Split menu is rebuilt: the active preset, the `Now:` row
 * for a non-preset value, or the last pick (0 if none) while the value is unknown. */
enum {
    SR_BT_SPLIT_CUR_NOW = -1
};
int sr_bt_split_cursor(bool known, uint32_t kb, int pick);

/* One row, at most 20 columns. A sent log keeps a trailing '*'. */
bool sr_bt_log_row(const char* name, uint32_t bytes, bool sent, char* dst, size_t cap);
bool sr_bt_log_more(uint32_t extra, char* dst, size_t cap);
void sr_bt_show_ssid(const char* ssid, char* dst, size_t cap);
