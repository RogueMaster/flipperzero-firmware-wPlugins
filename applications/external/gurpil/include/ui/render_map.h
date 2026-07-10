#pragma once

#include "include/domain/shapes.h"

#include <stdint.h>

/*
 * Pure screen-mapping and input-mapping helpers for the Canvas UI layer (no furi, no I/O),
 * host-testable in isolation from Canvas/InputEvent. src/views/game_view.c (the furi-facing
 * render callback) and the Task 11 app's input callback are the only places these get wired to
 * real hardware types.
 *
 * Single source of truth for the 128x64 screen layout: the vehicle's fixed column and the
 * ground baseline row both live here so the terrain/vehicle renderer and any test stay in sync.
 */

enum {
    GURPIL_SCREEN_WIDTH = 128, // display width, px.
    GURPIL_SCREEN_HEIGHT = 64, // display height, px.

    // The bottom footer strip: a dedicated band for the control legend (see FOOTER_LEGEND_* and
    // footer_legend_cell below) so it no longer needs its own opaque panel drawn over the
    // terrain. render_terrain (src/views/game_view.c) now draws the ground as a thin outline
    // capped at GURPIL_GROUND_BASELINE_Y below, so it never dips into the footer band regardless.
    // GURPIL_FOOTER_TOP_Y is both the footer's first row and the row the play/footer separator
    // line draws on.
    GURPIL_FOOTER_HEIGHT = 13,
    GURPIL_FOOTER_TOP_Y = GURPIL_SCREEN_HEIGHT - GURPIL_FOOTER_HEIGHT,

    GURPIL_VEHICLE_COLUMN = 42, // fixed screen column the vehicle sits at (~1/3 from left);
                                // the terrain scrolls under it as the game distance grows.

    // Screen y a height-0 ground column draws at — the playfield's own last row, just above the
    // footer separator, rather than the screen's actual bottom row; taller ground (up to
    // TERRAIN_HEIGHT_MAX) still draws higher up from there. Note this is a fixed-size domain
    // height band (TERRAIN_HEIGHT_MAX, see domain/terrain.h) mapped onto a playfield that is
    // GURPIL_FOOTER_HEIGHT px shorter than the full screen: at the very tallest terrain, the
    // vehicle/rider sprite (which sits on the ground, not fixed to the top strip) has that much
    // less clearance below the HUD than before — already a tight margin pre-footer, see
    // render_vehicle's own comment in game_view.c.
    GURPIL_GROUND_BASELINE_Y = GURPIL_FOOTER_TOP_Y - 1,

    // Screen y the TALLEST terrain (TERRAIN_HEIGHT_MAX) draws at — reserves the top strip for
    // the HUD (distance/time) and the speed bar so terrain/vehicle never collide with them.
    // terrain_height_to_screen_y scales the whole height band into [PLAYFIELD_TOP_Y, BASELINE].
    GURPIL_PLAYFIELD_TOP_Y = 30,
};

/* Maps a screen column (0..GURPIL_SCREEN_WIDTH-1) to the world x sampled by terrain_at, given
 * the current integer game distance: GURPIL_VEHICLE_COLUMN always shows the terrain at
 * `distance`, columns to its right show terrain ahead, columns to its left show terrain
 * already passed. Negative results are valid inputs to terrain_at (it clamps them to 0). */
int32_t screen_column_to_world_x(int32_t distance, int column);

/* Maps a terrain height (clamped to TERRAIN_HEIGHT_MIN..TERRAIN_HEIGHT_MAX game units, see
 * domain/terrain.h) to a screen y: height 0 draws at GURPIL_GROUND_BASELINE_Y, taller ground
 * draws higher up the screen (smaller y). Clamps defensively even though terrain_at already
 * guarantees its samples stay in range. */
uint8_t terrain_height_to_screen_y(int16_t height);

/* Abstraction over the D-pad keys this game reacts to, decoupled from furi's InputKey so this
 * mapping stays host-testable; the input callback (furi-aware, Task 11) translates the real
 * InputKey into this enum before calling shape_for_input_key. */
typedef enum {
    GurpilKeyUp,
    GurpilKeyRight,
    GurpilKeyDown,
    GurpilKeyLeft,
    GurpilKeyOk,
    GurpilKeyBack,
    GurpilKeyOther,
} GurpilKey;

