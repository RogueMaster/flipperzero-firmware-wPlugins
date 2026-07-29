#include "mf_passive_draw.h"

#include "../common/mf_big_callsign_font.h"

static const uint8_t mf_passive_back_icon[] = {
    0x04, 0x00, 0x06, 0x00, 0xFF, 0x00, 0x06, 0x01,
    0x04, 0x02, 0x00, 0x02, 0x00, 0x01, 0xF8, 0x00,
};

void mf_passive_draw(const MfPassiveState* state, Canvas* canvas) {
    int32_t left;
    int32_t footer_left;
    uint8_t label_width;
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
    canvas_set_font(canvas, FontSecondary);
    label_width = canvas_string_width(canvas, "To exit press");
    footer_left = (128 - ((int32_t)label_width + 3 * 10 + 2 * 2)) / 2;
    canvas_draw_str(canvas, footer_left, 63, "To exit press");
    footer_left += label_width + 2;
    for(uint8_t i = 0U; i < 3U; i++)
        canvas_draw_xbm(canvas, footer_left + i * 12, 55, 10, 8, mf_passive_back_icon);
}
