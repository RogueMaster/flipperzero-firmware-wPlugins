#ifndef __economy_h__
#define __economy_h__
#include "game_structs.h"
const char *hoard_rank(uint32_t hoard);
bool has_heir_egg(const struct GameState *);
/* Consume one egg (prefer rare) and start a fresh dragon with a care head-start,
 * preserving the hoard and remaining eggs. Caller ensures an egg exists. */
void hatch_heir(struct GameState *, uint32_t now);
#endif
