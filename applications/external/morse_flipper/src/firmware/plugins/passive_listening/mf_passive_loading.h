#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool active;
    uint8_t frame;
    uint8_t back_clicks;
    uint32_t next_at;
    uint32_t load_at;
    uint32_t last_back_at;
} MfPassiveLoading;

void mf_passive_loading_start(MfPassiveLoading* loading, uint32_t now_ms);
bool mf_passive_loading_tick(MfPassiveLoading* loading, uint32_t now_ms);
bool mf_passive_loading_input(
    MfPassiveLoading* loading,
    bool short_back,
    bool long_back,
    bool other_press_or_short,
    uint32_t now_ms);
const char* mf_passive_loading_suffix(uint8_t frame);
