#pragma once

#include "mf_settings_api.h"

#include <stddef.h>

bool mf_settings_snapshot_normalize(MfSettingsSnapshot* snapshot);
uint8_t mf_settings_row_count(uint8_t entry, const MfSettingsSnapshot* snapshot);

#define MF_SETTINGS_CUSTOM_SET_CAP 8U
#define MF_SETTINGS_CUSTOM_NAME_CAP 24U

typedef struct {
    uint8_t count;
    char names[MF_SETTINGS_CUSTOM_SET_CAP][MF_SETTINGS_CUSTOM_NAME_CAP];
} MfSettingsCustomNames;

bool mf_settings_parse_custom_names(
    char* scratch,
    size_t scratch_size,
    MfSettingsCustomNames* names);
