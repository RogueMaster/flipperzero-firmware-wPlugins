/*
 * web_cmd.h — dashboard command parsing (no Arduino dependencies)
 *
 * A dashboard command is one small JSON object, {"cmd":"<name>","value":<v>},
 * sent as a WebSocket text frame or as the body of POST /api/cmd. The message
 * is tokenized as JSON: the command is the value of its "cmd" key and nothing
 * else, and every value is read from the key it belongs to. A command name
 * that turns up inside a string (a network called "nag") is only ever data.
 *
 * No allocation and no copy of the message: a value is a span into the
 * caller's buffer, and a string is decoded straight into the buffer it ends
 * up in. Plain C++ over char buffers so it is unit tested on the host
 * (test/test_web_cmd.cpp); web_dashboard.cpp is the Arduino glue around it.
 */
#ifndef WEB_CMD_H
#define WEB_CMD_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// ── JSON tokenizer ────────────────────────────────────────────────────────────

// Objects and arrays nest at most this deep. wifi_cfg, the deepest command,
// needs 2; the scanner recurses once per level.
static constexpr int WEB_JSON_MAX_DEPTH = 4;
// Longest member name a lookup can match ("sta_ssid" is 8).
static constexpr size_t WEB_JSON_KEY_MAX = 15;

// One JSON value inside a message: [p, p + len), not NUL-terminated.
// p == nullptr is "no such value".
struct WebJson {
    const char *p;
    size_t len;
};

inline const char *web_json_skip_ws(const char *p, const char *end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    return p;
}

// Four hex digits at p, as in \uXXXX.
inline bool web_json_hex4(const char *p, const char *end, uint32_t *out) {
    if (end - p < 4) return false;
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) {
        char c = p[i];
        uint32_t d;
        if (c >= '0' && c <= '9') d = (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f') d = (uint32_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = (uint32_t)(c - 'A' + 10);
        else return false;
        v = (v << 4) | d;
    }
    *out = v;
    return true;
}

// The skip functions take p at the first character of a token and return the
// character after it, or nullptr when the text there is not valid JSON.
inline const char *web_json_skip_string(const char *p, const char *end) {
    if (p >= end || *p != '"') return nullptr;
    for (p++; p < end; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"') return p + 1;
        if (c < 0x20) return nullptr;   // control characters must be escaped
        if (c != '\\') continue;
        if (++p >= end) return nullptr;
        switch (*p) {
        case '"': case '\\': case '/': case 'b': case 'f': case 'n': case 'r': case 't':
            break;
        case 'u': {
            uint32_t unit;
            if (!web_json_hex4(p + 1, end, &unit)) return nullptr;
            p += 4;
            break;
        }
        default:
            return nullptr;
        }
    }
    return nullptr;   // no closing quote
}

inline const char *web_json_skip_number(const char *p, const char *end) {
    auto digits = [end](const char *q) -> const char * {
        const char *start = q;
        while (q < end && *q >= '0' && *q <= '9') q++;
        return q == start ? nullptr : q;
    };
    if (p < end && *p == '-') p++;
    if (p >= end) return nullptr;
    if (*p == '0') p++;   // a leading zero stands alone
    else if (!(p = digits(p))) return nullptr;
    if (p < end && *p == '.' && !(p = digits(p + 1))) return nullptr;
    if (p < end && (*p == 'e' || *p == 'E')) {
        p++;
        if (p < end && (*p == '+' || *p == '-')) p++;
        if (!(p = digits(p))) return nullptr;
    }
    return p;
}

inline const char *web_json_skip_literal(const char *p, const char *end, const char *word) {
    size_t n = strlen(word);
    return ((size_t)(end - p) >= n && memcmp(p, word, n) == 0) ? p + n : nullptr;
}

