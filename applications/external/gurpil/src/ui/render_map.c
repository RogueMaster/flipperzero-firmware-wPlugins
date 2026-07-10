#include "include/ui/render_map.h"

#include "include/domain/terrain.h"

int32_t screen_column_to_world_x(int32_t distance, int column) {
    return distance + (int32_t)(column - GURPIL_VEHICLE_COLUMN);
}

uint8_t terrain_height_to_screen_y(int16_t height) {
    if (height < TERRAIN_HEIGHT_MIN) {
        height = TERRAIN_HEIGHT_MIN;
    } else if (height > TERRAIN_HEIGHT_MAX) {
        height = TERRAIN_HEIGHT_MAX;
    }
    // Scale the whole [0, TERRAIN_HEIGHT_MAX] band into [PLAYFIELD_TOP_Y, BASELINE] so the
    // tallest terrain tops out at PLAYFIELD_TOP_Y — leaving the top strip clear for the HUD.
    int32_t span = GURPIL_GROUND_BASELINE_Y - GURPIL_PLAYFIELD_TOP_Y;
    return (uint8_t)(GURPIL_GROUND_BASELINE_Y - ((int32_t)height * span) / TERRAIN_HEIGHT_MAX);
}

ShapeId shape_for_input_key(GurpilKey key) {
    switch (key) {
        case GurpilKeyUp:
            return ShapeCircle;
        case GurpilKeyRight:
            return ShapeLine;
        case GurpilKeyDown:
            return ShapeSquare;
        case GurpilKeyLeft:
            return ShapeTriangle;
        case GurpilKeyOk:
        case GurpilKeyBack:
        case GurpilKeyOther:
        default:
            return ShapeCount;
    }
}

// One entry per WHEEL_ROTATION_STEP_COUNT step, each a unit vector scaled by 1000 (so callers
// divide back down after multiplying by their radius) — 8 evenly-spaced angles around the
// circle, starting at 0 degrees (pointing right) and stepping counter-clockwise on screen.
typedef struct {
    int16_t dx1000;
    int16_t dy1000;
} WheelSpokeDirection;

static const WheelSpokeDirection WHEEL_SPOKE_DIRECTIONS[WHEEL_ROTATION_STEP_COUNT] = {
    {1000, 0},  {707, -707}, {0, -1000}, {-707, -707},
    {-1000, 0}, {-707, 707}, {0, 1000},  {707, 707},
};

uint32_t wheel_rotation_step(uint32_t frame) {
    return (frame / WHEEL_ROTATION_TICKS_PER_STEP) % WHEEL_ROTATION_STEP_COUNT;
}

void wheel_spoke_endpoint(uint32_t frame, int32_t radius, int32_t *dx, int32_t *dy) {
    const WheelSpokeDirection *direction = &WHEEL_SPOKE_DIRECTIONS[wheel_rotation_step(frame)];
    *dx = (radius * direction->dx1000) / 1000;
    *dy = (radius * direction->dy1000) / 1000;
}

int32_t footer_legend_slot_center_x(int slot_index) {
    return slot_index * FOOTER_LEGEND_SLOT_WIDTH + FOOTER_LEGEND_SLOT_WIDTH / 2;
}

FooterLegendCell footer_legend_cell(int index) {
    FooterLegendCell cell;
    cell.glyph_y = FOOTER_LEGEND_ROW_Y;
    cell.arrow_y = FOOTER_LEGEND_ROW_Y;

    int slot_index;
    switch (index) {
        case 0: // Up
            slot_index = 0;
            cell.shape = ShapeCircle;
            break;
        case 1: // Right
            slot_index = 1;
            cell.shape = ShapeLine;
            break;
        case 2: // Down
            slot_index = 2;
            cell.shape = ShapeSquare;
            break;
        case 3: // Left
        default:
            slot_index = 3;
            cell.shape = ShapeTriangle;
            break;
    }

    int32_t slot_center_x = footer_legend_slot_center_x(slot_index);
    cell.glyph_x = slot_center_x + FOOTER_LEGEND_GLYPH_X_OFFSET;
    cell.arrow_x = slot_center_x + FOOTER_LEGEND_ARROW_X_OFFSET;
    return cell;
}
