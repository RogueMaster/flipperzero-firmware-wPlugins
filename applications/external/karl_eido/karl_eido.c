/**
 * Karl Eido's Scope 
 * - Up/Down: Adjust triangle size (5-63px in 2px steps)
 * - OK (short press): Toggle debug info and center points
 * - OK (long press): Toggle line display
 * - Back: Exit app
 */

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>

// Screen dimensions
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

// Triangle size constraints
#define MIN_SIDE_LENGTH  10
#define MAX_SIDE_LENGTH  63
#define SIDE_LENGTH_STEP 2
#define CENTER_Y         31

// Point structure for coordinates
typedef struct {
    int x;
    int y;
} Point;

// Application state
typedef struct {
    int side_length;
    bool show_lines;
    bool show_info;
    bool running;
} AppState;

/**
 * Calculate the height of an equilateral triangle given its side length
 * Height = side_length * sqrt(3)/2
 */
static float triangle_height(int side_length) {
    return (float)side_length * 0.866025404f; // sqrt(3)/2
}

/**
 * Calculate triangle vertices based on grid position and orientation
 * 
 * @param vertices Output array for 3 vertices
 * @param col Column in the grid
 * @param row Row in the grid
 * @param side_length Side length of the triangle
 * @param pointing_right True if triangle points right, false if points left
 */
static void
    get_triangle_vertices(Point* vertices, int col, int row, int side_length, bool pointing_right) {
    float h = triangle_height(side_length);
    int base_x = (int)(col * h);
    int base_y = CENTER_Y + (row * side_length / 2);

    if(pointing_right) {
        // Triangle pointing right: |>
        vertices[0].x = base_x;
        vertices[0].y = base_y - side_length / 2;
        vertices[1].x = base_x;
        vertices[1].y = base_y + side_length / 2;
        vertices[2].x = base_x + (int)h;
        vertices[2].y = base_y;
    } else {
        // Triangle pointing left: <|
        vertices[0].x = base_x;
        vertices[0].y = base_y;
        vertices[1].x = base_x + (int)h;
        vertices[1].y = base_y - side_length / 2;
        vertices[2].x = base_x + (int)h;
        vertices[2].y = base_y + side_length / 2;
    }
}

/**
 * Check if a triangle is fully visible on screen (all vertices inside bounds)
 */
static bool is_triangle_fully_visible(Point* vertices) {
    for(int i = 0; i < 3; i++) {
        if(vertices[i].x < 0 || vertices[i].x >= SCREEN_WIDTH || vertices[i].y < 0 ||
           vertices[i].y >= SCREEN_HEIGHT) {
            return false;
        }
    }
    return true;
}

/**
 * Check if a triangle is at least partially visible on screen
 */
static bool is_triangle_visible(Point* vertices) {
    // Check if completely outside horizontal bounds
    if(vertices[0].x >= SCREEN_WIDTH && vertices[1].x >= SCREEN_WIDTH &&
       vertices[2].x >= SCREEN_WIDTH)
        return false;
    if(vertices[0].x < 0 && vertices[1].x < 0 && vertices[2].x < 0) return false;

    // Check if completely outside vertical bounds
    if(vertices[0].y >= SCREEN_HEIGHT && vertices[1].y >= SCREEN_HEIGHT &&
       vertices[2].y >= SCREEN_HEIGHT)
        return false;
    if(vertices[0].y < 0 && vertices[1].y < 0 && vertices[2].y < 0) return false;

    return true;
}

/**
 * Calculate the area of a triangle using the shoelace formula
 */
static int calculate_triangle_area(Point* vertices) {
    int area =
        abs((vertices[1].x - vertices[0].x) * (vertices[2].y - vertices[0].y) -
            (vertices[2].x - vertices[0].x) * (vertices[1].y - vertices[0].y));
    return area / 2;
}

/**
 * Calculate the center (centroid) of a triangle
 */
static Point get_triangle_center(Point* vertices) {
    Point center;
    center.x = (vertices[0].x + vertices[1].x + vertices[2].x) / 3;
    center.y = (vertices[0].y + vertices[1].y + vertices[2].y) / 3;
    return center;
}

/**
 * Draw the pattern
 */
