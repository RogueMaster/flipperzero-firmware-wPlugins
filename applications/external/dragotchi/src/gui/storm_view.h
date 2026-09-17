#ifndef __storm_view_h__
#define __storm_view_h__
#include <gui/view.h>
#include "../game_structs.h"

/* Custom event emitted when the user dismisses the Signal Storm screen. */
#define STORM_EVT_DONE 101u

View *storm_view_alloc(void *context);
void storm_view_free(View *);
/* Load the latest board-assisted catch into the view and reset the animation. */
void storm_view_set(View *, const struct GameState *);
/* Advance the animation by one frame (driven by the scene's timer). */
void storm_view_tick(View *);
#endif
