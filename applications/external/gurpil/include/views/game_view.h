#pragma once

#include "include/application/game.h"

#include <gui/canvas.h>

/*
 * The furi isolation boundary for rendering: this header pulls in Canvas, so only furi-aware
 * callers (the Task 11 app layer) include it. GameState and its accessors stay plain C.
 *
 * `gurpil_render` is a pure function of `game` and `best` — it never mutates GameState and
 * never blocks on input, so the caller can (and must) invoke it on every tick via
 * view_port_update, not just in response to input. That is what avoids the blank-UI-until-
 * first-keypress bug seen when a FAP is launched from Favourites/quick buttons: the very first
 * frame must be paintable before any InputEvent ever arrives.
 *
 * Seam for the game-over score: `best` (the persisted best distance) is supplied by the caller
 * rather than stored on GameState, so best_store's furi Storage I/O never has to leak into the
 * pure application/domain layers — the app loads it once via best_store_load and passes it in
 * every frame.
 *
 * Seam for the wheel-spin animation: `frame`, a per-tick counter, is supplied by the caller
 * (rather than kept as static state in this file) so the spin stays deterministic and the app
 * fully controls when it advances (it must freeze once the run is over, and must not run at all
 * while the Game scene isn't entered — see src/views/gurpil_game_view.c and
 * src/scenes/gurpil_scene_game.c).
 *
 * Seam for the checkpoint flash: `show_checkpoint_flash` is computed by the caller (which owns a
 * millisecond countdown in its own view model, decremented every tick — see
 * gurpil_game_view_tick) rather than derived here, since a stateless render function has no way
 * to remember "a checkpoint was crossed a few frames ago" on its own.
 *
 * Seam for "New best!": `is_new_best` is computed by the caller from the pure
 * game_is_new_best(game, pre_run_best) predicate (application/game.h) against the *pre-run*
 * best, since by the time a frame renders the game-over panel, the caller may already have
 * bumped the persisted `best` it passes in to match this run's own distance.
 */

/* Draws one frame: the scrolling terrain silhouette, the next checkpoint's marker (if on
 * screen), the rider-on-a-scooter vehicle with its currently mounted wheel shape spinning per
 * `frame`, a distance+timer HUD, a speed bar (game_speed_permille), the tutorial shape hint
 * (while game_hint_active), a brief "+Ns" flash when `show_checkpoint_flash` is set, an always-
 * on in-play D-pad control legend (bottom-left, Up/Right/Down/Left mapped to their wheel shapes
 * plus a "Back: menu" line, with the currently mounted shape highlighted) while the run is still
 * live, and — once game_is_over(game) — an opaque game-over panel (which replaces the legend)
 * showing the run's final distance, `best`, and "New best!" when `is_new_best` is set. */
void gurpil_render(Canvas *canvas, const GameState *game, int32_t best, uint32_t frame,
                   bool show_checkpoint_flash, bool is_new_best);
