#include "mf_passive_draw.h"

/* The fixed 69 px row deliberately has no surrounding UI in normal operation. */
void mf_passive_draw(const MfPassiveState* state, Canvas* canvas) {
    int32_t left = (128 - 69) / 2;
    if(state == NULL || canvas == NULL) return;
    canvas_set_font(canvas, FontSecondary);
    if(state->phase == MfPassivePhaseError) {
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "AUDIO ERR");
        return;
    }
    for(uint8_t i = 0U; i < 4U; i++) {
        char ch[2] = {i < state->revealed_count ? state->callsign.text[i] : '_', '\0'};
        canvas_draw_str_aligned(canvas, left + (int32_t)i * 18 + 7, 32, AlignCenter, AlignCenter, ch);
    }
}
