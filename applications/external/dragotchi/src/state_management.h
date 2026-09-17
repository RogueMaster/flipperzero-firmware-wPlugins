#ifndef __STATE_MANAGEMENT_H__
#define __STATE_MANAGEMENT_H__
#include <stdint.h>
#include "game_structs.h"

/* Load (and fast-forward) or freshly initialise the pet. Returns the events
 * that occurred while the app was closed (for the "while you were away" cue). */
GameEventFlags init_state(struct GameState *);
void persist_state(struct GameState *);
void reset_state(struct GameState *);

/* One simulation tick to the current time. Returns event flags. */
GameEventFlags tick_state(struct GameState *);
/* Dispatch a player action (message type) to the game logic at the current
 * time. Returns event flags. */
GameEventFlags do_action(struct GameState *, enum ThreadsMessageType);
/* Hunt: forage the airwaves for a catch (respects cooldown). */
GameEventFlags do_forage(struct GameState *);
/* Legacy: hatch an heir egg into a fresh dragon. */
void do_hatch_heir(struct GameState *);
/* Send the dragon on an expedition of @minutes. */
void do_expedition(struct GameState *, uint16_t minutes);

/* Human-readable status text for the Stats screen. */
void get_state_str(const struct GameState *, char *, size_t);
/* Is it night right now (for the sleep glyph)? */
bool state_is_night_now(void);
uint32_t state_expedition_remaining(const struct GameState *);
#endif