/* Maps a D-pad direction to the wheel shape it mounts: Up->Circle, Right->Line, Down->Square,
 * Left->Triangle. Returns ShapeCount — an invalid shape, per game_set_shape's own out-of-range
 * guard — for keys that don't select a shape (Ok/Back/Other), so callers can pass the result
 * straight to game_set_shape without an extra branch. */
ShapeId shape_for_input_key(GurpilKey key);

/*
 * Wheel-spin animation stepper (no furi, no Canvas): the app passes a per-tick `frame` counter
 * into gurpil_render so the mounted wheel glyph visibly rotates instead of sitting still. Pure
 * integer math, no trig library — a fixed 8-direction lookup table stands in for angle steps.
 */

enum {
    WHEEL_ROTATION_STEP_COUNT = 8,     // discrete angles a full spin steps through.
    WHEEL_ROTATION_TICKS_PER_STEP = 2, // sim ticks each angle is held; lower spins faster.
};

/* Returns the current discrete rotation step (0..WHEEL_ROTATION_STEP_COUNT-1) for `frame`, the
 * app's per-tick counter. Wraps every WHEEL_ROTATION_STEP_COUNT * WHEEL_ROTATION_TICKS_PER_STEP
 * frames. Shape glyphs that approximate rotation with a handful of discrete orientations
 * (square, triangle) index into their own smaller cycle with this same step. */
uint32_t wheel_rotation_step(uint32_t frame);

/* Computes the (dx, dy) offset from a wheel's center to the tip of a spinning spoke/diameter
 * marker of `radius` screen px, for the rotation step `wheel_rotation_step(frame)` selects.
 * Used to draw a rotating spoke on the circle glyph, or a rotating diameter bar on the line
 * glyph. */
void wheel_spoke_endpoint(uint32_t frame, int32_t radius, int32_t *dx, int32_t *dy);

/*
 * In-play D-pad control legend layout (no furi, no Canvas): a horizontal strip of [arrow +
 * shape-glyph] pairs spanning the bottom FOOTER (see GURPIL_FOOTER_HEIGHT above), always visible
 * during PLAY so the player can see which D-pad direction mounts which wheel shape without
 * memorizing the mapping. Arrow and glyph sit side by side on one shared row (not stacked in two
 * smaller rows) so both can use the footer's full content height and read clearly on-device — the
 * earlier stacked layout made both too small to read. The math lives here (not game_view.c) so it
 * is host-testable; game_view.c only adds the furi-side Canvas calls (each
 * arrow's CanvasDirection, the current-shape highlight, the play/footer separator line, and the
 * "Back" indicator in the strip's own 5th slot — its text/position stay in game_view.c since they
 * need no host test).
 */

enum {
    // The footer is divided into FOOTER_LEGEND_SLOT_COUNT equal-width slots: one per D-pad
    // direction that mounts a shape, plus a 5th for the "Back" indicator (game_view.c's own
    // slot, addressed via FOOTER_LEGEND_BACK_SLOT_INDEX below).
    FOOTER_LEGEND_SLOT_COUNT = 5,
    FOOTER_LEGEND_SLOT_WIDTH = GURPIL_SCREEN_WIDTH / FOOTER_LEGEND_SLOT_COUNT,
    FOOTER_LEGEND_CELL_COUNT = 4, // shape-mapped cells only (slots 0..3); slot 4 is Back.
    FOOTER_LEGEND_BACK_SLOT_INDEX = 4,

    FOOTER_LEGEND_CONTENT_TOP_Y = GURPIL_FOOTER_TOP_Y + 1, // first row below the separator line.

    FOOTER_LEGEND_GLYPH_RADIUS = 4,     // radius (or half-size) of each shape glyph, px — doubled
                                        // from the old stacked layout so the shape itself reads
                                        // clearly at a glance, not just its outline.
    FOOTER_LEGEND_HIGHLIGHT_MARGIN = 1, // padding between the mounted shape's glyph and the
                                        // highlight frame drawn around it, px.
    // Farthest either the glyph or its highlight frame reaches from the glyph's own center, px.
    FOOTER_LEGEND_GLYPH_HALF_EXTENT = FOOTER_LEGEND_GLYPH_RADIUS + FOOTER_LEGEND_HIGHLIGHT_MARGIN,

