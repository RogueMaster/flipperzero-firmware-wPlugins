#include "../vk_thermo.h"
#include "vk_thermo_graph_view.h"
#include <furi.h>
#include <furi_hal.h>
#include <input/input.h>
#include <gui/elements.h>
#include <math.h>

// Graph area dimensions
#define GRAPH_LEFT      28
#define GRAPH_TOP       12
#define GRAPH_WIDTH     92
#define GRAPH_HEIGHT    36
#define MAX_UNIQUE_UIDS 10

typedef enum {
    LineStyleSolid,
    LineStyleDashed,
    LineStyleDotted,
} LineStyle;

struct VkThermoGraphView {
    View* view;
    VkThermoGraphViewCallback callback;
    void* context;
};

typedef struct {
    VkThermoLog* log;
    uint32_t temp_unit;
    bool comparison_mode;
    uint8_t unique_uids[MAX_UNIQUE_UIDS][VK_THERMO_UID_LENGTH];
    uint8_t uid_count;
    uint8_t selected_uid_index;
} VkThermoGraphViewModel;

void vk_thermo_graph_view_set_callback(
    VkThermoGraphView* instance,
    VkThermoGraphViewCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(callback);
    instance->callback = callback;
    instance->context = context;
}

// Check if two UIDs are equal
static bool uids_equal(const uint8_t* uid1, const uint8_t* uid2) {
    return memcmp(uid1, uid2, VK_THERMO_UID_LENGTH) == 0;
}

// Check if UID is all zeros
static bool uid_is_zero(const uint8_t* uid) {
    for(size_t i = 0; i < VK_THERMO_UID_LENGTH; i++) {
        if(uid[i] != 0) return false;
    }
    return true;
}

// Build list of unique UIDs from log
static void build_uid_list(VkThermoGraphViewModel* model) {
    model->uid_count = 0;

    if(!model->log || model->log->count == 0) return;

    for(uint8_t i = 0; i < model->log->count && model->uid_count < MAX_UNIQUE_UIDS; i++) {
        int idx = (model->log->head - model->log->count + i + VK_THERMO_LOG_MAX_ENTRIES) %
                  VK_THERMO_LOG_MAX_ENTRIES;
        VkThermoLogEntry* entry = &model->log->entries[idx];

        if(!entry->valid || uid_is_zero(entry->uid)) continue;

        bool found = false;
        for(uint8_t j = 0; j < model->uid_count; j++) {
            if(uids_equal(model->unique_uids[j], entry->uid)) {
                found = true;
                break;
            }
        }

        if(!found) {
            memcpy(model->unique_uids[model->uid_count], entry->uid, VK_THERMO_UID_LENGTH);
            model->uid_count++;
        }
    }

    if(model->selected_uid_index >= model->uid_count && model->uid_count > 0) {
        model->selected_uid_index = 0;
    }
}

// Get entries for a specific UID
static uint8_t get_entries_for_uid(
    VkThermoGraphViewModel* model,
    const uint8_t* uid,
    float* temps,
    uint8_t max_entries) {
    uint8_t count = 0;

    if(!model->log || model->log->count == 0) return 0;

    for(uint8_t i = 0; i < model->log->count && count < max_entries; i++) {
        int idx = (model->log->head - model->log->count + i + VK_THERMO_LOG_MAX_ENTRIES) %
                  VK_THERMO_LOG_MAX_ENTRIES;
        VkThermoLogEntry* entry = &model->log->entries[idx];

        if(!entry->valid) continue;
        if(!uids_equal(entry->uid, uid)) continue;

        if(model->temp_unit == VkThermoTempUnitFahrenheit) {
            temps[count] = vk_thermo_celsius_to_fahrenheit(entry->temperature_celsius);
        } else if(model->temp_unit == VkThermoTempUnitKelvin) {
            temps[count] = vk_thermo_celsius_to_kelvin(entry->temperature_celsius);
        } else {
            temps[count] = entry->temperature_celsius;
        }
        count++;
    }

    return count;
}

// Draw a line with configurable style (solid, dashed, dotted)
static void draw_styled_line(Canvas* canvas, int x1, int y1, int x2, int y2, LineStyle style) {
    if(style == LineStyleSolid) {
        canvas_draw_line(canvas, x1, y1, x2, y2);
        return;
    }

    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int steps = (dx > dy) ? dx : dy;
    if(steps == 0) {
        canvas_draw_dot(canvas, x1, y1);
        return;
    }

    int gap = (style == LineStyleDashed) ? 3 : 1;
    for(int i = 0; i <= steps; i++) {
        if((i / gap) % 2 == 0) {
            int x = x1 + (x2 - x1) * i / steps;
            int y = y1 + (y2 - y1) * i / steps;
            canvas_draw_dot(canvas, x, y);
        }
    }
}

