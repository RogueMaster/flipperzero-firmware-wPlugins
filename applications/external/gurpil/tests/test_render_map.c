#include "include/ui/render_map.h"

#include "include/domain/shapes.h"
#include "include/domain/terrain.h"

#include <assert.h>
#include <stdio.h>

static void test_world_x_centers_on_vehicle_column(void) {
    // At the vehicle's own column, world x is exactly the current distance, regardless of how
    // far the run has travelled.
    assert(screen_column_to_world_x(0, GURPIL_VEHICLE_COLUMN) == 0);
    assert(screen_column_to_world_x(500, GURPIL_VEHICLE_COLUMN) == 500);
}

static void test_world_x_scrolls_with_column(void) {
    // Columns right of the vehicle look ahead; columns left of it look behind.
    assert(screen_column_to_world_x(500, GURPIL_VEHICLE_COLUMN + 10) == 510);
    assert(screen_column_to_world_x(500, GURPIL_VEHICLE_COLUMN - 10) == 490);
}

static void test_world_x_can_go_negative_near_run_start(void) {
    // terrain_at documents that it clamps negative x to 0, so this is a valid, expected input
    // right after game_start when early columns look behind x=0.
    assert(screen_column_to_world_x(0, 0) < 0);
}

static void test_height_to_screen_y_at_the_bounds(void) {
    assert(terrain_height_to_screen_y(TERRAIN_HEIGHT_MIN) == GURPIL_GROUND_BASELINE_Y);
    // Tallest terrain tops out at the reserved playfield top (leaving the HUD strip clear).
    assert(terrain_height_to_screen_y(TERRAIN_HEIGHT_MAX) == GURPIL_PLAYFIELD_TOP_Y);
}

static void test_height_to_screen_y_is_monotonic_decreasing(void) {
    // Taller ground must draw strictly higher up the screen (smaller y) than shorter ground.
    uint8_t y_low = terrain_height_to_screen_y(5);
    uint8_t y_high = terrain_height_to_screen_y(30);
    assert(y_high < y_low);
}

static void test_height_to_screen_y_clamps_out_of_range(void) {
    assert(terrain_height_to_screen_y(-10) == terrain_height_to_screen_y(TERRAIN_HEIGHT_MIN));
    assert(terrain_height_to_screen_y(1000) == terrain_height_to_screen_y(TERRAIN_HEIGHT_MAX));
}

static void test_shape_for_input_key_matches_design(void) {
    assert(shape_for_input_key(GurpilKeyUp) == ShapeCircle);
    assert(shape_for_input_key(GurpilKeyRight) == ShapeLine);
    assert(shape_for_input_key(GurpilKeyDown) == ShapeSquare);
    assert(shape_for_input_key(GurpilKeyLeft) == ShapeTriangle);
}

static void test_shape_for_input_key_non_shape_keys_are_invalid(void) {
    // ShapeCount is the same "out of range, ignore me" sentinel game_set_shape already guards
    // against, so callers can wire this straight through without an extra branch.
    assert(shape_for_input_key(GurpilKeyOk) == ShapeCount);
    assert(shape_for_input_key(GurpilKeyBack) == ShapeCount);
    assert(shape_for_input_key(GurpilKeyOther) == ShapeCount);
}

static void test_wheel_rotation_step_holds_then_advances(void) {
    // Each step is held for WHEEL_ROTATION_TICKS_PER_STEP frames before advancing.
    assert(wheel_rotation_step(0) == 0);
    assert(wheel_rotation_step(WHEEL_ROTATION_TICKS_PER_STEP - 1) == 0);
    assert(wheel_rotation_step(WHEEL_ROTATION_TICKS_PER_STEP) == 1);
}

static void test_wheel_rotation_step_wraps_after_a_full_spin(void) {
    uint32_t frames_per_spin = WHEEL_ROTATION_STEP_COUNT * WHEEL_ROTATION_TICKS_PER_STEP;
    assert(wheel_rotation_step(frames_per_spin) == wheel_rotation_step(0));
    assert(wheel_rotation_step(frames_per_spin + 1) == wheel_rotation_step(1));
}

static void test_wheel_spoke_endpoint_starts_pointing_right(void) {
    int32_t dx = 0, dy = 0;
    wheel_spoke_endpoint(0, 4, &dx, &dy);
    assert(dx == 4);
    assert(dy == 0);
}

static void test_wheel_spoke_endpoint_stays_within_radius(void) {
    // Every step's offset must stay within the wheel's own radius in both axes, whatever the
    // angle — a spoke that overshoots the glyph it is meant to sit inside would look broken.
    for (uint32_t frame = 0; frame < WHEEL_ROTATION_STEP_COUNT * WHEEL_ROTATION_TICKS_PER_STEP;
         frame++) {
        int32_t dx = 0, dy = 0;
        wheel_spoke_endpoint(frame, 4, &dx, &dy);
        assert(dx >= -4 && dx <= 4);
        assert(dy >= -4 && dy <= 4);
    }
}

