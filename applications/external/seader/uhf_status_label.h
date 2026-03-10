#pragma once

#include <stdbool.h>
#include <stddef.h>

#define SEADER_UHF_STATUS_LABEL_MAX_LEN 64U

void seader_uhf_status_label_format(
    bool has_monza4qt,
    bool monza4qt_key_present,
    bool has_higgs3,
    bool higgs3_key_present,
    char* out,
    size_t out_size);