inline const char *web_json_skip_value(const char *p, const char *end, int depth) {
    if (p >= end) return nullptr;
    switch (*p) {
    case '"': return web_json_skip_string(p, end);
    case 't': return web_json_skip_literal(p, end, "true");
    case 'f': return web_json_skip_literal(p, end, "false");
    case 'n': return web_json_skip_literal(p, end, "null");
    case '{':
    case '[': {
        if (depth >= WEB_JSON_MAX_DEPTH) return nullptr;
        bool object = (*p == '{');
        char close = object ? '}' : ']';
        p = web_json_skip_ws(p + 1, end);
        if (p < end && *p == close) return p + 1;
        for (;;) {
            if (object) {
                if (!(p = web_json_skip_string(p, end))) return nullptr;
                p = web_json_skip_ws(p, end);
                if (p >= end || *p != ':') return nullptr;
                p = web_json_skip_ws(p + 1, end);
            }
            if (!(p = web_json_skip_value(p, end, depth + 1))) return nullptr;
            p = web_json_skip_ws(p, end);
            if (p >= end) return nullptr;
            if (*p == close) return p + 1;
            if (*p != ',') return nullptr;
            p = web_json_skip_ws(p + 1, end);
        }
    }
    default:
        return web_json_skip_number(p, end);
    }
}

