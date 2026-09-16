#include "markets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char* markets_name(unsigned index) {
    static const char* names[] = {
        "Bitcoin BTC/USDT", "Ethereum ETH/USDT", "Gold USD/oz", "WTI Oil USD/bbl", "Brent Oil USD/bbl", "Silver USD/oz"};
    return index < MARKETS_COUNT ? names[index] : "Markets";
}

const char* markets_url(unsigned index) {
    static const char* urls[] = {
        "https://data-api.binance.vision/api/v3/ticker/24hr?symbol=BTCUSDT",
        "https://data-api.binance.vision/api/v3/ticker/24hr?symbol=ETHUSDT",
        "https://api.gold-api.com/price/XAU",
        "https://americasoilwatch.com/api/v1/wti",
        "https://americasoilwatch.com/api/v1/brent",
        "https://api.gold-api.com/price/XAG",
    };
    return index < MARKETS_COUNT ? urls[index] : NULL;
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

static bool oil_timestamp(const char* value) {
    /* The API reports UTC with milliseconds; accept whole seconds as well. */
    size_t length = strlen(value);
    if(length != 20U && length != 24U) return false;
    char whole_seconds[21];
    memcpy(whole_seconds, value, 19U);
    whole_seconds[19] = 'Z';
    whole_seconds[20] = '\0';
    if(!utc_timestamp(whole_seconds)) return false;
    if(length == 20U) return value[19] == 'Z';
    return value[19] == '.' && value[20] >= '0' && value[20] <= '9' &&
           value[21] >= '0' && value[21] <= '9' &&
           value[22] >= '0' && value[22] <= '9' && value[23] == 'Z';
}

bool markets_format(unsigned index, const char* json, char* output, size_t capacity) {
    if(index >= MARKETS_COUNT || !json || !output || !capacity) return false;
    output[0] = 0;
    char symbol[16], value[32], updated[40];
    int written;
    if(index == 3U || index == 4U) {
        if(!price(json, "priceUsd", value, sizeof(value)) ||
           !field(json, "lastUpdated", updated, sizeof(updated)) ||
           !oil_timestamp(updated)) return false;
        written = snprintf(output, capacity,
                           "%s USD / barrel\nUpdated: %.19s UTC\nSource: AmericasOilWatch",
                           value, updated);
        if(written < 0 || (size_t)written >= capacity) {
            output[0] = '\0';
            return false;
        }
        return true;
    }
    const char* expected[] = {"BTCUSDT", "ETHUSDT", "XAU", "", "", "XAG"};
    if(!field(json, "symbol", symbol, sizeof(symbol)) || strcmp(symbol, expected[index]) ||
       !price(json, (index == 2U || index == 5U) ? "price" : "lastPrice", value, sizeof(value))) return false;
    if(index == 2U || index == 5U) {
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
