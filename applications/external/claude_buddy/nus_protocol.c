#include "nus_protocol.h"

#include <furi.h>
#include <furi_hal_power.h>
#include <core/memmgr.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ── tiny JSON helpers (kept self-contained; mirrors protocol.c style) ── */

/* Find the first character of the string value for a top-level key.
 * Returns NULL if the key is missing or its value is not a string.
 * Sets *out_len to the raw length.  Does NOT process escapes —
 * Anthropic payloads are short ASCII so passthrough is fine for MVP.
 * Tolerates whitespace between the colon and opening quote, which Python's
 * default json.dumps inserts. */
static const char* json_find_string(const char* json, const char* key, int* out_len) {
    if(!json || !key || !out_len) return NULL;
    char pattern[64];
    int n = snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    if(n <= 0 || n >= (int)sizeof(pattern)) return NULL;
    const char* p = strstr(json, pattern);
    if(!p) return NULL;
    p += n;
    while(*p == ' ') p++;
    if(*p != '"') return NULL;
    p++; /* skip opening quote */
    const char* end = strchr(p, '"');
    if(!end) return NULL;
    *out_len = (int)(end - p);
    return p;
}

static bool json_get_string(const char* json, const char* key, char* out, int out_size) {
    int len = 0;
    const char* start = json_find_string(json, key, &len);
    if(!start) {
        if(out && out_size > 0) out[0] = '\0';
        return false;
    }
    if(len >= out_size) len = out_size - 1;
    memcpy(out, start, len);
    out[len] = '\0';
    return true;
}

static bool json_get_int(const char* json, const char* key, int* out) {
    if(!json || !key || !out) return false;
    char pattern[64];
    int n = snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    if(n <= 0 || n >= (int)sizeof(pattern)) return false;
    const char* start = strstr(json, pattern);
    if(!start) return false;
    start += n;
    while(*start == ' ') start++;
    char* end = NULL;
    long v = strtol(start, &end, 10);
    if(end == start) return false;
    *out = (int)v;
    return true;
}

/* Generalised "find a matching bracketed body" helper.  `open` and
 * `close` control whether we look for an object or array.  Skips string
 * literals (so interior braces/brackets don't mess up the depth count)
 * and escape sequences inside strings. */
static const char*
    json_find_bracketed(const char* json, const char* key, char open, char close, int* out_len) {
    if(!json || !key || !out_len) return NULL;
    char pattern[64];
    int n = snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    if(n <= 0 || n >= (int)sizeof(pattern)) return NULL;
    const char* p = strstr(json, pattern);
    if(!p) return NULL;
    p += n;
    while(*p == ' ') p++;
    if(*p != open) return NULL;
    const char* start = p + 1;
    int depth = 1;
    bool in_str = false;
    bool esc = false;
    for(p = start; *p; p++) {
        if(esc) {
            esc = false;
            continue;
        }
        if(*p == '\\') {
            esc = true;
            continue;
        }
        if(*p == '"') {
            in_str = !in_str;
            continue;
        }
        if(in_str) continue;
        if(*p == open) depth++;
        else if(*p == close) {
            depth--;
            if(depth == 0) {
                *out_len = (int)(p - start);
                return start;
            }
        }
    }
    return NULL;
}

static const char* json_find_object(const char* json, const char* key, int* out_len) {
    return json_find_bracketed(json, key, '{', '}', out_len);
}

static const char* json_find_array(const char* json, const char* key, int* out_len) {
    return json_find_bracketed(json, key, '[', ']', out_len);
}

/* ── public parser ───────────────────────────────────────────── */