// Well-formed UTF-8: no stray or missing continuation bytes, no overlong
// forms, no surrogates, nothing above U+10FFFF.
inline bool web_utf8_valid(const char *s, size_t n) {
    const unsigned char *p = (const unsigned char *)s;
    const unsigned char *end = p + n;
    while (p < end) {
        unsigned char c = *p++;
        if (c < 0x80) continue;
        int more;
        uint32_t cp, min;
        if (c >= 0xC2 && c <= 0xDF)      { more = 1; cp = c & 0x1Fu; min = 0x80; }
        else if (c >= 0xE0 && c <= 0xEF) { more = 2; cp = c & 0x0Fu; min = 0x800; }
        else if (c >= 0xF0 && c <= 0xF4) { more = 3; cp = c & 0x07u; min = 0x10000; }
        else return false;
        for (; more > 0; more--) {
            if (p >= end || (*p & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (*p++ & 0x3Fu);
        }
        if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
    }
    return true;
}

// String value v decoded into out, NUL-terminated, escapes resolved (\uXXXX
// becomes UTF-8, surrogate pairs included). False when v is not a string, when
// it does not fit in cap (nothing is truncated), when the result is not valid
// UTF-8, and for \u0000, which a C string cannot carry. *len, when asked for,
// is the decoded length in bytes.
inline bool web_json_str(WebJson v, char *out, size_t cap, size_t *len = nullptr) {
    if (!v.p || v.len < 2 || v.p[0] != '"' || v.p[v.len - 1] != '"' || cap == 0) return false;
    const char *p = v.p + 1;
    const char *end = v.p + v.len - 1;   // the closing quote
    size_t n = 0;
    while (p < end) {
        uint32_t cp = (unsigned char)*p++;
        if (cp != '\\') {
            // A raw byte, UTF-8 sequences included: checked as a whole below.
            if (n + 1 >= cap) return false;
            out[n++] = (char)cp;
            continue;
        }
        if (p >= end) return false;
        switch (*p++) {
        case '"':  cp = '"';  break;
        case '\\': cp = '\\'; break;
        case '/':  cp = '/';  break;
        case 'b':  cp = '\b'; break;
        case 'f':  cp = '\f'; break;
        case 'n':  cp = '\n'; break;
        case 'r':  cp = '\r'; break;
        case 't':  cp = '\t'; break;
        case 'u':
            if (!web_json_hex4(p, end, &cp)) return false;
            p += 4;
            if (cp >= 0xD800 && cp <= 0xDBFF) {
                // High surrogate: only valid with its low half right behind it.
                uint32_t low;
                if (end - p < 6 || p[0] != '\\' || p[1] != 'u' ||
                    !web_json_hex4(p + 2, end, &low) || low < 0xDC00 || low > 0xDFFF) {
                    return false;
                }
                p += 6;
                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
            } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                return false;
            }
            break;
        default:
            return false;
        }
        if (cp == 0) return false;
        size_t need = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
        if (n + need >= cap) return false;
        if (need == 1) {
            out[n++] = (char)cp;
        } else {
            static const unsigned char lead[] = {0, 0, 0xC0, 0xE0, 0xF0};
            out[n++] = (char)(lead[need] | (cp >> (6 * (need - 1))));
            for (size_t i = need - 1; i > 0; i--) {
                out[n++] = (char)(0x80 | ((cp >> (6 * (i - 1))) & 0x3F));
            }
        }
    }
    out[n] = '\0';
    if (!web_utf8_valid(out, n)) return false;
    if (len) *len = n;
    return true;
}

// true / false, and nothing that merely looks truthy.
inline bool web_json_bool(WebJson v, bool *out) {
    if (v.p && v.len == 4 && memcmp(v.p, "true", 4) == 0)  { *out = true;  return true; }
    if (v.p && v.len == 5 && memcmp(v.p, "false", 5) == 0) { *out = false; return true; }
    return false;
}

// A whole number that fits an int: no fraction, no exponent.
inline bool web_json_int(WebJson v, int *out) {
    if (!v.p || v.len == 0 || v.len > 11) return false;   // "-2147483648"
    bool negative = (v.p[0] == '-');
    size_t i = negative ? 1 : 0;
    if (i >= v.len) return false;
    int64_t n = 0;
    for (; i < v.len; i++) {
        if (v.p[i] < '0' || v.p[i] > '9') return false;
        n = n * 10 + (v.p[i] - '0');
    }
    if (negative) n = -n;
    if (n < INT32_MIN || n > INT32_MAX) return false;
    *out = (int)n;
    return true;
}

// Member `key` of object v. Members are matched on their decoded name; if a
// name repeats the first one counts. False when v is not an object or has no
// such member.
inline bool web_json_get(WebJson v, const char *key, WebJson *out) {
    if (!v.p || v.len < 2 || v.p[0] != '{') return false;
    const char *end = v.p + v.len;
    const char *p = web_json_skip_ws(v.p + 1, end);
    while (p < end && *p == '"') {
        const char *name_end = web_json_skip_string(p, end);
        if (!name_end) return false;
        WebJson name = {p, (size_t)(name_end - p)};
        p = web_json_skip_ws(name_end, end);
        if (p >= end || *p != ':') return false;
        p = web_json_skip_ws(p + 1, end);
        const char *value_end = web_json_skip_value(p, end, 1);
        if (!value_end) return false;
        char text[WEB_JSON_KEY_MAX + 1];
        if (web_json_str(name, text, sizeof(text)) && strcmp(text, key) == 0) {
            out->p = p;
            out->len = (size_t)(value_end - p);
            return true;
        }
        p = web_json_skip_ws(value_end, end);
        if (p < end && *p == ',') p = web_json_skip_ws(p + 1, end);
    }
    return false;
}

// ── Command message ───────────────────────────────────────────────────────────

static constexpr size_t WEB_CMD_NAME_MAX = 31;

enum WebCmdResult {
    WEB_CMD_OK,
    WEB_CMD_RESTART,     // done; the device has to restart to apply it
    WEB_CMD_TOO_LONG,
    WEB_CMD_BAD_JSON,    // not a {"cmd":"..."} message
    WEB_CMD_UNKNOWN,     // no such command (on this board)
    WEB_CMD_BAD_VALUE,   // the command is ours, its value is not usable
};

// Short machine-readable reason for the reply; nullptr when it was not refused.
inline const char *web_cmd_error(WebCmdResult r) {
    switch (r) {
    case WEB_CMD_TOO_LONG:  return "too_long";
    case WEB_CMD_BAD_JSON:  return "bad_json";
    case WEB_CMD_UNKNOWN:   return "unknown_cmd";
    case WEB_CMD_BAD_VALUE: return "bad_value";
    default:                return nullptr;
    }
}

struct WebCmd {
    char name[WEB_CMD_NAME_MAX + 1];   // the "cmd" member
    WebJson value;                     // the "value" member; p == nullptr when absent
    bool bad_value;                    // the value was not what the command takes
    const char *field;                 // wifi_cfg: the setting that was rejected

    bool is(const char *n) const { return strcmp(name, n) == 0; }

    // The value as the type a command takes. False, with bad_value set, when
    // it is absent or of another type: the command then does nothing.
    bool boolean(bool *out)            { return check(web_json_bool(value, out)); }
    bool integer(int *out)             { return check(web_json_int(value, out)); }
    bool string(char *out, size_t cap) { return check(web_json_str(value, out, cap)); }

    // An integer of at least `min`, taken as `cap` above that: a percentage, a
    // timeout. Below `min` it is refused like a value of the wrong type. Done
    // on the int, so the caller can narrow the result without it wrapping.
    bool integer_capped(int *out, int min, int cap) {
        int n;
        if (!integer(&n)) return false;
        if (n < min) return check(false);
        *out = n > cap ? cap : n;
        return true;
    }

    bool check(bool ok) {
        if (!ok) bad_value = true;
        return ok;
    }
};

// Takes one message apart. False unless it is a single JSON object with a
// string "cmd" member and nothing but whitespace around it.
inline bool web_cmd_parse(const char *buf, size_t len, WebCmd *cmd) {
    cmd->name[0] = '\0';
    cmd->value.p = nullptr;
    cmd->value.len = 0;
    cmd->bad_value = false;
    cmd->field = nullptr;
    if (!buf) return false;

    const char *end = buf + len;
    const char *p = web_json_skip_ws(buf, end);
    if (p >= end || *p != '{') return false;
    const char *msg_end = web_json_skip_value(p, end, 0);
    if (!msg_end || web_json_skip_ws(msg_end, end) != end) return false;

    WebJson msg = {p, (size_t)(msg_end - p)};
    WebJson name;
    if (!web_json_get(msg, "cmd", &name) || name.p[0] != '"') return false;
    // A name that does not fit is none of ours: left empty, it matches nothing.
    if (!web_json_str(name, cmd->name, sizeof(cmd->name))) cmd->name[0] = '\0';
    web_json_get(msg, "value", &cmd->value);
    return true;
}

// ── Wi-Fi settings: the value of a wifi_cfg command ───────────────────────────

static constexpr size_t WEB_WIFI_SSID_MAX = 32;
static constexpr size_t WEB_WIFI_PASS_MIN = 8;
static constexpr size_t WEB_WIFI_PASS_MAX = 64;
// The status JSON shows a stored password as this; sent back it means "keep".
static constexpr char WEB_WIFI_PASS_KEEP[] = "***";
// Station hostname. A DNS label may be 63 long, but the Arduino core keeps the
// name in a 32-byte buffer and would cut anything longer than 31 short.
static constexpr size_t WEB_HOSTNAME_MAX = 31;

// One DNS label: 1-31 letters, digits and hyphens, no hyphen at either end.
inline bool web_hostname_valid(const char *s) {
    size_t n = s ? strlen(s) : 0;
    if (n == 0 || n > WEB_HOSTNAME_MAX || s[0] == '-' || s[n - 1] == '-') return false;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-';
        if (!ok) return false;
    }
    return true;
}