// Quadratic Bezier interpolation
static float bezier_point(float t, float p0, float p1, float p2) {
    float mt = 1.0f - t;
    return mt * mt * p0 + 2.0f * mt * t * p1 + t * t * p2;
}

// Draw Bezier curve segment with line style
static void draw_bezier_segment(
    Canvas* canvas,
    int x0,
    int y0,
    int x1,
    int y1,
    int x2,
    int y2,
    LineStyle style) {
    const int steps = 8;
    int prev_x = x0;
    int prev_y = y0;

    for(int i = 1; i <= steps; i++) {
        float t = (float)i / steps;
        int x = (int)bezier_point(t, (float)x0, (float)x1, (float)x2);
        int y = (int)bezier_point(t, (float)y0, (float)y1, (float)y2);
        draw_styled_line(canvas, prev_x, prev_y, x, y, style);
        prev_x = x;
        prev_y = y;
    }
}

// Draw 3x3 filled square marker at a data point
static void draw_marker(Canvas* canvas, int x, int y) {
    for(int dy = -1; dy <= 1; dy++) {
        for(int dx = -1; dx <= 1; dx++) {
            int px = x + dx;
            int py = y + dy;
            if(px >= GRAPH_LEFT && px < GRAPH_LEFT + GRAPH_WIDTH && py >= GRAPH_TOP &&
               py < GRAPH_TOP + GRAPH_HEIGHT) {
                canvas_draw_dot(canvas, px, py);
            }
        }
    }
}

// Draw hollow square marker (for comparison mode - second UID)
static void draw_marker_hollow(Canvas* canvas, int x, int y) {
    for(int d = -1; d <= 1; d++) {
        if(x - 1 >= GRAPH_LEFT && y + d >= GRAPH_TOP && x - 1 < GRAPH_LEFT + GRAPH_WIDTH &&
           y + d < GRAPH_TOP + GRAPH_HEIGHT)
            canvas_draw_dot(canvas, x - 1, y + d);
        if(x + 1 >= GRAPH_LEFT && y + d >= GRAPH_TOP && x + 1 < GRAPH_LEFT + GRAPH_WIDTH &&
           y + d < GRAPH_TOP + GRAPH_HEIGHT)
            canvas_draw_dot(canvas, x + 1, y + d);
    }
    if(x >= GRAPH_LEFT && y - 1 >= GRAPH_TOP && x < GRAPH_LEFT + GRAPH_WIDTH &&
       y - 1 < GRAPH_TOP + GRAPH_HEIGHT)
        canvas_draw_dot(canvas, x, y - 1);
    if(x >= GRAPH_LEFT && y + 1 >= GRAPH_TOP && x < GRAPH_LEFT + GRAPH_WIDTH &&
       y + 1 < GRAPH_TOP + GRAPH_HEIGHT)
        canvas_draw_dot(canvas, x, y + 1);
}

// Draw cross marker (for comparison mode - third UID)
static void draw_marker_cross(Canvas* canvas, int x, int y) {
    canvas_draw_dot(canvas, x, y);
    for(int d = -1; d <= 1; d += 2) {
        if(x + d >= GRAPH_LEFT && x + d < GRAPH_LEFT + GRAPH_WIDTH)
            canvas_draw_dot(canvas, x + d, y);
        if(y + d >= GRAPH_TOP && y + d < GRAPH_TOP + GRAPH_HEIGHT)
            canvas_draw_dot(canvas, x, y + d);
    }
}

