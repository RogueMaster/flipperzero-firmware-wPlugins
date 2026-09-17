#ifndef __needs_h__
#define __needs_h__
#include "game_structs.h"
/* Advance a need to time @now (decay). Returns event flags. */
GameEventFlags advance_hunger(struct GameState *, uint32_t now);
GameEventFlags advance_happiness(struct GameState *, uint32_t now);
GameEventFlags advance_health(struct GameState *, uint32_t now); // drains if starving/sick/filthy
/* Actions */
void hunger_feed(struct GameState *);       // Feed
void happiness_play(struct GameState *);    // Play
void health_restore(struct GameState *, uint32_t amount);
#endif
