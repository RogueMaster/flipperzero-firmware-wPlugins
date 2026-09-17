#ifndef __GAME_DECODER_H__
#define __GAME_DECODER_H__
#include <gui/icon.h>
#include "../game_structs.h"
/* Icon for the current pet, from the full game state. */
const Icon *decode_image(const struct GameState *);
/* Icon for a given stage/alignment/display-state/frame (used by the pet view). */
const Icon *decode_image_for(uint8_t stage, uint8_t alignment, uint8_t display_state, uint32_t frame);
#endif
