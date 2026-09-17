// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "timeclock_pin.h"
#include <furi_hal_random.h>

uint32_t tc_pin_hash(const char* pin, uint32_t salt) {
    // FNV-1a over the salt bytes followed by the PIN characters.
    uint32_t hash = 2166136261u;

    uint8_t salt_bytes[4] = {
        (uint8_t)(salt & 0xFF),
        (uint8_t)((salt >> 8) & 0xFF),
        (uint8_t)((salt >> 16) & 0xFF),
        (uint8_t)((salt >> 24) & 0xFF),
    };
    for(size_t i = 0; i < sizeof(salt_bytes); i++) {
        hash ^= salt_bytes[i];
        hash *= 16777619u;
    }
    for(const char* p = pin; *p != '\0'; p++) {
        hash ^= (uint8_t)(*p);
        hash *= 16777619u;
    }
    return hash;
}

uint32_t tc_pin_make_salt(void) {
    uint32_t salt = 0;
    furi_hal_random_fill_buf((uint8_t*)&salt, sizeof(salt));
    if(salt == 0) salt = 0xA5A5A5A5u; // avoid the trivial zero salt
    return salt;
}
