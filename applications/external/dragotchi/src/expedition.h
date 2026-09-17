#ifndef __expedition_h__
#define __expedition_h__
#include <stddef.h>
#include "game_structs.h"
void expedition_start(struct GameState *, uint16_t minutes, uint32_t now);
bool expedition_done(const struct GameState *, uint32_t now);
uint32_t expedition_remaining_sec(const struct GameState *, uint32_t now);
/* Roll + apply loot, roll risk, build log, clear on_expedition. Uses global RNG. */
GameEventFlags expedition_resolve(struct GameState *, char *log, size_t logn);
#endif
