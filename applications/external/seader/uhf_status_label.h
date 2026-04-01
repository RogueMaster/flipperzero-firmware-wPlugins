#pragma once

#include <stdbool.h>
#include <stddef.h>

#define SEADER_UHF_STATUS_LABEL_MAX_LEN 48U

typedef enum {
    SeaderUhfProbeStatusUnknown = 0,
    SeaderUhfProbeStatusSuccess,
    SeaderUhfProbeStatusFailed,
} SeaderUhfProbeStatus;

void seader_uhf_status_label_format(
    SeaderUhfProbeStatus probe_status,
    bool has_monza4qt,
    bool monza4qt_key_present,
    bool has_higgs3,
    bool higgs3_key_present,
    char* out,
    size_t out_size);
