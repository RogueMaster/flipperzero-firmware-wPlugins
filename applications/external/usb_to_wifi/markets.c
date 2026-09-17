#include "markets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char* markets_name(unsigned index) {
    static const char* names[] = {
        "Bitcoin BTC/USDT", "Ethereum ETH/USDT", "Gold USD/oz", "Brent BZ/USDT", "Silver USD/oz"};
    return index < MARKETS_COUNT ? names[index] : "Markets";
}

const char* markets_url(unsigned index) {
    static const char* urls[] = {
        "https://data-api.binance.vision/api/v3/ticker/24hr?symbol=BTCUSDT",
        "https://data-api.binance.vision/api/v3/ticker/24hr?symbol=ETHUSDT",
        "https://api.gold-api.com/price/XAU",
        "https://fapi.binance.com/fapi/v1/ticker/24hr?symbol=BZUSDT",
        "https://api.gold-api.com/price/XAG",
    };
    return index < MARKETS_COUNT ? urls[index] : NULL;
}

bool markets_build_coin_request(
    const char* input,
    char* symbol,
    size_t symbol_capacity,
    char* url,
    size_t url_capacity) {
    if(!input || !symbol || !url || symbol_capacity == 0U || url_capacity == 0U) return false;
    size_t length = strlen(input);
    if(length < 2U || length > 20U) return false;
    if(length + 5U > symbol_capacity) return false;
    for(size_t index = 0U; index < length; ++index) {
        char value = input[index];
        if(value >= 'a' && value <= 'z') value = (char)(value - ('a' - 'A'));
        if(!((value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9'))) return false;
        symbol[index] = value;
    }
    symbol[length] = '\0';
    if(length < 4U || strcmp(symbol + length - 4U, "USDT") != 0) {
        memcpy(symbol + length, "USDT", 5U);
    }
    int written = snprintf(
        url,
        url_capacity,
        "https://data-api.binance.vision/api/v3/ticker/24hr?symbol=%s",
        symbol);
    if(written < 0 || (size_t)written >= url_capacity) {
        symbol[0] = '\0';
        url[0] = '\0';
        return false;
    }
    return true;
}

/* Bounded scalar extraction for the providers' flat objects. Reject truncated,
 * escaped or oversized values instead of rendering partial prices. */
static const char* whitespace(const char* p) {
    while(*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') ++p;
    return p;
}

static bool field(const char* json, const char* key, char* out, size_t size) {
    const char* p = whitespace(json);
    if(*p != '{') return false;
    p = whitespace(p + 1);
    bool found = false;
    while(*p != '}') {
        if(*p != '"') return false;
        const char* name = ++p;
        while(*p && *p != '"') {
            if((unsigned char)*p < 32 || *p == '\\') return false;
            ++p;
        }
        if(!*p) return false;
        bool match = (size_t)(p - name) == strlen(key) &&
                     memcmp(name, key, (size_t)(p - name)) == 0;
        p = whitespace(p + 1);
        if(*p != ':') return false;
        p = whitespace(p + 1);
        bool quoted = *p == '"';
        if(quoted) ++p;
        const char* value = p;
        while(*p && (quoted ? *p != '"' :
              *p != ',' && *p != '}' && whitespace(p) == p)) {
            if((unsigned char)*p < 32 || *p == '\\' ||
               (!quoted && (*p == '{' || *p == '[' || *p == '"'))) return false;
            ++p;
        }
        size_t length = (size_t)(p - value);
        if(!quoted && !length) return false;
        if(match) {
            if(found || !length || length >= size) return false;
            memcpy(out, value, length);
            out[length] = '\0';
            found = true;
        }
        if(quoted) {
            if(*p != '"') return false;
            ++p;
        }
        p = whitespace(p);
        if(*p == '}') break;
        if(*p != ',') return false;
        p = whitespace(p + 1);
        if(*p == '}') return false;
    }
    return found && *whitespace(p + 1) == '\0';
}
static bool price(const char* json, const char* key, char* out, size_t size) {
    if(!field(json, key, out, size)) return false;
    bool dot = false, digit = false;
    for(char* p = out; *p; ++p) {
        if(*p == '.' && !dot) { dot = true; continue; }
        if(*p < '0' || *p > '9') return false;
        digit = true;
    }
    if(!digit || out[0] == '.' || out[strlen(out) - 1] == '.' ||
       strtod(out, NULL) <= 0) return false;
    /* Preserve exact provider decimal text, dropping only insignificant zeros. */
    if(dot) {
        size_t n = strlen(out);
        while(n && out[n - 1] == '0') out[--n] = 0;
        if(n && out[n - 1] == '.') out[--n] = 0;
    }
    return true;
}

static bool digits(const char* value, size_t exact_length) {
    if(strlen(value) != exact_length) return false;
    for(const char* p = value; *p; ++p) {
        if(*p < '0' || *p > '9') return false;
    }
    return true;
}

static bool utc_timestamp(const char* value) {
    if(strlen(value) != 20U) return false;
    for(size_t index = 0U; index < 20U; ++index) {
        const char expected =
            (index == 4U || index == 7U) ? '-' :
            index == 10U ? 'T' :
            (index == 13U || index == 16U) ? ':' :
            index == 19U ? 'Z' :
            '\0';
        if(expected != '\0') {
            if(value[index] != expected) return false;
        } else if(value[index] < '0' || value[index] > '9') {
            return false;
        }
    }
    return true;
}

static bool format_binance(
    const char* expected_symbol,
    const char* unit,
    const char* json,
    char* output,
    size_t capacity) {
    char symbol[MARKETS_MAX_SYMBOL_LENGTH + 1U];
    char value[32];
    char updated[40];
    if(!expected_symbol || !unit || !field(json, "symbol", symbol, sizeof(symbol)) ||
       strcmp(symbol, expected_symbol) ||
       !price(json, "lastPrice", value, sizeof(value)) ||
       !field(json, "closeTime", updated, sizeof(updated)) || !digits(updated, 13U)) {
        return false;
    }
    unsigned long long seconds = strtoull(updated, NULL, 10) / 1000U;
    int written = snprintf(
        output,
        capacity,
        "%s %s\nUpdated: %02u:%02u:%02u UTC\nSource: Binance",
        value,
        unit,
        (unsigned)(seconds / 3600U % 24U),
        (unsigned)(seconds / 60U % 60U),
        (unsigned)(seconds % 60U));
    if(written < 0 || (size_t)written >= capacity) {
        output[0] = '\0';
        return false;
    }
    return true;
}

bool markets_format_coin(
    const char* symbol,
    const char* json,
    char* output,
    size_t capacity) {
    if(!symbol || !json || !output || capacity == 0U) return false;
    output[0] = '\0';
    return format_binance(symbol, "USDT", json, output, capacity);
}

bool markets_format(unsigned index, const char* json, char* output, size_t capacity) {
    if(index >= MARKETS_COUNT || !json || !output || !capacity) return false;
    output[0] = 0;
    char symbol[16], value[32], updated[40];
    int written;
    if(index < 2U) {
        return format_binance(
            index == 0U ? "BTCUSDT" : "ETHUSDT",
            "USDT",
            json,
            output,
            capacity);
    }
    if(index == 3U) {
        return format_binance(
            "BZUSDT", "USDT / barrel", json, output, capacity);
    }
    if(index != 2U && index != 4U) return false;
    const char* expected[] = {"", "", "XAU", "", "XAG"};
    if(!field(json, "symbol", symbol, sizeof(symbol)) || strcmp(symbol, expected[index]) ||
       !price(json, "price", value, sizeof(value))) return false;
    char currency[8];
    if(!field(json, "currency", currency, sizeof(currency)) || strcmp(currency, "USD") ||
       !field(json, "updatedAt", updated, sizeof(updated)) || !utc_timestamp(updated)) {
        return false;
    }
    written = snprintf(
        output,
        capacity,
        "%s USD / troy oz\nUpdated: %s\nSource: Gold API",
        value,
        updated);
    if(written < 0 || (size_t)written >= capacity) { output[0] = 0; return false; }
    return true;
}
