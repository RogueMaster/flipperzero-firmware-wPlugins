#pragma once
#include <gui/gui.h>
#include "day.h"
#include "game_state.h"

// Draw the scrollable event card.
// Caller passes the player name for %s substitution in body text.
void event_draw(Canvas* c, const ActiveEvent* ev, const GameState* gs);
