#ifndef __states_h__
#define __states_h__
#include "game_structs.h"
/* Hygiene */
GameEventFlags advance_hygiene(struct GameState *, uint32_t now);
bool clean_poop(struct GameState *);           // Clean action; true if there was poop
/* Sickness (odds rise when hungry/dirty) */
GameEventFlags advance_sickness(struct GameState *, uint32_t now);
bool cure_sickness(struct GameState *);        // part of Medicine; true if was sick
/* Sleep / day-night (hour derived from the timestamp) */
bool is_night(uint32_t now);
bool is_asleep(const struct GameState *, uint32_t now);
void set_lights(struct GameState *, bool off, uint32_t now); // Lights action
GameEventFlags advance_sleep(struct GameState *, uint32_t now);
#endif