// Draw curve and markers for a set of temperature data
static void draw_data_series(
    Canvas* canvas,
    float* temps,
    uint8_t temp_count,
    float min_temp,
    float range,
    LineStyle style,
    uint8_t marker_type) {
    if(temp_count == 0) return;

    // Calculate x spacing
    float x_step = (temp_count > 1) ? (float)GRAPH_WIDTH / (temp_count - 1) : 0;

    // Convert temperatures to coordinates
    int y_coords[VK_THERMO_LOG_MAX_ENTRIES];
    int x_coords[VK_THERMO_LOG_MAX_ENTRIES];
    for(uint8_t i = 0; i < temp_count; i++) {
        float normalized = (temps[i] - min_temp) / range;
        y_coords[i] = GRAPH_TOP + GRAPH_HEIGHT - (int)(normalized * GRAPH_HEIGHT);
        x_coords[i] = GRAPH_LEFT + (int)(i * x_step);
    }

    // Draw curve
    if(temp_count == 1) {
        // Single point - just marker
    } else if(temp_count == 2) {
        draw_styled_line(canvas, x_coords[0], y_coords[0], x_coords[1], y_coords[1], style);
    } else {
        for(uint8_t i = 0; i < temp_count - 1; i++) {
            if(i == 0) {
                int mid_x = (x_coords[0] + x_coords[1]) / 2;
                int mid_y = (y_coords[0] + y_coords[1]) / 2;
                draw_styled_line(canvas, x_coords[0], y_coords[0], mid_x, mid_y, style);
            } else if(i == temp_count - 2) {
                int mid_x = (x_coords[i - 1] + x_coords[i]) / 2;
                int mid_y = (y_coords[i - 1] + y_coords[i]) / 2;
                draw_bezier_segment(
                    canvas,
                    mid_x,
                    mid_y,
                    x_coords[i],
                    y_coords[i],
                    x_coords[i + 1],
                    y_coords[i + 1],
                    style);
            } else {
                int mid1_x = (x_coords[i - 1] + x_coords[i]) / 2;
                int mid1_y = (y_coords[i - 1] + y_coords[i]) / 2;
                int mid2_x = (x_coords[i] + x_coords[i + 1]) / 2;
                int mid2_y = (y_coords[i] + y_coords[i + 1]) / 2;
                draw_bezier_segment(
                    canvas, mid1_x, mid1_y, x_coords[i], y_coords[i], mid2_x, mid2_y, style);
            }
        }
    }

    // Draw data point markers
    for(uint8_t i = 0; i < temp_count; i++) {
        switch(marker_type) {
        case 0:
            draw_marker(canvas, x_coords[i], y_coords[i]);
            break;
        case 1:
            draw_marker_hollow(canvas, x_coords[i], y_coords[i]);
            break;
        default:
            draw_marker_cross(canvas, x_coords[i], y_coords[i]);
            break;
        }
    }
}

static void vk_thermo_graph_view_draw(Canvas* canvas, VkThermoGraphViewModel* model) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(!model->log || model->log->count == 0 || model->uid_count == 0) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "No data");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, "Scan an Thermo first");
        elements_button_left(canvas, "Back");
        return;
    }

    // Header
    canvas_set_font(canvas, FontSecondary);

    if(model->comparison_mode) {
        // Comparison mode header
        char header_str[24];
        snprintf(header_str, sizeof(header_str), "Compare (%d)", model->uid_count);
        canvas_draw_str(canvas, 0, 8, header_str);
    } else {
        // Single mode header with UID selector
        canvas_draw_str(canvas, 0, 8, "Graph");

        if(model->uid_count > 0) {
            char uid_str[8];
            vk_thermo_uid_to_short_string(
                model->unique_uids[model->selected_uid_index], uid_str, sizeof(uid_str));

            char selector_str[24];
            if(model->uid_count > 1) {
                snprintf(
                    selector_str,
                    sizeof(selector_str),
                    "< %s %d/%d >",
                    uid_str,
                    model->selected_uid_index + 1,
                    model->uid_count);
            } else {
                snprintf(selector_str, sizeof(selector_str), "%s", uid_str);
            }
            canvas_draw_str_aligned(canvas, 127, 8, AlignRight, AlignBottom, selector_str);
        }
    }

    // Determine which UIDs to draw and find global min/max
    float global_min = 999.0f;
    float global_max = -999.0f;
    uint8_t start_uid = model->comparison_mode ? 0 : model->selected_uid_index;
    uint8_t end_uid = model->comparison_mode ? model->uid_count : (model->selected_uid_index + 1);
    bool has_data = false;

    // First pass: find global min/max across all UIDs to draw
    for(uint8_t u = start_uid; u < end_uid; u++) {
        float temps[VK_THERMO_LOG_MAX_ENTRIES];
        uint8_t count =
            get_entries_for_uid(model, model->unique_uids[u], temps, VK_THERMO_LOG_MAX_ENTRIES);
        for(uint8_t i = 0; i < count; i++) {
            if(temps[i] < global_min) global_min = temps[i];
            if(temps[i] > global_max) global_max = temps[i];
            has_data = true;
        }
    }

    if(!has_data) {
        canvas_draw_str_aligned(
            canvas, 64, 32, AlignCenter, AlignCenter, "No data for this Thermo");
        elements_button_left(canvas, "Back");
        return;
    }

    // Add padding to range
    float range = global_max - global_min;
    if(range < 1.0f) range = 1.0f;
    global_min -= range * 0.1f;
    global_max += range * 0.1f;
    range = global_max - global_min;

    // Draw Y-axis labels
    canvas_set_font(canvas, FontSecondary);
    char label[8];
    snprintf(label, sizeof(label), "%.1f", (double)global_max);
    canvas_draw_str(canvas, 0, GRAPH_TOP + 4, label);
    snprintf(label, sizeof(label), "%.1f", (double)global_min);
    canvas_draw_str(canvas, 0, GRAPH_TOP + GRAPH_HEIGHT, label);

    // Draw graph border
    canvas_draw_frame(canvas, GRAPH_LEFT - 1, GRAPH_TOP - 1, GRAPH_WIDTH + 2, GRAPH_HEIGHT + 2);

    // Draw data series
    LineStyle styles[] = {LineStyleSolid, LineStyleDashed, LineStyleDotted};
    for(uint8_t u = start_uid; u < end_uid; u++) {
        float temps[VK_THERMO_LOG_MAX_ENTRIES];
        uint8_t count =
            get_entries_for_uid(model, model->unique_uids[u], temps, VK_THERMO_LOG_MAX_ENTRIES);
        if(count == 0) continue;

        uint8_t style_idx = model->comparison_mode ? (u % 3) : 0;
        draw_data_series(canvas, temps, count, global_min, range, styles[style_idx], style_idx);
    }

    // Bottom buttons
    elements_button_left(canvas, "Back");
    if(model->uid_count > 1) {
        elements_button_center(canvas, model->comparison_mode ? "Single" : "Cmp");
    }
}