bool nus_protocol_parse(const char* json_line, NusMessage* msg) {
    if(!json_line || !msg) return false;
    memset(msg, 0, sizeof(*msg));

    /* Dispatch by recognising distinctive top-level keys. The order
     * matters: heartbeat is the most common, check it first. */
    if(json_get_int(json_line, "total", &msg->total)) {
        msg->kind = NusMsgHeartbeat;
        json_get_int(json_line, "running", &msg->running);
        json_get_int(json_line, "waiting", &msg->waiting);
        int tmp = 0;
        if(json_get_int(json_line, "tokens", &tmp) && tmp > 0) msg->tokens = (uint32_t)tmp;
        if(json_get_int(json_line, "tokens_today", &tmp) && tmp > 0) msg->tokens_today = (uint32_t)tmp;
        json_get_string(json_line, "msg", msg->msg, sizeof(msg->msg));
        /* Entries body pointer is valid only during this call — caller
         * must consume before the JSON line is freed. */
        msg->entries_body = json_find_array(json_line, "entries", &msg->entries_body_len);

        int prompt_len = 0;
        const char* prompt = json_find_object(json_line, "prompt", &prompt_len);
        if(prompt) {
            /* Bound the search to the prompt object body by null-terminating
             * a scratch copy.  Heap-allocate — this parser runs on the BLE
             * event thread (~2 KB stack), so a 192-byte stack frame on top
             * of the enclosing NusMessage risked MemManage faults on long
             * idle sessions. */
            const int bufsz = 192;
            char* buf = malloc(bufsz);
            if(buf) {
                if(prompt_len >= bufsz) prompt_len = bufsz - 1;
                memcpy(buf, prompt, prompt_len);
                buf[prompt_len] = '\0';
                json_get_string(buf, "id", msg->prompt_id, sizeof(msg->prompt_id));
                json_get_string(buf, "tool", msg->prompt_tool, sizeof(msg->prompt_tool));
                json_get_string(buf, "hint", msg->prompt_hint, sizeof(msg->prompt_hint));
                msg->has_prompt = msg->prompt_id[0] != '\0';
                free(buf);
            }
        }
        return true;
    }

    /* {"evt":"turn", "content":[...]} */
    char evt[16] = {0};
    if(json_get_string(json_line, "evt", evt, sizeof(evt))) {
        if(strcmp(evt, "turn") == 0) {
            msg->kind = NusMsgTurn;
            msg->turn_content_body =
                json_find_array(json_line, "content", &msg->turn_content_body_len);
            return true;
        }
    }

    /* {"time":[epoch_seconds, tz_offset_seconds]} */
    int time_len = 0;
    const char* time_body = json_find_array(json_line, "time", &time_len);
    if(time_body) {
        msg->kind = NusMsgTime;
        /* Body is "EPOCH,TZ" — numeric pair. strtoll isn't in the FAP
         * API; use strtol (32-bit, sufficient until 2038) and widen. */
        char* end = NULL;
        long epoch = strtol(time_body, &end, 10);
        msg->time_epoch = (int64_t)epoch;
        if(end && *end == ',') {
            msg->time_tz_offset = (int32_t)strtol(end + 1, NULL, 10);
        }
        return true;
    }

    /* {"cmd":"...", ...} */
    char cmd[16] = {0};
    if(json_get_string(json_line, "cmd", cmd, sizeof(cmd))) {
        if(strcmp(cmd, "status") == 0) {
            msg->kind = NusMsgCmdStatus;
            return true;
        }
        if(strcmp(cmd, "owner") == 0) {
            msg->kind = NusMsgCmdOwner;
            json_get_string(json_line, "name", msg->name, sizeof(msg->name));
            return true;
        }
        if(strcmp(cmd, "name") == 0) {
            msg->kind = NusMsgCmdName;
            json_get_string(json_line, "name", msg->name, sizeof(msg->name));
            return true;
        }
        if(strcmp(cmd, "unpair") == 0) {
            msg->kind = NusMsgCmdUnpair;
            return true;
        }
        if(strcmp(cmd, "char_begin") == 0) {
            msg->kind = NusMsgCmdCharBegin;
            json_get_string(json_line, "name", msg->pack_name, sizeof(msg->pack_name));
            int total = 0;
            json_get_int(json_line, "total", &total);
            msg->pack_total = (uint32_t)total;
            return true;
        }
        if(strcmp(cmd, "file") == 0) {
            msg->kind = NusMsgCmdFile;
            json_get_string(json_line, "path", msg->file_path, sizeof(msg->file_path));
            int size = 0;
            json_get_int(json_line, "size", &size);
            msg->file_size = (uint32_t)size;
            return true;
        }
        if(strcmp(cmd, "chunk") == 0) {
            msg->kind = NusMsgCmdChunk;
            /* The payload can be multi-hundred bytes of base64 — keep a
             * body pointer instead of copying into the stack struct. */
            msg->chunk_body = json_find_string(json_line, "d", &msg->chunk_body_len);
            return true;
        }
        if(strcmp(cmd, "file_end") == 0) {
            msg->kind = NusMsgCmdFileEnd;
            return true;
        }
        if(strcmp(cmd, "char_end") == 0) {
            msg->kind = NusMsgCmdCharEnd;
            return true;
        }
    }

    return false;
}

/* Exposed for the app to extract text blocks from a turn event body.
 * Calls `cb` once per `"type":"text"` block with its "text" string body
 * (excluding quotes).  Returns number of blocks yielded. */
