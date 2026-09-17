#include <furi.h>
#include "game_decoder.h"
#include "dragotchi_icons.h"

const Icon *decode_image_for(uint8_t stage, uint8_t alignment, uint8_t display_state, uint32_t frame) {
    uint32_t f = frame % 2u;
    if(stage == ADULT) {
        if(display_state == DISP_SLEEPING)
            return alignment == ALIGN_WHITE ? &I_adult_white_sleep_60x60 :
                   alignment == ALIGN_BLACK ? &I_adult_black_sleep_60x60 : &I_adult_grey_sleep_60x60;
        if(display_state == DISP_EATING)
            return alignment == ALIGN_WHITE ? &I_adult_white_eat_60x60 :
                   alignment == ALIGN_BLACK ? &I_adult_black_eat_60x60 : &I_adult_grey_eat_60x60;
        if(alignment == ALIGN_WHITE) return f ? &I_adult_white_01_60x60 : &I_adult_white_00_60x60;
        if(alignment == ALIGN_BLACK) return f ? &I_adult_black_01_60x60 : &I_adult_black_00_60x60;
        return f ? &I_adult_grey_01_60x60 : &I_adult_grey_00_60x60;
    }
    switch(stage) {
        case EGG: return f ? &I_egg_01_60x60 : &I_egg_00_60x60;
        case HATCHLING:
            if(display_state == DISP_SLEEPING) return &I_hatch_sleep_60x60;
            if(display_state == DISP_EATING) return &I_hatch_eat_60x60;
            return f ? &I_hatch_01_60x60 : &I_hatch_00_60x60;
        case WYRMLING:
            if(display_state == DISP_SLEEPING) return &I_wyrm_sleep_60x60;
            if(display_state == DISP_EATING) return &I_wyrm_eat_60x60;
            return f ? &I_wyrm_01_60x60 : &I_wyrm_00_60x60;
        case DRAKE:
            if(display_state == DISP_SLEEPING) return &I_drake_sleep_60x60;
            if(display_state == DISP_EATING) return &I_drake_eat_60x60;
            return f ? &I_drake_01_60x60 : &I_drake_00_60x60;
        case DEAD: return f ? &I_dead_01_60x60 : &I_dead_00_60x60;
        default: return &I_egg_00_60x60;
    }
}

const Icon *decode_image(const struct GameState *game_state) {
    return decode_image_for(game_state->persistent.stage,
                            game_state->persistent.alignment,
                            game_state->display_state,
                            game_state->next_animation_index);
}
