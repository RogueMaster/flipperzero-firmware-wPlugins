#include "morse_flipper_passive_loading.h"

#include <string.h>

static const char* const morse_flipper_passive_loading_frames[] = {"...", "...", " ..", ". .", ".. "};

static bool morse_flipper_passive_loading_reached(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

void morse_flipper_passive_loading_start(MorseFlipperPassiveLoading* loading, uint32_t now_ms) {
    if(loading == NULL) return;
    *loading = (MorseFlipperPassiveLoading){
        .active = true,
        .next_at = now_ms + 200U,
        .load_at = now_ms + 1000U,
    };
}

void morse_flipper_passive_loading_clear(MorseFlipperPassiveLoading* loading) {
    if(loading != NULL) memset(loading, 0, sizeof(*loading));
}

bool morse_flipper_passive_loading_tick(MorseFlipperPassiveLoading* loading, uint32_t now_ms) {
    if(loading == NULL || !loading->active) return false;
    while(loading->frame + 1U < (sizeof(morse_flipper_passive_loading_frames) / sizeof(morse_flipper_passive_loading_frames[0])) &&
          morse_flipper_passive_loading_reached(now_ms, loading->next_at)) {
        loading->frame++;
        loading->next_at += 200U;
    }
    if(!morse_flipper_passive_loading_reached(now_ms, loading->load_at)) return false;
    morse_flipper_passive_loading_clear(loading);
    return true;
}

bool morse_flipper_passive_loading_input(
    MorseFlipperPassiveLoading* loading,
    bool short_back,
    bool long_back,
    bool other_press_or_short,
    uint32_t now_ms) {
    if(loading == NULL || !loading->active) return false;
    if(short_back) {
        if(loading->back_clicks == 0U || (uint32_t)(now_ms - loading->last_back_at) > 700U)
            loading->back_clicks = 1U;
        else
            loading->back_clicks++;
        loading->last_back_at = now_ms;
        return loading->back_clicks >= 3U;
    }
    if(long_back || other_press_or_short) loading->back_clicks = 0U;
    return false;
}

const char* morse_flipper_passive_loading_suffix(uint8_t frame) {
    if(frame >= (sizeof(morse_flipper_passive_loading_frames) / sizeof(morse_flipper_passive_loading_frames[0]))) frame = 0U;
    return morse_flipper_passive_loading_frames[frame];
}
