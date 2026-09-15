#pragma once
#include <stdbool.h>
#include <stddef.h>

#define MARKETS_COUNT 3U

const char* markets_name(unsigned index);
const char* markets_url(unsigned index);
bool markets_format(unsigned index, const char* json, char* output, size_t capacity);