static void draw_pattern(Canvas* canvas, AppState* state) {
    if(state == NULL || canvas == NULL) return;
    if(state->side_length < MIN_SIDE_LENGTH) return;

    float h = triangle_height(state->side_length);

    // Calculate grid dimensions
    int num_cols = (int)(SCREEN_WIDTH / h) + 2;
    int num_rows = (int)(SCREEN_HEIGHT / (state->side_length / 2.0f)) + 2;

    int full_triangles = 0;
    int partial_triangles = 0;
    int triangle_area = 0;
    int line_count = 0;

    // Draw lines by type to avoid duplicates
    if(state->show_lines) {
        // Draw all vertical lines (left edges of right-pointing triangles)
        for(int col = 0; col <= num_cols; col++) {
            int x = (int)(col * h);
            if(x >= 0 && x < SCREEN_WIDTH) {
                canvas_draw_line(canvas, x, 0, x, SCREEN_HEIGHT - 1);
                line_count++;
            }
        }

        // Draw diagonal lines (only from right-pointing triangles to avoid duplicates)
        for(int col = 0; col < num_cols; col++) {
            for(int row = -num_rows; row < num_rows; row++) {
                bool pointing_right = ((col + row) % 2 == 0);
                if(!pointing_right) continue; // Only process right-pointing triangles

                Point vertices[3];
                get_triangle_vertices(vertices, col, row, state->side_length, pointing_right);

                if(!is_triangle_visible(vertices)) continue;

                // Draw the two diagonal edges of the right-pointing triangle
                // Upper diagonal: from top vertex to right apex
                canvas_draw_line(
                    canvas, vertices[0].x, vertices[0].y, vertices[2].x, vertices[2].y);
                line_count++;

                // Lower diagonal: from bottom vertex to right apex
                canvas_draw_line(
                    canvas, vertices[1].x, vertices[1].y, vertices[2].x, vertices[2].y);
                line_count++;
            }
        }
    }

    // Count triangles and draw centers
    for(int col = 0; col < num_cols; col++) {
        for(int row = -num_rows; row < num_rows; row++) {
            bool pointing_right = ((col + row) % 2 == 0);

            Point vertices[3];
            get_triangle_vertices(vertices, col, row, state->side_length, pointing_right);

            if(!is_triangle_visible(vertices)) continue;

            // Classify triangle as full or partial
            if(is_triangle_fully_visible(vertices)) {
                full_triangles++;
            } else {
                partial_triangles++;
            }

            // Calculate area (only once, all triangles are same size)
            if(triangle_area == 0) {
                triangle_area = calculate_triangle_area(vertices);
            }

            // Draw center point if enabled
            if(state->show_info) {
                Point center = get_triangle_center(vertices);
                if(center.x >= 0 && center.x < SCREEN_WIDTH && center.y >= 0 &&
                   center.y < SCREEN_HEIGHT) {
                    canvas_draw_disc(canvas, center.x, center.y, 1);
                }
            }
        }
    }

    // Draw debug info if enabled
    if(state->show_info) {
        char debug_str[64];
        snprintf(
            debug_str,
            sizeof(debug_str),
            "a:%d L:%d T:%d(+%d) A:%dpx",
            state->side_length,
            line_count,
            full_triangles,
            partial_triangles,
            triangle_area);

        // Draw white background for text
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 0, 0, SCREEN_WIDTH, 10);

        // Draw text in black
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(canvas, 2, 8, debug_str);
    }
}

/**
 * Canvas render callback
 */
static void render_callback(Canvas* canvas, void* ctx) {
    AppState* state = (AppState*)ctx;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    draw_pattern(canvas, state);
}

/**
 * Input event callback
 */
static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

/**
 * Handle input events and update application state
 */
static bool handle_input(InputEvent* event, AppState* state) {
    if(event->type != InputTypePress && event->type != InputTypeRepeat &&
       event->type != InputTypeLong) {
        return false;
    }

    bool state_changed = false;

    switch(event->key) {
    case InputKeyUp:
        if(event->type == InputTypePress || event->type == InputTypeRepeat) {
            if(state->side_length < MAX_SIDE_LENGTH) {
                state->side_length += SIDE_LENGTH_STEP;
                state_changed = true;
            }
        }
        break;

    case InputKeyDown:
        if(event->type == InputTypePress || event->type == InputTypeRepeat) {
            if(state->side_length > MIN_SIDE_LENGTH) {
                state->side_length -= SIDE_LENGTH_STEP;
                state_changed = true;
            }
        }
        break;

    case InputKeyOk:
        if(event->type == InputTypeLong) {
            // Toggle line display
            state->show_lines = !state->show_lines;
            state_changed = true;
        } else if(event->type == InputTypePress) {
            // Toggle debug info and center points together
            state->show_info = !state->show_info;
            state_changed = true;
        }
        break;

    case InputKeyBack:
        state->running = false;
        break;

    default:
        break;
    }

    return state_changed;
}

/**
 * Main application entry point
 * Entry point name must match application.fam: entry_point="karl_main"
 */
int32_t karl_main(void* p) {
    UNUSED(p);

    // Initialize application state
    AppState* state = malloc(sizeof(AppState));
    if(state == NULL) {
        return -1; // Failed to allocate memory
    }

    state->side_length = MAX_SIDE_LENGTH;
    state->show_lines = true;
    state->show_info = false;
    state->running = true;

    // Create event queue
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    if(event_queue == NULL) {
        free(state);
        return -1;
    }

    // Set up viewport
    ViewPort* view_port = view_port_alloc();
    if(view_port == NULL) {
        furi_message_queue_free(event_queue);
        free(state);
        return -1;
    }

    view_port_draw_callback_set(view_port, render_callback, state);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    // Register viewport with GUI
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // Main event loop
    InputEvent event;
    while(state->running) {
        if(furi_message_queue_get(event_queue, &event, 100) == FuriStatusOk) {
            if(handle_input(&event, state)) {
                view_port_update(view_port);
            }
        }
    }

    // Cleanup
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_record_close(RECORD_GUI);
    free(state);

    return 0;
}
