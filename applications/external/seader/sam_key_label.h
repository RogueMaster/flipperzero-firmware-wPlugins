#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SEADER_SAM_KEY_LABEL_MAX_LEN 32U

void seader_sam_key_label_format(
    bool sam_present,
    const uint8_t* elite_ice_value,
    size_t elite_ice_value_len,
    char* out,
    size_t out_size);
