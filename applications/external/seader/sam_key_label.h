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

SeaderSamKeyProbeStatus seader_sam_key_probe_status_from_snmp_result(
    bool probe_succeeded,
    const uint8_t* elite_ice_value,
    size_t elite_ice_value_len);
void seader_sam_key_label_format(
    bool sam_present,
    SeaderSamKeyProbeStatus probe_status,
    const uint8_t* elite_ice_value,
    size_t elite_ice_value_len,
    bool standard_pacs_keys_probed,
    bool standard_pacs_keys_present,
    char* out,
    size_t out_size);
