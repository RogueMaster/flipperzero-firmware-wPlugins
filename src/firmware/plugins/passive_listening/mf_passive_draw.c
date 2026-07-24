#include "mf_passive_draw.h"

#include "../common/mf_big_callsign_font.h"

/* The fixed 69 px row deliberately has no surrounding UI in normal operation. */
void mf_passive_draw(const MfPassiveState* state, Canvas* canvas) {
    int32_t left = (128 - 69) / 2;
    if(state == NULL || canvas == NULL) return;
    if(state->phase == MfPassivePhaseError) {
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "AUDIO ERR");
        return;
    }
    for(uint8_t i = 0U; i < 4U; i++) {
        mf_big_callsign_draw_char(
            canvas,
            left + (int32_t)i * (MF_BIG_CALLSIGN_WIDTH + MF_BIG_CALLSIGN_GAP),
            (64 - MF_BIG_CALLSIGN_HEIGHT) / 2,
            i < state->revealed_count ? state->callsign.text[i] : '_',
            false);
    }
}
