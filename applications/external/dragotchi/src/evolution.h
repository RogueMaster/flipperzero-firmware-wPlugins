#ifndef __evolution_h__
#define __evolution_h__
#include "game_structs.h"
/* Advance life stage by age; on Drake->Adult, set alignment from care score. */
GameEventFlags check_evolution(struct GameState *, uint32_t now);
/* Adult only: care-gated old-age death. care >= CARE_IMMORTAL never dies of
 * old age; below that, death odds rise as care falls, after AGE_ELDER. */
GameEventFlags check_old_age(struct GameState *, uint32_t now);
#endif