// A WPA2 passphrase is 8 to 63 bytes; 64 is the 256-bit key itself, written
// in hex. ESP-IDF (4.4, the pinned Arduino core) takes 64 characters as that
// key on both interfaces: the SoftAP does not start on 64 that are not all hex
// digits (ESP_ERR_WIFI_PASSWORD, so no AP and no dashboard), and the station
// cannot make a key of them. Empty is an open network.
inline bool web_wifi_pass_ok(const char *pass, size_t len) {
    if (len == 0) return true;
    if (len < WEB_WIFI_PASS_MIN) return false;
    if (len < WEB_WIFI_PASS_MAX) return true;
    if (len > WEB_WIFI_PASS_MAX) return false;
    for (size_t i = 0; i < len; i++) {
        char c = pass[i];
        bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!hex) return false;
    }
    return true;
}

struct WebWifiCfg {
    bool has_ssid, has_pass, has_hidden, has_sta_ssid, has_sta_pass, has_host;
    bool hidden;
    char ssid[WEB_WIFI_SSID_MAX + 1];
    char pass[WEB_WIFI_PASS_MAX + 1];
    char sta_ssid[WEB_WIFI_SSID_MAX + 1];
    char sta_pass[WEB_WIFI_PASS_MAX + 1];
    char host[WEB_HOSTNAME_MAX + 1];   // empty = back to the default
};