static void test_wheel_spoke_endpoint_traces_a_full_rotation(void) {
    // Stepping through one whole spin's worth of frames must visit every discrete direction
    // exactly once before the offset repeats.
    int32_t seen_dx[WHEEL_ROTATION_STEP_COUNT];
    int32_t seen_dy[WHEEL_ROTATION_STEP_COUNT];
    for (uint32_t step = 0; step < WHEEL_ROTATION_STEP_COUNT; step++) {
        wheel_spoke_endpoint(step * WHEEL_ROTATION_TICKS_PER_STEP, 4, &seen_dx[step],
                             &seen_dy[step]);
    }
    for (uint32_t a = 0; a < WHEEL_ROTATION_STEP_COUNT; a++) {
        for (uint32_t b = a + 1; b < WHEEL_ROTATION_STEP_COUNT; b++) {
            assert(seen_dx[a] != seen_dx[b] || seen_dy[a] != seen_dy[b]);
        }
    }
}

static void test_footer_legend_cell_shapes_match_shape_for_input_key(void) {
    // Up/Right/Down/Left order, index-for-index, must mirror shape_for_input_key so the in-play
    // legend never drifts out of sync with the actual D-pad mapping.
    assert(footer_legend_cell(0).shape == shape_for_input_key(GurpilKeyUp));
    assert(footer_legend_cell(1).shape == shape_for_input_key(GurpilKeyRight));
    assert(footer_legend_cell(2).shape == shape_for_input_key(GurpilKeyDown));
    assert(footer_legend_cell(3).shape == shape_for_input_key(GurpilKeyLeft));
}

static void test_footer_legend_cell_slots_are_evenly_spaced_and_ordered(void) {
    // Every cell shares the arrow/glyph row and sits over its own slot's center x, in ascending
    // slot order (Up/Right/Down/Left == slot 0..3) — otherwise the legend would read out of
    // order or cells would overlap. Arrow and glyph now sit side by side on one shared row
    // (see FOOTER_LEGEND_ROW_Y), not stacked, so both x's — not just the glyph's — must advance
    // by a full slot width between cells.
    for (int index = 1; index < FOOTER_LEGEND_CELL_COUNT; index++) {
        FooterLegendCell previous = footer_legend_cell(index - 1);
        FooterLegendCell current = footer_legend_cell(index);
        assert(current.glyph_x - previous.glyph_x == FOOTER_LEGEND_SLOT_WIDTH);
        assert(current.arrow_x - previous.arrow_x == FOOTER_LEGEND_SLOT_WIDTH);
        assert(current.arrow_y == previous.arrow_y);
        assert(current.glyph_y == previous.glyph_y);
    }
}

static void test_footer_legend_cell_arrow_sits_left_of_glyph_on_the_same_row(void) {
    // Each cell's arrow and shape glyph now sit side by side on one shared row (not stacked in
    // two smaller rows, see this module's own comment on FOOTER_LEGEND_ROW_Y) so both can be
    // drawn bigger and still fit the footer's fixed height: same y, arrow to the left.
    for (int index = 0; index < FOOTER_LEGEND_CELL_COUNT; index++) {
        FooterLegendCell cell = footer_legend_cell(index);
        assert(cell.arrow_y == cell.glyph_y);
        assert(cell.arrow_x < cell.glyph_x);
    }
}

// Bounding box of legend cell `index`'s arrow triangle, direction-aware: mirrors the
// Up/Right/Down/Left -> CanvasDirection choice game_view.c's render_footer_legend makes for the
// same index, so this test verifies the actual on-screen footprint rather than a loose,
// direction-agnostic guess.
typedef struct {
    int32_t left, right, top, bottom;
} PixelBounds;

static PixelBounds footer_legend_arrow_bounds(int index, FooterLegendCell cell) {
    int32_t reach = FOOTER_LEGEND_ARROW_REACH;           // tip's reach along the pointed axis.
    int32_t half_width = FOOTER_LEGEND_ARROW_HALF_WIDTH; // reach on the perpendicular axis.
    PixelBounds bounds;
    switch (index) {
        case 0: // Up: tip above the anchor.
            bounds = (PixelBounds){cell.arrow_x - half_width, cell.arrow_x + half_width,
                                   cell.arrow_y - reach, cell.arrow_y};
            break;
        case 1: // Right: tip right of the anchor.
            bounds = (PixelBounds){cell.arrow_x, cell.arrow_x + reach, cell.arrow_y - half_width,
                                   cell.arrow_y + half_width};
            break;
        case 2: // Down: tip below the anchor.
            bounds = (PixelBounds){cell.arrow_x - half_width, cell.arrow_x + half_width,
                                   cell.arrow_y, cell.arrow_y + reach};
            break;
        case 3: // Left: tip left of the anchor.
        default:
            bounds = (PixelBounds){cell.arrow_x - reach, cell.arrow_x, cell.arrow_y - half_width,
                                   cell.arrow_y + half_width};
            break;
    }
    return bounds;
}

