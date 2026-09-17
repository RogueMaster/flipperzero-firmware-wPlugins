#ifndef __discipline_h__
#define __discipline_h__
#include "game_structs.h"
/* True if the pet currently has a genuine unmet need. */
bool has_real_need(const struct GameState *);
/* Maybe raise an attention/discipline "call" (a beep with no real need).
 * More likely when discipline is low. */
GameEventFlags advance_attention(struct GameState *, uint32_t now);
/* Scold action. Correct during a call (+discipline, +care); a misread when
 * there was a real need or no call (-care). Returns true if it was correct. */
bool scold(struct GameState *);
#endif
