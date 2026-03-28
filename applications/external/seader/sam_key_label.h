#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SEADER_SAM_KEY_LABEL_MAX_LEN 32U

typedef enum {
    SeaderSamKeyProbeStatusUnknown = 0,
    SeaderSamKeyProbeStatusVerifiedStandard,
    SeaderSamKeyProbeStatusVerifiedValue,
    SeaderSamKeyProbeStatusProbeFailed,
} SeaderSamKeyProbeStatus;

void seader_sam_key_label_format(
    bool sam_present,
    SeaderSamKeyProbeStatus probe_status,
    const uint8_t* elite_ice_value,
    size_t elite_ice_value_len,
    char* out,
    size_t out_size);
