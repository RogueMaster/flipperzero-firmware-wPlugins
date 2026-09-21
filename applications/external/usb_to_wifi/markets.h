#pragma once
#include <stdbool.h>
#include <stddef.h>

#define MARKETS_COUNT             5U
#define MARKETS_SEARCH_INDEX      MARKETS_COUNT
#define MARKETS_MAX_SYMBOL_LENGTH 24U

const char* markets_name(unsigned index);
const char* markets_url(unsigned index);
bool markets_format(unsigned index, const char* json, char* output, size_t capacity);
bool markets_build_coin_request(
    const char* input,
    char* symbol,
    size_t symbol_capacity,
    char* url,
    size_t url_capacity);
bool markets_format_coin(const char* symbol, const char* json, char* output, size_t capacity);
bool markets_format_updated_label(const char* formatted_market, char* output, size_t capacity);
