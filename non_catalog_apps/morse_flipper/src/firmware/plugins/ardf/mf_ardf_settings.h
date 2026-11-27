#pragma once

#include "mf_ardf_types.h"

#define MF_ARDF_CONFIG_MAGIC           0x41524446UL
#define MF_ARDF_CONFIG_VERSION         2U
#define MF_ARDF_DEFAULT_INTERVAL_INDEX 10U

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    MfArdfSettings settings;
} MfArdfConfigFile;

void mf_ardf_settings_defaults(MfArdfSettings* settings);
bool mf_ardf_settings_valid(const MfArdfSettings* settings);
bool mf_ardf_settings_decode(MfArdfSettings* settings, const void* bytes, size_t bytes_size);
void mf_ardf_settings_encode(const MfArdfSettings* settings, MfArdfConfigFile* file);
bool mf_ardf_settings_load(MfArdfSettings* settings);
bool mf_ardf_settings_save(const MfArdfSettings* settings);
size_t mf_ardf_normalize_custom(char* output, size_t output_size, const char* input);
uint32_t mf_ardf_interval_seconds(uint8_t index);
const char* mf_ardf_interval_label(uint8_t index);