    FOOTER_LEGEND_ARROW_SIZE = 6, // triangle base AND height for each direction arrow, px —
                                  // doubled from the old stacked layout for the same reason.
    // The farthest an arrow's tip reaches from its own anchor point along the axis it points
    // (canvas_draw_triangle's apex sits height-1 px past (x, y)).
    FOOTER_LEGEND_ARROW_REACH = FOOTER_LEGEND_ARROW_SIZE - 1,
    // The farthest an arrow reaches from its own anchor perpendicular to the axis it points
    // (canvas_draw_triangle's base spans +/- base/2 around (x, y)).
    FOOTER_LEGEND_ARROW_HALF_WIDTH = FOOTER_LEGEND_ARROW_SIZE / 2,

    // Shared anchor row for every cell's arrow AND glyph — a single row, vertically centered in
    // the footer's content band, tall enough that the glyph (plus its highlight frame) reaches
    // exactly to the band's own top and bottom edges without spilling past either.
    FOOTER_LEGEND_ROW_Y = FOOTER_LEGEND_CONTENT_TOP_Y + FOOTER_LEGEND_GLYPH_HALF_EXTENT,

    // Horizontal gap, px, between an arrow's farthest reach toward the glyph (the Right-pointing
    // cell's tip, the worst case since every other direction reaches away from the glyph instead)
    // and the glyph's own nearest edge, so the two never touch.
    FOOTER_LEGEND_ARROW_GLYPH_GAP = 2,
    // Glyph center offset from the slot's own center x: nudged right so its own left edge sits
    // just right of the slot center, leaving room for the arrow to its left.
    FOOTER_LEGEND_GLYPH_X_OFFSET = FOOTER_LEGEND_GLYPH_HALF_EXTENT - 1,
    // Arrow anchor offset from the slot's own center x: far enough left that even a Right-
    // pointing arrow's tip (FOOTER_LEGEND_ARROW_REACH px further right than the anchor) still
    // clears the glyph's own left edge (1 px left of the slot center, per
    // FOOTER_LEGEND_GLYPH_X_OFFSET above) by FOOTER_LEGEND_ARROW_GLYPH_GAP px.
    FOOTER_LEGEND_ARROW_X_OFFSET = -(FOOTER_LEGEND_ARROW_REACH + FOOTER_LEGEND_ARROW_GLYPH_GAP + 1),
};

typedef struct {
    int32_t glyph_x, glyph_y; // shape-glyph center.
    int32_t arrow_x, arrow_y; // direction arrow's base/height intersection point (the same (x, y)
                              // canvas_draw_triangle takes) — its tip extends further than this
                              // anchor, per the FOOTER_LEGEND_ARROW_REACH comment above; the
                              // caller picks the CanvasDirection so the tip points the right way.
                              // Shares the glyph's own row (arrow_y == glyph_y) and sits to its
                              // left (arrow_x < glyph_x) — see FOOTER_LEGEND_ARROW_X_OFFSET.
    ShapeId shape;            // the wheel shape this direction mounts.
} FooterLegendCell;

/* Returns the x each footer slot (0..FOOTER_LEGEND_SLOT_COUNT-1, including the Back slot) is
 * centered on, given the strip is divided into FOOTER_LEGEND_SLOT_COUNT equal-width columns. */
int32_t footer_legend_slot_center_x(int slot_index);

/* Returns the layout + mounted shape for legend cell `index` (0..FOOTER_LEGEND_CELL_COUNT-1, in
 * Up/Right/Down/Left order — matching shape_for_input_key). Every
 * cell shares the same arrow/glyph row (FOOTER_LEGEND_ROW_Y) and the same pair of x offsets from
 * its own slot center (FOOTER_LEGEND_ARROW_X_OFFSET, FOOTER_LEGEND_GLYPH_X_OFFSET); only the
 * slot's own center x (footer_legend_slot_center_x) varies between cells. Out-of-range indices
 * fall back to the Left cell, matching this module's existing no-crash convention for the
 * analogous shape_for_input_key default case. */
FooterLegendCell footer_legend_cell(int index);
