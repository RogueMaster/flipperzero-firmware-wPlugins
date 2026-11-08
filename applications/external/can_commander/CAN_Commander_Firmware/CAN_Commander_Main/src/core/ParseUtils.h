#pragma once

#include <Arduino.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "Types.h"

namespace core {
namespace parse {

// Argument strings are space-separated `key=value` tokens.
//
// Two families of helpers:
// - parseX(args, key, out): the key is required. Returns true only when the key is
//   present and its whole value is valid. `out` is untouched on failure.
// - parseOptX(args, key, out): the key is optional. Returns true when the key is
//   absent (`out` untouched) or present and valid; returns false only when the key
//   is present with an invalid value. Callers reject the command on false.
//
// Values are never silently truncated: a value that does not fit the destination is
// invalid. Use parseStringTruncate() only for display-only text (names, units).

inline bool
    findTokenValue(const char* args, const char* key, const char*& value, size_t& value_len) {
    if(args == nullptr || key == nullptr) {
        return false;
    }

    const size_t key_len = strlen(key);
    const char* p = args;
    while(*p != '\0') {
        while(*p == ' ') {
            ++p;
        }
        if(*p == '\0') {
            break;
        }

        const char* tok_start = p;
        while(*p != '\0' && *p != ' ') {
            ++p;
        }
        const char* tok_end = p;

        const char* eq = (const char*)memchr(tok_start, '=', (size_t)(tok_end - tok_start));
        if(eq != nullptr) {
            const size_t tok_key_len = (size_t)(eq - tok_start);
            if(tok_key_len == key_len && strncmp(tok_start, key, key_len) == 0) {
                value = eq + 1;
                value_len = (size_t)(tok_end - eq - 1);
                return true;
            }
        }
    }
    return false;
}

inline bool hasKey(const char* args, const char* key) {
    const char* value = nullptr;
    size_t value_len = 0;
    return findTokenValue(args, key, value, value_len);
}

// Copies the value for `key` into `out`. Returns false when the key is absent or the
// value does not fit (no truncation).
inline bool getTokenValue(const char* args, const char* key, char* out, size_t out_len) {
    if(out == nullptr || out_len == 0) {
        return false;
    }
    const char* value = nullptr;
    size_t value_len = 0;
    if(!findTokenValue(args, key, value, value_len)) {
        return false;
    }
    if(value_len >= out_len) {
        return false;
    }
    memcpy(out, value, value_len);
    out[value_len] = '\0';
    return true;
}

// ---- Whole-string value parsers ----

inline bool parseU64Text(const char* text, uint64_t& out, int base) {
    if(text == nullptr || text[0] == '\0' || text[0] == '-' || text[0] == '+' ||
       isspace((unsigned char)text[0])) {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    const unsigned long long v = strtoull(text, &end, base);
    if(end == text || *end != '\0' || errno == ERANGE) {
        return false;
    }
    out = (uint64_t)v;
    return true;
}

inline bool parseU32Text(const char* text, uint32_t& out, int base) {
    uint64_t v = 0;
    if(!parseU64Text(text, v, base) || v > 0xFFFFFFFFULL) {
        return false;
    }
    out = (uint32_t)v;
    return true;
}

inline bool parseI32Text(const char* text, int32_t& out, int base) {
    if(text == nullptr || text[0] == '\0' || isspace((unsigned char)text[0])) {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    const long long v = strtoll(text, &end, base);
    if(end == text || *end != '\0' || errno == ERANGE || v < INT32_MIN || v > INT32_MAX) {
        return false;
    }
    out = (int32_t)v;
    return true;
}

inline bool parseFloatText(const char* text, float& out) {
    if(text == nullptr || text[0] == '\0' || isspace((unsigned char)text[0])) {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    const float v = strtof(text, &end);
    if(end == text || *end != '\0' || errno == ERANGE || !isfinite(v)) {
        return false;
    }
    out = v;
    return true;
}

inline bool parseBool01Text(const char* text, bool& out) {
    if(text == nullptr) {
        return false;
    }
    if(strcmp(text, "0") == 0) {
        out = false;
        return true;
    }
    if(strcmp(text, "1") == 0) {
        out = true;
        return true;
    }
    return false;
}

inline bool parseBusText(const char* text, BusId& bus) {
    if(text == nullptr) {
        return false;
    }
    if(strcmp(text, "can0") == 0) {
        bus = BusId::CAN0;
        return true;
    }
    if(strcmp(text, "can1") == 0) {
        bus = BusId::CAN1;
        return true;
    }
    if(strcmp(text, "both") == 0) {
        bus = BusId::BOTH;
        return true;
    }
    return false;
}

// ---- Required-key parsers ----

inline bool parseU32(const char* args, const char* key, uint32_t& out, uint32_t base = 10) {
    char buf[32];
    if(!getTokenValue(args, key, buf, sizeof(buf))) {
        return false;
    }
    return parseU32Text(buf, out, (int)base);
}

inline bool parseU64(const char* args, const char* key, uint64_t& out, uint32_t base = 0) {
    char buf[32];
    if(!getTokenValue(args, key, buf, sizeof(buf))) {
        return false;
    }
    return parseU64Text(buf, out, (int)base);
}

inline bool parseI32(const char* args, const char* key, int32_t& out, uint32_t base = 10) {
    char buf[32];
    if(!getTokenValue(args, key, buf, sizeof(buf))) {
        return false;
    }
    return parseI32Text(buf, out, (int)base);
}

inline bool parseFloat(const char* args, const char* key, float& out) {
    char buf[32];
    if(!getTokenValue(args, key, buf, sizeof(buf))) {
        return false;
    }
    return parseFloatText(buf, out);
}

inline bool parseBool01(const char* args, const char* key, bool& out) {
    char buf[4];
    if(!getTokenValue(args, key, buf, sizeof(buf))) {
        return false;
    }
    return parseBool01Text(buf, out);
}

inline bool parseString(const char* args, const char* key, char* out, size_t out_len) {
    return getTokenValue(args, key, out, out_len);
}

// Display-only text (names, units): copies the value, truncating it to fit.
inline bool parseStringTruncate(const char* args, const char* key, char* out, size_t out_len) {
    if(out == nullptr || out_len == 0) {
        return false;
    }
    const char* value = nullptr;
    size_t value_len = 0;
    if(!findTokenValue(args, key, value, value_len)) {
        return false;
    }
    if(value_len >= out_len) {
        value_len = out_len - 1;
    }
    memcpy(out, value, value_len);
    out[value_len] = '\0';
    return true;
}

inline bool parseBus(const char* args, const char* key, BusId& bus) {
    char buf[8];
    if(!getTokenValue(args, key, buf, sizeof(buf))) {
        return false;
    }
    return parseBusText(buf, bus);
}

// ---- Optional-key parsers (absent = ok, present-but-invalid = false) ----

inline bool parseOptU32(const char* args, const char* key, uint32_t& out, uint32_t base = 10) {
    return !hasKey(args, key) || parseU32(args, key, out, base);
}

inline bool parseOptU64(const char* args, const char* key, uint64_t& out, uint32_t base = 0) {
    return !hasKey(args, key) || parseU64(args, key, out, base);
}

inline bool parseOptFloat(const char* args, const char* key, float& out) {
    return !hasKey(args, key) || parseFloat(args, key, out);
}

inline bool parseOptBool01(const char* args, const char* key, bool& out) {
    return !hasKey(args, key) || parseBool01(args, key, out);
}

inline bool parseOptString(const char* args, const char* key, char* out, size_t out_len) {
    return !hasKey(args, key) || parseString(args, key, out, out_len);
}

inline bool parseOptBus(const char* args, const char* key, BusId& bus) {
    return !hasKey(args, key) || parseBus(args, key, bus);
}

// ---- Hex bytes ----

inline int hexNibble(char c) {
    if(c >= '0' && c <= '9') {
        return c - '0';
    }
    if(c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if(c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

// Parses an even-length string of hex digits (no separators) into bytes.
inline bool
    parseHexBytes(const char* hex_str, uint8_t* out, uint8_t max_len, uint8_t& actual_len) {
    actual_len = 0;
    if(hex_str == nullptr || out == nullptr) {
        return false;
    }

    const size_t len = strlen(hex_str);
    if(len == 0 || (len % 2) != 0 || (len / 2) > max_len) {
        return false;
    }

    const uint8_t bytes = (uint8_t)(len / 2);
    uint8_t tmp[64];
    if(bytes > sizeof(tmp)) {
        return false;
    }
    for(uint8_t i = 0; i < bytes; i++) {
        const int hi = hexNibble(hex_str[i * 2]);
        const int lo = hexNibble(hex_str[i * 2 + 1]);
        if(hi < 0 || lo < 0) {
            return false;
        }
        tmp[i] = (uint8_t)((hi << 4) | lo);
    }

    memcpy(out, tmp, bytes);
    actual_len = bytes;
    return true;
}

} // namespace parse
} // namespace core