// Reads the settings out of a wifi_cfg value. Every member is optional: one
// that is absent, or a password sent back as "***", is left as it is on the
// device (has_* stays false). Returns nullptr when every member present is
// valid, else the name of the first one that is not ("value" when the value
// is no object at all) and *cfg must not be used: the settings are taken as a
// whole or not at all.
//
//   ssid      1-32 bytes
//   pass      empty (open AP), 8-63 bytes or 64 hex digits, see
//             web_wifi_pass_ok()
//   hidden    true / false
//   sta_ssid  0-32 bytes, empty = no network
//   sta_pass  as pass; anything up to 64 bytes while sta_ssid is sent empty
//             (the page does not check it then)
//   host      empty (the default) or a hostname, see web_hostname_valid()
//
// Lengths are bytes after decoding, and nothing is cut to fit. Text may not
// hold control characters: they would end up raw in the status JSON.
inline const char *web_wifi_parse(WebJson value, WebWifiCfg *cfg) {
    // 0 = absent, 1 = read, -1 = not acceptable
    auto text = [value](const char *key, char *out, size_t cap, size_t *len) -> int {
        WebJson member;
        if (!web_json_get(value, key, &member)) return 0;
        if (!web_json_str(member, out, cap, len)) return -1;
        for (size_t i = 0; i < *len; i++) {
            unsigned char c = (unsigned char)out[i];
            if (c < 0x20 || c == 0x7F) return -1;
        }
        return 1;
    };

    if (!value.p || value.len < 2 || value.p[0] != '{') return "value";
    size_t len = 0;
    int got;

    got = text("ssid", cfg->ssid, sizeof(cfg->ssid), &len);
    if (got < 0 || (got > 0 && len == 0)) return "ssid";
    cfg->has_ssid = got > 0;

    got = text("pass", cfg->pass, sizeof(cfg->pass), &len);
    if (got < 0) return "pass";
    cfg->has_pass = got > 0 && strcmp(cfg->pass, WEB_WIFI_PASS_KEEP) != 0;
    if (cfg->has_pass && !web_wifi_pass_ok(cfg->pass, len)) return "pass";

    WebJson hidden;
    cfg->has_hidden = web_json_get(value, "hidden", &hidden);
    if (cfg->has_hidden && !web_json_bool(hidden, &cfg->hidden)) return "hidden";

    got = text("sta_ssid", cfg->sta_ssid, sizeof(cfg->sta_ssid), &len);
    if (got < 0) return "sta_ssid";
    cfg->has_sta_ssid = got > 0;

    got = text("sta_pass", cfg->sta_pass, sizeof(cfg->sta_pass), &len);
    if (got < 0) return "sta_pass";
    cfg->has_sta_pass = got > 0 && strcmp(cfg->sta_pass, WEB_WIFI_PASS_KEEP) != 0;
    bool no_network = cfg->has_sta_ssid && cfg->sta_ssid[0] == '\0';
    if (cfg->has_sta_pass && !no_network && !web_wifi_pass_ok(cfg->sta_pass, len)) return "sta_pass";

    got = text("host", cfg->host, sizeof(cfg->host), &len);
    if (got < 0 || (got > 0 && len > 0 && !web_hostname_valid(cfg->host))) return "host";
    cfg->has_host = got > 0;

    return nullptr;
}

#endif // WEB_CMD_H
