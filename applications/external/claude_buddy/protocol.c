#include "protocol.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <furi_hal_version.h>

// Minimal JSON field extraction (no external dependency, fixed buffers)
static bool json_get_string(const char* json, const char* key, char* out, int out_size) {
    if(!json || !key || !out || out_size <= 0) return false;
    char pattern[80];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    const char* start = strstr(json, pattern);
    if(!start) return false;
    start += strlen(pattern);
    const char* end = strchr(start, '"');
    if(!end) return false;
    int len = end - start;
    if(len >= out_size) len = out_size - 1;
    memcpy(out, start, len);
    out[len] = '\0';
    return true;
}

static bool json_get_bool(const char* json, const char* key, bool* out) {
    if(!json || !key || !out) return false;
    char pattern[80];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    const char* start = strstr(json, pattern);
    if(!start) return false;
    start += strlen(pattern);
    while(*start == ' ')
        start++;
    if(strncmp(start, "true", 4) == 0) {
        *out = true;
        return true;
    } else if(strncmp(start, "false", 5) == 0) {
        *out = false;
        return true;
    }
    return false;
}

static bool json_get_int(const char* json, const char* key, int* out) {
    if(!json || !key || !out) return false;
    char pattern[80];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    const char* start = strstr(json, pattern);
    if(!start) return false;
    start += strlen(pattern);
    while(*start == ' ')
        start++;
    char* end = NULL;
    long value = strtol(start, &end, 10);
    if(end == start) return false;
    *out = (int)value;
    return true;
}

static MsgType parse_type(const char* type_str) {
    if(!type_str) return MsgTypeUnknown;
    if(strcmp(type_str, "notify") == 0) return MsgTypeNotify;
    if(strcmp(type_str, "ping") == 0) return MsgTypePing;
    if(strcmp(type_str, "status") == 0) return MsgTypeStatus;
    if(strcmp(type_str, "menu") == 0) return MsgTypeMenu;
    if(strcmp(type_str, "state") == 0) return MsgTypeState;
    if(strcmp(type_str, "perm") == 0) return MsgTypePerm;
    if(strcmp(type_str, "usage") == 0) return MsgTypeUsage;
    return MsgTypeUnknown;
}

bool protocol_parse(const char* json_line, ProtocolMessage* msg) {
    if(!json_line || !msg) return false;
    memset(msg, 0, sizeof(ProtocolMessage));

    char type_str[32];
    if(!json_get_string(json_line, "t", type_str, sizeof(type_str))) {
        return false;
    }

    msg->type = parse_type(type_str);
    if(msg->type == MsgTypeUnknown) return false;

    // Extract fields from the "d":{...} section
    const char* d_start = strstr(json_line, "\"d\":{");
    if(!d_start) return false;
    d_start += 4; // point to '{'

    switch(msg->type) {
    case MsgTypeNotify:
        json_get_string(d_start, "sound", msg->sound, sizeof(msg->sound));
        json_get_bool(d_start, "vibro", &msg->vibro);
        json_get_string(d_start, "text", msg->text, sizeof(msg->text));
        json_get_string(d_start, "sub", msg->text2, sizeof(msg->text2));
        break;
    case MsgTypeStatus:
        json_get_string(d_start, "line1", msg->text, sizeof(msg->text));
        json_get_string(d_start, "line2", msg->text2, sizeof(msg->text2));
        break;
    case MsgTypeMenu:
        json_get_string(d_start, "items", msg->menu_data, sizeof(msg->menu_data));
        break;
    case MsgTypeState:
        json_get_bool(d_start, "claude", &msg->claude_connected);
        if(json_get_string(d_start, "host", msg->host_name, sizeof(msg->host_name))) {
            msg->has_host = true;
        }
        break;
    case MsgTypePerm:
        json_get_string(d_start, "tool", msg->text, sizeof(msg->text));
        json_get_string(d_start, "detail", msg->text2, sizeof(msg->text2));
        break;
    case MsgTypePing: {
        int rssi = 0;
        if(json_get_int(d_start, "rssi", &rssi)) {
            msg->has_rssi = true;
            msg->rssi = (int16_t)rssi;
        }
    } break;
    case MsgTypeUsage: {
        int val = 0;
        if(json_get_int(d_start, "ctx", &val)) {
            msg->has_usage_ctx = true;
            if(val < 0) val = 0;
            if(val > 100) val = 100;
            msg->usage_ctx = (uint8_t)val;
        }
        if(json_get_int(d_start, "sess", &val)) {
            msg->has_usage_sess = true;
            if(val < 0) val = 0;
            if(val > 100) val = 100;
            msg->usage_sess = (uint8_t)val;
        }
        if(json_get_int(d_start, "clvl", &val)) {
            msg->has_usage_compact = true;
            if(val < 0) val = 0;
            if(val > 3) val = 3;
            msg->usage_compact = (uint8_t)val;
        }
    } break;
    default:
        break;
    }

    return true;
}