static bool vk_thermo_graph_view_input(InputEvent* event, void* context) {
    furi_assert(context);
    VkThermoGraphView* instance = context;

    if(event->type == InputTypeRelease) {
        switch(event->key) {
        case InputKeyBack:
        case InputKeyLeft:
            instance->callback(VkThermoCustomEventGraphBack, instance->context);
            return true;

        case InputKeyOk:
            // Toggle comparison mode (only if multiple UIDs)
            with_view_model(
                instance->view,
                VkThermoGraphViewModel * model,
                {
                    if(model->uid_count > 1) {
                        model->comparison_mode = !model->comparison_mode;
                    }
                },
                true);
            return true;

        case InputKeyRight:
            // Cycle to next Thermo (single mode only)
            with_view_model(
                instance->view,
                VkThermoGraphViewModel * model,
                {
                    if(!model->comparison_mode && model->uid_count > 1) {
                        model->selected_uid_index =
                            (model->selected_uid_index + 1) % model->uid_count;
                    }
                },
                true);
            return true;

        case InputKeyUp:
            with_view_model(
                instance->view,
                VkThermoGraphViewModel * model,
                {
                    if(!model->comparison_mode && model->uid_count > 1) {
                        model->selected_uid_index =
                            (model->selected_uid_index + model->uid_count - 1) % model->uid_count;
                    }
                },
                true);
            return true;

        case InputKeyDown:
            with_view_model(
                instance->view,
                VkThermoGraphViewModel * model,
                {
                    if(!model->comparison_mode && model->uid_count > 1) {
                        model->selected_uid_index =
                            (model->selected_uid_index + 1) % model->uid_count;
                    }
                },
                true);
            return true;

        default:
            break;
        }
    }
    return false;
}

VkThermoGraphView* vk_thermo_graph_view_alloc(void) {
    VkThermoGraphView* instance = malloc(sizeof(VkThermoGraphView));
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(VkThermoGraphViewModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, (ViewDrawCallback)vk_thermo_graph_view_draw);
    view_set_input_callback(instance->view, vk_thermo_graph_view_input);

    with_view_model(
        instance->view,
        VkThermoGraphViewModel * model,
        {
            model->log = NULL;
            model->temp_unit = VkThermoTempUnitCelsius;
            model->comparison_mode = false;
            model->uid_count = 0;
            model->selected_uid_index = 0;
        },
        true);

    return instance;
}

void vk_thermo_graph_view_free(VkThermoGraphView* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* vk_thermo_graph_view_get_view(VkThermoGraphView* instance) {
    furi_assert(instance);
    return instance->view;
}

void vk_thermo_graph_view_set_log(VkThermoGraphView* instance, VkThermoLog* log) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        VkThermoGraphViewModel * model,
        {
            model->log = log;
            model->comparison_mode = false;
            build_uid_list(model);
        },
        true);
}

void vk_thermo_graph_view_set_temp_unit(VkThermoGraphView* instance, uint32_t temp_unit) {
    furi_assert(instance);
    with_view_model(
        instance->view, VkThermoGraphViewModel * model, { model->temp_unit = temp_unit; }, true);
}

void vk_thermo_graph_view_cycle_thermo(VkThermoGraphView* instance, bool forward) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        VkThermoGraphViewModel * model,
        {
            if(model->uid_count > 1) {
                if(forward) {
                    model->selected_uid_index = (model->selected_uid_index + 1) % model->uid_count;
                } else {
                    model->selected_uid_index =
                        (model->selected_uid_index + model->uid_count - 1) % model->uid_count;
                }
            }
        },
        true);
}
