#include "markets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char* markets_name(unsigned index) {
    static const char* names[] = {"Bitcoin BTC/USDT", "Ethereum ETH/USDT", "Gold USD/oz"};
    return index < MARKETS_COUNT ? names[index] : "Markets";
}

const char* markets_url(unsigned index) {
    static const char* urls[] = {
        "https://data-api.binance.vision/api/v3/ticker/24hr?symbol=BTCUSDT",
        "https://data-api.binance.vision/api/v3/ticker/24hr?symbol=ETHUSDT",
        "https://api.gold-api.com/price/XAU",
    };
    return index < MARKETS_COUNT ? urls[index] : NULL;
}

/* Bounded scalar extraction for the providers' flat objects. Reject truncated,
 * escaped or oversized values instead of rendering partial prices. */
static bool field(const char* json, const char* key, char* out, size_t size) {
    char needle[40];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char* p = strstr(json, needle);
    if(!p) return false;
    p += strlen(needle);
    while(*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') ++p;
    if(*p++ != ':') return false;
    while(*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') ++p;
    bool quoted = *p == '"';
    if(quoted) ++p;
    size_t n = 0;
    while(*p && (quoted ? *p != '"' : *p != ',' && *p != '}' && *p != ' ' && *p != '\n')) {
        if(n + 1 >= size || (unsigned char)*p < 32 || *p == '\\') return false;
        out[n++] = *p++;
    }
    out[n] = 0;
    return n && (quoted ? *p == '"' : *p == ',' || *p == '}' || *p == ' ' || *p == '\n');
}
static bool price(const char* json, const char* key, char* out, size_t size) {
    if(!field(json, key, out, size)) return false;
    bool dot = false, digit = false;
    for(char* p = out; *p; ++p) {
        if(*p == '.' && !dot) { dot = true; continue; }
        if(*p < '0' || *p > '9') return false;
        digit = true;
    }
    if(!digit || strtod(out, NULL) <= 0) return false;
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

bool markets_format(unsigned index, const char* json, char* output, size_t capacity) {
    if(index >= MARKETS_COUNT || !json || !output || !capacity) return false;
    output[0] = 0;
    char symbol[16], value[32], updated[40];
    const char* expected[] = {"BTCUSDT", "ETHUSDT", "XAU"};
    if(!field(json, "symbol", symbol, sizeof(symbol)) || strcmp(symbol, expected[index]) ||
       !price(json, index == 2 ? "price" : "lastPrice", value, sizeof(value))) return false;
    int written;
    if(index == 2) {
        char currency[8];
        if(!field(json, "currency", currency, sizeof(currency)) || strcmp(currency, "USD") ||
           !field(json, "updatedAt", updated, sizeof(updated)) || !utc_timestamp(updated)) {
            return false;
        }
        written = snprintf(output, capacity, "%s USD / troy oz\nUpdated: %s\nSource: Gold API", value, updated);
    } else {
        if(!field(json, "closeTime", updated, sizeof(updated)) || !digits(updated, 13U)) {
            return false;
        }
        unsigned long long seconds = strtoull(updated, NULL, 10) / 1000;
        written = snprintf(output, capacity, "%s USDT\nUpdated: %02u:%02u:%02u UTC\nSource: Binance",
            value, (unsigned)(seconds / 3600 % 24), (unsigned)(seconds / 60 % 60), (unsigned)(seconds % 60));
    }
    if(written < 0 || (size_t)written >= capacity) { output[0] = 0; return false; }
    return true;
}