int nus_protocol_foreach_turn_text(
    const char* content_body, int content_body_len,
    void (*cb)(const char* text, int text_len, void* ctx), void* ctx) {
    if(!content_body || content_body_len <= 0 || !cb) return 0;

    /* Each element is an object: walk with depth tracking, then for
     * "type":"text" objects extract the "text" string. */
    const char* p = content_body;
    const char* end = content_body + content_body_len;
    int count = 0;

    while(p < end) {
        while(p < end && *p != '{') p++;
        if(p >= end) break;
        const char* obj_start = p;
        int depth = 1;
        bool in_str = false, esc = false;
        p++;
        while(p < end && depth > 0) {
            if(esc) { esc = false; p++; continue; }
            if(*p == '\\') { esc = true; p++; continue; }
            if(*p == '"') { in_str = !in_str; p++; continue; }
            if(!in_str) {
                if(*p == '{') depth++;
                else if(*p == '}') depth--;
            }
            p++;
        }
        int obj_len = (int)(p - obj_start);

        /* Heap-allocate the scratch copy — 512 bytes is too much for the
         * BLE event thread's stack (~2 KB) once the rest of our call
         * chain is factored in. */
        char* scratch = malloc(512);
        if(!scratch) return count;
        int copy = obj_len;
        if(copy >= 511) copy = 511;
        memcpy(scratch, obj_start, copy);
        scratch[copy] = '\0';

        char type[16] = {0};
        json_get_string(scratch, "type", type, sizeof(type));
        if(strcmp(type, "text") == 0) {
            int tlen = 0;
            const char* t = json_find_string(scratch, "text", &tlen);
            if(t) {
                cb(t, tlen, ctx);
                count++;
            }
        }
        free(scratch);
    }
    return count;
}

/* ── builders ────────────────────────────────────────────────── */

int nus_build_perm_decision(char* buf, int buf_size, const char* id, bool allow) {
    if(!buf || buf_size <= 0 || !id) return 0;
    int n = snprintf(
        buf,
        buf_size,
        "{\"cmd\":\"permission\",\"id\":\"%s\",\"decision\":\"%s\"}\n",
        id,
        allow ? "once" : "deny");
    return (n > 0 && n < buf_size) ? n : 0;
}

int nus_build_ack(char* buf, int buf_size, const char* cmd, uint32_t n) {
    if(!buf || buf_size <= 0 || !cmd) return 0;
    int w = snprintf(
        buf, buf_size, "{\"ack\":\"%s\",\"ok\":true,\"n\":%lu}\n",
        cmd, (unsigned long)n);
    return (w > 0 && w < buf_size) ? w : 0;
}

int nus_build_status_ack(
    char* buf, int buf_size, const char* device_name, bool secure, const NusStats* stats) {
    if(!buf || buf_size <= 0) return 0;

    /* Read telemetry from furi_hal at build time so every status poll
     * reflects current reality.  Values are size-clamped because the
     * desktop treats overflow as missing. */
    int pct = (int)furi_hal_power_get_pct();
    int mV = (int)(furi_hal_power_get_battery_voltage(FuriHalPowerICCharger) * 1000.0f);
    /* Battery current reads in amps; the desktop expects mA.  ST's power IC
     * returns positive for discharge on some hardware and negative on
     * others — pass through verbatim and let the app display handle it. */
    int mA = (int)(furi_hal_power_get_battery_current(FuriHalPowerICCharger) * 1000.0f);
    bool usb = furi_hal_power_is_charging() || furi_hal_power_is_charging_done();

    uint32_t up_s = furi_get_tick() / furi_kernel_get_tick_frequency();
    uint32_t heap = (uint32_t)memmgr_get_free_heap();

    uint32_t appr = stats ? stats->approvals : 0;
    uint32_t deny = stats ? stats->denies : 0;
    uint32_t lvl  = stats ? stats->level : 0;
    /* Velocity: approvals this session is approximate — we'd need a
     * session-start baseline to be exact.  Use a rough per-hour rate
     * over uptime as a stand-in. */
    uint32_t vel = (up_s > 0) ? (appr * 3600u / up_s) : 0;
    /* nap/dizzy counts don't apply to Flipper (no accelerometer); always 0. */

    int n = snprintf(
        buf,
        buf_size,
        "{\"ack\":\"status\",\"ok\":true,\"data\":{"
        "\"name\":\"%s\",\"sec\":%s,"
        "\"bat\":{\"pct\":%d,\"mV\":%d,\"mA\":%d,\"usb\":%s},"
        "\"sys\":{\"up\":%lu,\"heap\":%lu},"
        "\"stats\":{\"appr\":%lu,\"deny\":%lu,\"vel\":%lu,\"nap\":0,\"lvl\":%lu}"
        "}}\n",
        device_name ? device_name : "Claude",
        secure ? "true" : "false",
        pct,
        mV,
        mA,
        usb ? "true" : "false",
        (unsigned long)up_s,
        (unsigned long)heap,
        (unsigned long)appr,
        (unsigned long)deny,
        (unsigned long)vel,
        (unsigned long)lvl);
    return (n > 0 && n < buf_size) ? n : 0;
}
