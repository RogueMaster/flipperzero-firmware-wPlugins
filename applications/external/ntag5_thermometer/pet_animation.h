#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Celebration animations for known tags
 *
 * One file per tag in PET_ANIMATION_DIRECTORY, named "<tag ID as shown on screen>.anim".
 * tools/make_anim.py creates them from a GIF.
 *
 * File format, little endian:
 *
 * | Offset | Size | Field |
 * |---|---|---|
 * | 0 | 4 | magic "PETA" |
 * | 4 | 1 | version, 1 |
 * | 5 | 1 | width in pixels, 1..128 |
 * | 6 | 1 | height in pixels, 1..64 |
 * | 7 | 1 | number of frames, 1..255 |
 * | 8 | 2 | frame duration in ms, 20..10000 |
 * | 10 | 1 | how often the sequence plays, 1..255 |
 * | 11 | 1 | reserved, 0 |
 * | 12 | 20 | pet name, shown during the animation, NUL padded, may be empty |
 * | 32 | ... | frames, (width + 7) / 8 * height bytes each |
 *
 * Frames are XBM bitmaps as canvas_draw_xbm() takes them: rows top to bottom, least
 * significant bit is the leftmost pixel, set bits are black.
 */

#define PET_ANIMATION_DIRECTORY  APP_DATA_PATH("pets")
#define PET_ANIMATION_NAME_SIZE  (20)
#define PET_ANIMATION_FRAME_MAX  (128 / 8 * 64)
#define PET_ANIMATION_WIDTH_MAX  (128)
#define PET_ANIMATION_HEIGHT_MAX (64)

typedef struct PetAnimation PetAnimation;

PetAnimation* pet_animation_alloc(void);

void pet_animation_free(PetAnimation* instance);

/** Open the animation of a tag
 *
 * @param      id    tag ID as shown on screen
 * @return     true if the tag has a valid animation file
 */
bool pet_animation_open(PetAnimation* instance, const char* id);

void pet_animation_close(PetAnimation* instance);

bool pet_animation_is_open(const PetAnimation* instance);

/** Read the next frame
 *
 * @param      frame  buffer of PET_ANIMATION_FRAME_MAX bytes
 * @return     false when all loops have played or the file is broken
 */
bool pet_animation_next_frame(PetAnimation* instance, uint8_t* frame);

uint8_t pet_animation_get_width(const PetAnimation* instance);

uint8_t pet_animation_get_height(const PetAnimation* instance);

uint32_t pet_animation_get_frame_ms(const PetAnimation* instance);

/** Pet name, empty if the file has none */
const char* pet_animation_get_name(const PetAnimation* instance);

#ifdef __cplusplus
}
#endif