static PixelBounds footer_legend_glyph_bounds(FooterLegendCell cell) {
    int32_t half_extent = FOOTER_LEGEND_GLYPH_HALF_EXTENT; // glyph radius + highlight margin.
    return (PixelBounds){cell.glyph_x - half_extent, cell.glyph_x + half_extent,
                         cell.glyph_y - half_extent, cell.glyph_y + half_extent};
}

static void test_footer_legend_cells_fit_inside_the_footer(void) {
    // Every cell's actual arrow footprint (direction-aware, see footer_legend_arrow_bounds) and
    // glyph-plus-highlight-frame footprint must stay within the footer's content band, below the
    // play/footer separator line and above the screen's own last row, and within the screen's
    // own width — regardless of which of the 4 directions that cell's arrow points.
    for (int index = 0; index < FOOTER_LEGEND_CELL_COUNT; index++) {
        FooterLegendCell cell = footer_legend_cell(index);
        PixelBounds arrow = footer_legend_arrow_bounds(index, cell);
        PixelBounds glyph = footer_legend_glyph_bounds(cell);

        assert(arrow.top >= FOOTER_LEGEND_CONTENT_TOP_Y);
        assert(arrow.bottom <= GURPIL_SCREEN_HEIGHT - 1);
        assert(arrow.left >= 0 && arrow.right < GURPIL_SCREEN_WIDTH);

        assert(glyph.top >= FOOTER_LEGEND_CONTENT_TOP_Y);
        assert(glyph.bottom <= GURPIL_SCREEN_HEIGHT - 1);
        assert(glyph.left >= 0 && glyph.right < GURPIL_SCREEN_WIDTH);

        // The arrow never reaches as far as the glyph, whichever direction it points, so the two
        // never touch within a cell.
        assert(arrow.right < glyph.left);
    }
    assert(GURPIL_FOOTER_TOP_Y < GURPIL_SCREEN_HEIGHT);
    assert(GURPIL_GROUND_BASELINE_Y < GURPIL_FOOTER_TOP_Y);
}

static void test_footer_legend_slot_center_x_covers_the_back_slot(void) {
    // The 5th slot (Back, game_view.c's own indicator — not a shape-mapped cell) must still land
    // on screen, evenly spaced alongside the 4 shape cells.
    int32_t back_x = footer_legend_slot_center_x(FOOTER_LEGEND_BACK_SLOT_INDEX);
    assert(back_x > footer_legend_cell(FOOTER_LEGEND_CELL_COUNT - 1).glyph_x);
    assert(back_x < GURPIL_SCREEN_WIDTH);
}

int main(void) {
    test_world_x_centers_on_vehicle_column();
    printf("test_world_x_centers_on_vehicle_column: PASS\n");

    test_world_x_scrolls_with_column();
    printf("test_world_x_scrolls_with_column: PASS\n");

    test_world_x_can_go_negative_near_run_start();
    printf("test_world_x_can_go_negative_near_run_start: PASS\n");

    test_height_to_screen_y_at_the_bounds();
    printf("test_height_to_screen_y_at_the_bounds: PASS\n");

    test_height_to_screen_y_is_monotonic_decreasing();
    printf("test_height_to_screen_y_is_monotonic_decreasing: PASS\n");

    test_height_to_screen_y_clamps_out_of_range();
    printf("test_height_to_screen_y_clamps_out_of_range: PASS\n");

    test_shape_for_input_key_matches_design();
    printf("test_shape_for_input_key_matches_design: PASS\n");

    test_shape_for_input_key_non_shape_keys_are_invalid();
    printf("test_shape_for_input_key_non_shape_keys_are_invalid: PASS\n");

    test_wheel_rotation_step_holds_then_advances();
    printf("test_wheel_rotation_step_holds_then_advances: PASS\n");

    test_wheel_rotation_step_wraps_after_a_full_spin();
    printf("test_wheel_rotation_step_wraps_after_a_full_spin: PASS\n");

    test_wheel_spoke_endpoint_starts_pointing_right();
    printf("test_wheel_spoke_endpoint_starts_pointing_right: PASS\n");

    test_wheel_spoke_endpoint_stays_within_radius();
    printf("test_wheel_spoke_endpoint_stays_within_radius: PASS\n");

    test_wheel_spoke_endpoint_traces_a_full_rotation();
    printf("test_wheel_spoke_endpoint_traces_a_full_rotation: PASS\n");

    test_footer_legend_cell_shapes_match_shape_for_input_key();
    printf("test_footer_legend_cell_shapes_match_shape_for_input_key: PASS\n");

    test_footer_legend_cell_slots_are_evenly_spaced_and_ordered();
    printf("test_footer_legend_cell_slots_are_evenly_spaced_and_ordered: PASS\n");

    test_footer_legend_cell_arrow_sits_left_of_glyph_on_the_same_row();
    printf("test_footer_legend_cell_arrow_sits_left_of_glyph_on_the_same_row: PASS\n");

    test_footer_legend_cells_fit_inside_the_footer();
    printf("test_footer_legend_cells_fit_inside_the_footer: PASS\n");

    test_footer_legend_slot_center_x_covers_the_back_slot();
    printf("test_footer_legend_slot_center_x_covers_the_back_slot: PASS\n");

    return 0;
}
