#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    uint16_t local_dit_ms;
    uint8_t lesson;
    uint8_t group_size;
    uint8_t session_groups;
    uint8_t custom_set_idx;
    uint8_t input_source;
    uint8_t farnsworth_wpm;
    uint8_t answer_timeout_s;
    uint8_t group_pause_s;
} MorseFlipperListeningSettings;

void mf_config_test_save(const MorseFlipperListeningSettings* settings, uint8_t out[632]);
bool mf_config_test_load(const uint8_t in[632], MorseFlipperListeningSettings* settings);
