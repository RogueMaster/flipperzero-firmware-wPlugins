#ifndef __game_model_h__
#define __game_model_h__
#include "game_structs.h"
/* Initialise a fresh pet (a new egg) at time @now. */
void game_state_init(struct GameState *, uint32_t now);
#endif
