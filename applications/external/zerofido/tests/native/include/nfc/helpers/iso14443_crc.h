#pragma once

#include <stdbool.h>

typedef struct BitBuffer BitBuffer;

typedef enum {
    Iso14443CrcTypeA = 0,
} Iso14443CrcType;

void iso14443_crc_append(Iso14443CrcType type, BitBuffer *buffer);
bool iso14443_crc_check(Iso14443CrcType type, const BitBuffer *buffer);
void iso14443_crc_trim(BitBuffer *buffer);
