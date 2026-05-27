/**
 * Claude Desktop / Cowork wire protocol over BLE NUS.
 *
 * Spec: https://github.com/anthropics/claude-desktop-buddy/blob/main/REFERENCE.md
 *
 * Each line is one JSON object.
 *
 * Desktop → device:
 *   heartbeat snapshot       (every change + 10s keepalive)
 *   {"evt":"turn", ...}      (one-shot per assistant turn)
 *   {"time":[epoch, tz]}     (on connect)
 *   {"cmd":"<cmd>", ...}     (status / owner / name / unpair)
 *
 * Device → desktop:
 *   {"cmd":"permission","id":"<id>","decision":"once"|"deny"}
 *   {"ack":"<cmd>","ok":true,"n":0,"data":{...}?}
 */

#pragma once

#include "nus_stats.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NusMsgUnknown = 0,
    NusMsgHeartbeat,
    NusMsgTurn,
    NusMsgTime,
    NusMsgCmdStatus,
    NusMsgCmdOwner,
    NusMsgCmdName,
    NusMsgCmdUnpair,
    /* Folder push sub-protocol: char_begin → [file → chunk* → file_end]*
     * → char_end. */
    NusMsgCmdCharBegin,
    NusMsgCmdFile,
    NusMsgCmdChunk,
    NusMsgCmdFileEnd,
    NusMsgCmdCharEnd,
} NusMsgKind;

#define NUS_MSG_FIELD_LEN  64
#define NUS_PROMPT_ID_LEN  40
#define NUS_OWNER_NAME_LEN 32

typedef struct {
    NusMsgKind kind;

    /* Heartbeat fields. Always present (zeroed if absent). */
    int total;
    int running;
    int waiting;
    uint32_t tokens;        /* cumulative since desktop start */
    uint32_t tokens_today;  /* resets at local midnight */
    char msg[NUS_MSG_FIELD_LEN];

    /* Permission prompt embedded in heartbeat. */
    bool has_prompt;
    char prompt_id[NUS_PROMPT_ID_LEN];
    char prompt_tool[NUS_MSG_FIELD_LEN];
    char prompt_hint[NUS_MSG_FIELD_LEN];

    /* cmd:owner / cmd:name */
    char name[NUS_OWNER_NAME_LEN];

    /* Raw body pointers into the caller's json_line (valid ONLY until
     * nus_protocol_parse returns).  Present when the corresponding
     * field is in the message; zeroed otherwise.  Used to feed the
     * transcript without copying large arrays into this struct. */
    const char* entries_body;
    int entries_body_len;
    const char* turn_content_body;
    int turn_content_body_len;

    /* cmd:time — epoch seconds + TZ offset in seconds. 0 if missing. */
    int64_t time_epoch;
    int32_t time_tz_offset;

    /* Folder push payload fields (one-of, depending on NusMsgKind). */
    char pack_name[32];         /* char_begin */
    uint32_t pack_total;
    char file_path[64];         /* file */
    uint32_t file_size;
    const char* chunk_body;     /* chunk — base64 string, body pointer */
    int chunk_body_len;
} NusMessage;

bool nus_protocol_parse(const char* json_line, NusMessage* msg);

/* Walk a turn event's `content` array, invoking cb once per text block.
 * The text/text_len pointers are into the original JSON — consume
 * synchronously (don't capture for later). */
int nus_protocol_foreach_turn_text(
    const char* content_body, int content_body_len,
    void (*cb)(const char* text, int text_len, void* ctx), void* ctx);

/* Build outgoing messages.  Each writes a complete line including the
 * trailing newline.  Returns bytes written, or 0 on overflow. */
int nus_build_perm_decision(char* buf, int buf_size, const char* id, bool allow);
/* Generic ack builder. `n` is 0 for most acks and the running byte
 * count for chunk / file_end acks in the folder-push sub-protocol. */
int nus_build_ack(char* buf, int buf_size, const char* cmd, uint32_t n);
/* Deprecated alias for backwards compatibility — prefer nus_build_ack. */
static inline int nus_build_simple_ack(char* buf, int buf_size, const char* cmd) {
    return nus_build_ack(buf, buf_size, cmd, 0);
}
/* Status ack with the full data block the desktop's stats panel expects:
 * name, sec flag, bat (pct/mV/mA/usb), sys (up/heap), stats (appr/deny/
 * vel/nap/lvl).  Values are read from furi_hal at call time. */
int nus_build_status_ack(
    char* buf, int buf_size, const char* device_name, bool secure, const NusStats* stats);

#ifdef __cplusplus
}
#endif
