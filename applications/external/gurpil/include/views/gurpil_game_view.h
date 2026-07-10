#pragma once

#include "include/application/game.h"

#include <gui/view.h>
#include <gui/view_dispatcher.h>

/*
 * The furi View wrapper around the game itself: owns a GameState plus the wheel-spin frame
 * counter and the persisted `best` shown in the HUD/game-over overlay, all inside the View's own
 * model (ViewModelTypeLocking — see src/views/gurpil_game_view.c) so the Game scene's FuriTimer
 * (a different thread) and the GUI thread's draw/input callbacks never race, mirroring the
 * firmware's own PowerOff view (applications/services/power/power_service/views/power_off.c).
 *
 * The pure gameplay (game_start/game_tick/game_is_over/... in include/application/game.h and the
 * canvas rendering in include/views/game_view.h) is unchanged; this module only adds the
 * furi/View plumbing around it so it can run as one scene under the app's ViewDispatcher.
 */

typedef struct GurpilGameView GurpilGameView;

// Custom event this view sends to the ViewDispatcher when the player asks to leave the game
// (Ok, once the run is over) — the Game scene's on_event switches back to the menu on receiving
// it. Back (at any time, playing or over) does not need this: an unconsumed Back falls through
// to SceneManager's own default "pop to previous scene" behavior.
enum {
    GurpilGameEventReturnToMenu,
};

/* Allocates the view. `view_dispatcher` is kept only to send GurpilGameEventReturnToMenu; it is
 * not retained beyond that (the view is added to it separately, by the caller). */
GurpilGameView *gurpil_game_view_alloc(ViewDispatcher *view_dispatcher);

void gurpil_game_view_free(GurpilGameView *instance);

/* View instance to register with the app's ViewDispatcher/hand to view_dispatcher_add_view. */
View *gurpil_game_view_get_view(const GurpilGameView *instance);

/* Starts a brand-new run: fresh RNG seed, sim/endless/shape reset, animation frame counter back
 * to 0, `best` stored for the HUD/game-over overlay. Called from the Game scene's on_enter. */
void gurpil_game_view_start_run(GurpilGameView *instance, uint32_t seed, int32_t best);

/* Advances the run by one fixed `dt_ms` step and requests a redraw — called every tick from the
 * Game scene's own FuriTimer, unconditionally (a no-op once the run is over, aside from still
 * requesting the redraw, so the frozen final frame keeps repainting like any other view). Also
 * refreshes the checkpoint-flash countdown: reset to its full duration whenever
 * game_checkpoint_just_hit fires, ticked down by `dt_ms` otherwise, so the on-screen "+Ns" flash
 * (game_view.h's `show_checkpoint_flash`) stays lit for a short, fixed real-time span rather than
 * a tick count tied to any particular timer period.
 *
 * Returns true exactly once per run, on the tick the countdown reaches 0, so the caller can score
 * and persist the run's distance without any risk of double-scoring on later ticks. */
bool gurpil_game_view_tick(GurpilGameView *instance, uint32_t dt_ms);

/* The run's distance so far (or its final distance, once over). */
int32_t gurpil_game_view_distance(GurpilGameView *instance);

/* True once this run's distance has strictly exceeded `pre_run_best` — wraps the pure
 * game_is_new_best predicate (application/game.h). Intended to be read right after
 * gurpil_game_view_tick reports the run just ended, with `pre_run_best` the best recorded
 * *before* this run started (i.e. before the caller potentially bumps its own persisted best). */
bool gurpil_game_view_is_new_best(GurpilGameView *instance, int32_t pre_run_best);

/* Updates the `best` shown in the HUD/game-over overlay, and whether the game-over panel should
 * show "New best!" — called right after gurpil_game_view_tick reports the run just ended, once
 * the caller has compared/persisted it (see gurpil_game_view_is_new_best above). */
void gurpil_game_view_set_best(GurpilGameView *instance, int32_t best, bool is_new_best);
