// SPDX-License-Identifier: GPL-3.0
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef enum {
    PcEnglish,
    PcSpanish,
    PcUkrainian,
    PcLanguageCount
} PcLanguage;
typedef struct {
    PcLanguage language;
    uint8_t duration;
    bool probe_files;
} PcSettings;
#define PC_SETTINGS_BYTES 12u
void pc_settings_default(PcSettings* s);
void pc_settings_encode(const PcSettings* s, uint8_t out[PC_SETTINGS_BYTES]);
bool pc_settings_decode(PcSettings* s, const uint8_t* in, size_t len);
uint32_t pc_duration_ms(const PcSettings* s);
uint32_t pc_crc32(uint32_t previous, const uint8_t* data, size_t len);
