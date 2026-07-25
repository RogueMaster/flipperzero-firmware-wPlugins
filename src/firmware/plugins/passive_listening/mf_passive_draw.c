#include "mf_passive_draw.h"

#include "../common/mf_big_callsign_font.h"

void mf_passive_draw(const MfPassiveState* state, Canvas* canvas) {
    int32_t left;
    if(state == NULL || canvas == NULL) return;
    if(state->phase == MfPassivePhaseLoading) {
        canvas_draw_str(canvas, 31, 34, "Loading");
        canvas_draw_str(canvas, 80, 34, mf_passive_loading_suffix(state->loading.frame));
        return;
    }
    if(state->phase == MfPassivePhaseError) {
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "AUDIO ERR");
        return;
    }
    left = (128 - (int32_t)(state->prompt_len * MF_BIG_CALLSIGN_WIDTH +
                            (state->prompt_len - 1U) * MF_BIG_CALLSIGN_GAP)) /
           2;
    for(uint8_t i = 0U; i < state->prompt_len; i++) {
        mf_big_callsign_draw_char(
            canvas,
            left + (int32_t)i * (MF_BIG_CALLSIGN_WIDTH + MF_BIG_CALLSIGN_GAP),
            (64 - MF_BIG_CALLSIGN_HEIGHT) / 2,
            i < state->revealed_count ? state->prompt[i] : '_',
            false);
    }
}
