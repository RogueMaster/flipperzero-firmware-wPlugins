#ifndef __care_h__
#define __care_h__
#include "game_structs.h"
/* Adjust the care score, clamped to [CARE_MIN, CARE_MAX]. */
void care_reward(struct PersistentGameState *, int amount);
void care_penalty(struct PersistentGameState *, int amount);
/* Which adult a given care score earns. */
enum DragonAlignment care_band(int32_t care_score);
#endif
