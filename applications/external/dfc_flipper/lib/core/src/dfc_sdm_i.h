#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool dfc_sdm_derive_session_key(
    const uint8_t static_key[16],
    uint8_t label_high,
    uint8_t label_low,
    const uint8_t* uid,
    size_t uid_length,
    bool include_uid,
    uint32_t read_counter,
    bool include_read_counter,
    uint8_t output[16]);