/**
 * Write JSON-escaped *src* into [dst, dst_end). Escapes '"' and '\'.
 * Returns pointer past the last written byte (null terminator position).
 */
static char* json_escape_into(char* dst, const char* dst_end, const char* src) {
    if(!dst || !src) return dst;
    while(*src && dst + 1 < dst_end) {
        if(*src == '"' || *src == '\\') {
            if(dst + 2 >= dst_end) break;
            *dst++ = '\\';
        }
        *dst++ = *src++;
    }
    if(dst < dst_end) *dst = '\0';
    return dst;
}

static int build_simple(char* buf, int buf_size, const char* type) {
    if(!buf || buf_size <= 0 || !type) return 0;
    return snprintf(buf, buf_size, "{\"v\":1,\"t\":\"%s\",\"d\":{}}\n", type);
}

int protocol_build_hello(char* buf, int buf_size) {
    if(!buf || buf_size <= 0) return 0;
    const char* pet = furi_hal_version_get_name_ptr();
    const char* name = pet ? pet : "";
    const char prefix[] = "{\"v\":1,\"t\":\"hello\",\"d\":{\"fw\":\"0.1.0\",\"bt\":\"";
    const char suffix[] = "\"}}\n";
    char* p = buf;
    char* end = buf + buf_size;
    int pfx_len = strlen(prefix);
    if(p + pfx_len >= end) return 0;
    memcpy(p, prefix, pfx_len);
    p += pfx_len;
    p = json_escape_into(p, end, name);
    int sfx_len = strlen(suffix);
    if(p + sfx_len >= end) return 0;
    memcpy(p, suffix, sfx_len);
    p += sfx_len;
    if(p < end) *p = '\0';
    return (int)(p - buf);
}

int protocol_build_cmd(char* buf, int buf_size, const char* text) {
    if(!buf || buf_size <= 0) return 0;
    const char* src = text ? text : "";
    const char prefix[] = "{\"v\":1,\"t\":\"cmd\",\"d\":{\"text\":\"";
    const char suffix[] = "\"}}\n";
    char* p = buf;
    char* end = buf + buf_size;
    int pfx_len = strlen(prefix);
    if(p + pfx_len >= end) return 0;
    memcpy(p, prefix, pfx_len);
    p += pfx_len;
    p = json_escape_into(p, end, src);
    int sfx_len = strlen(suffix);
    if(p + sfx_len >= end) return 0;
    memcpy(p, suffix, sfx_len);
    p += sfx_len;
    if(p < end) *p = '\0';
    return (int)(p - buf);
}

int protocol_build_enter(char* buf, int buf_size) {
    return build_simple(buf, buf_size, "enter");
}

int protocol_build_esc(char* buf, int buf_size) {
    return build_simple(buf, buf_size, "esc");
}

int protocol_build_down(char* buf, int buf_size) {
    return build_simple(buf, buf_size, "down");
}

int protocol_build_voice(char* buf, int buf_size) {
    return build_simple(buf, buf_size, "voice");
}

int protocol_build_space_down(char* buf, int buf_size) {
    return build_simple(buf, buf_size, "space_down");
}

int protocol_build_space_up(char* buf, int buf_size) {
    return build_simple(buf, buf_size, "space_up");
}

int protocol_build_pong(char* buf, int buf_size) {
    return build_simple(buf, buf_size, "pong");
}

int protocol_build_interrupt(char* buf, int buf_size) {
    return build_simple(buf, buf_size, "interrupt");
}

int protocol_build_backspace(char* buf, int buf_size) {
    return build_simple(buf, buf_size, "backspace");
}

int protocol_build_yes(char* buf, int buf_size) {
    return build_simple(buf, buf_size, "yes");
}

int protocol_build_pgup(char* buf, int buf_size) {
    return build_simple(buf, buf_size, "pgup");
}

int protocol_build_pgdown(char* buf, int buf_size) {
    return build_simple(buf, buf_size, "pgdown");
}

int protocol_build_ctrl_o(char* buf, int buf_size) {
    return build_simple(buf, buf_size, "ctrl_o");
}

int protocol_build_ctrl_e(char* buf, int buf_size) {
    return build_simple(buf, buf_size, "ctrl_e");
}

int protocol_build_shift_tab(char* buf, int buf_size) {
    return build_simple(buf, buf_size, "shift_tab");
}

int protocol_build_perm_resp(char* buf, int buf_size, bool allow, bool always, bool esc) {
    if(!buf || buf_size <= 0) return 0;
    return snprintf(
        buf,
        buf_size,
        "{\"v\":1,\"t\":\"perm_resp\",\"d\":{\"allow\":%s,\"always\":%s,\"esc\":%s}}\n",
        allow ? "true" : "false",
        always ? "true" : "false",
        esc ? "true" : "false");
}
