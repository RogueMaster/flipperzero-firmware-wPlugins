#ifndef __game_logic_h__
#define __game_logic_h__
#include "game_structs.h"
/* Advance the whole simulation to @now (used by both the live tick and the
 * offline fast-forward). Returns combined event flags and updates display. */
GameEventFlags advance_state(struct GameState *, uint32_t now);
/* Player actions (each returns event flags for sound/vibro). */
GameEventFlags do_feed(struct GameState *, uint32_t now);
GameEventFlags do_play(struct GameState *, uint32_t now);
GameEventFlags do_clean(struct GameState *, uint32_t now);
GameEventFlags do_medicine(struct GameState *, uint32_t now);
GameEventFlags do_scold(struct GameState *, uint32_t now);
GameEventFlags do_lights(struct GameState *, uint32_t now); // toggles lights
#endif
