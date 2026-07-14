// Includes
#include <furi.h> // Furi OS core functionality
#include <gui/gui.h> // GUI system for drawing to the screen
#include <gui/elements.h> // GUI elements library for button hints and UI components
#include <input/input.h> // Input handling for button presses
#include <stdlib.h> // Standard library functions (malloc, calloc, etc.)
#include <string.h> // Memory and string manipulation functions
#include <math.h> // Math functions (sqrt, fmax, fmin)
#include <furi_hal.h> // Logging functionality
#include "mitzi_snowflake_icons.h"

// ===================================================================
// Constants
// ===================================================================
#define GRID_SIZE       16 // Grid is 16x16 logical hex cells
#define HEX_WIDTH       5 // Each hex cell is 5 pixels wide (flat-top)
#define HEX_HEIGHT      3 // Each hex cell is 3 pixels tall (flat-top)
#define SCREEN_OFFSET_X 48 // Draw on right side of screen
#define SCREEN_OFFSET_Y 0 // Start at top

#define TAG "Snowflake"

// Parameter limits
#define ALPHA_MIN  0.5f
#define ALPHA_MAX  5.0f
#define ALPHA_STEP 0.1f
#define ALPHA_INIT 1.0f // Initial alpha value

#define BETA_MIN  0.1f
#define BETA_MAX  0.9f
#define BETA_STEP 0.05f
#define BETA_INIT 0.5f // Initial beta value

#define GAMMA_MIN  0.001f
#define GAMMA_MAX  0.1f
#define GAMMA_STEP 0.005f
#define GAMMA_INIT 0.01f // Initial gamma value

// ===================================================================
// Parameter selection
// ===================================================================
typedef enum {
    PARAM_ALPHA,
    PARAM_BETA,
    PARAM_GAMMA,
    PARAM_COUNT
} ParamType;

// ===================================================================
// Application State Structure
// ===================================================================
typedef struct {
    float* s; // State values (water content)
    float* u; // Non-frozen diffusing water
    uint8_t* frozen; // Boolean: is cell frozen?
    int step;

    // Adjustable parameters
    float alpha; // Diffusion constant
    float beta; // Boundary vapor level
    float gamma; // Background vapor addition

    ParamType selected_param; // Which parameter is being adjusted
    uint32_t back_press_timer; // For detecting long press
} SnowflakeState;

// ===================================================================
// Function: Get array index
// ===================================================================
static inline int get_index(int x, int y) {
    return y * GRID_SIZE + x;
}

// ===================================================================
// Function: Map logical hex cell to screen center pixel
// Flat-top hexagons: odd columns are offset downward by HEX_HEIGHT/2
// ===================================================================
static void get_hex_center_pixel(int hex_x, int hex_y, int* pixel_x, int* pixel_y) {
    *pixel_x = SCREEN_OFFSET_X + hex_x * HEX_WIDTH;
    *pixel_y = SCREEN_OFFSET_Y + hex_y * HEX_HEIGHT;

    // Offset odd columns downward for hexagonal packing
    if(hex_x % 2 == 1) {
        *pixel_y += HEX_HEIGHT / 2;
    }
}

// ===================================================================
// Function: Fill hexagonal cell
// Draws a 5x3 flat-top hexagon pattern:
//   0,X,X,X,0
//   X,X,C,X,X
//   0,X,X,X,0
// Center pixel C is at row 1, col 2 (middle of pattern)
// ===================================================================
static void fill_hex_cell(Canvas* canvas, int center_px, int center_py, bool filled) {
    // Row 0: 0,X,X,X,0 (offset -1 from center)
    if(filled) {
        canvas_draw_dot(canvas, center_px - 1, center_py - 1);
        canvas_draw_dot(canvas, center_px, center_py - 1);
        canvas_draw_dot(canvas, center_px + 1, center_py - 1);
    }

    // Row 1: X,X,C,X,X (center row, offset 0)
    if(filled) {
        canvas_draw_dot(canvas, center_px - 2, center_py);
        canvas_draw_dot(canvas, center_px - 1, center_py);
        canvas_draw_dot(canvas, center_px, center_py); // C = center pixel
        canvas_draw_dot(canvas, center_px + 1, center_py);
        canvas_draw_dot(canvas, center_px + 2, center_py);
    } else {
        // Always draw center pixel even when not filled (for grid visualization)
        canvas_draw_dot(canvas, center_px, center_py);
    }

    // Row 2: 0,X,X,X,0 (offset +1 from center)
    if(filled) {
        canvas_draw_dot(canvas, center_px - 1, center_py + 1);
        canvas_draw_dot(canvas, center_px, center_py + 1);
        canvas_draw_dot(canvas, center_px + 1, center_py + 1);
    }
}

// ===================================================================
// Function: Get hexagonal neighbors for flat-top hexagons
// Using "odd-q" vertical layout (odd columns shifted down)
// ===================================================================
static void get_hex_neighbors(int x, int y, int neighbors_x[6], int neighbors_y[6]) {
    if(x % 2 == 0) {
        // Even columns
        neighbors_x[0] = x;
        neighbors_y[0] = y - 1; // N
        neighbors_x[1] = x + 1;
        neighbors_y[1] = y - 1; // NE
        neighbors_x[2] = x + 1;
        neighbors_y[2] = y; // SE
        neighbors_x[3] = x;
        neighbors_y[3] = y + 1; // S
        neighbors_x[4] = x - 1;
        neighbors_y[4] = y; // SW
        neighbors_x[5] = x - 1;
        neighbors_y[5] = y - 1; // NW
    } else {
        // Odd columns (offset down)
        neighbors_x[0] = x;
        neighbors_y[0] = y - 1; // N
        neighbors_x[1] = x + 1;
        neighbors_y[1] = y; // NE
        neighbors_x[2] = x + 1;
        neighbors_y[2] = y + 1; // SE
        neighbors_x[3] = x;
        neighbors_y[3] = y + 1; // S
        neighbors_x[4] = x - 1;
        neighbors_y[4] = y + 1; // SW
        neighbors_x[5] = x - 1;
        neighbors_y[5] = y; // NW
    }
}

// ===================================================================
// Function: Check if cell is boundary cell
// ===================================================================
static bool is_boundary_cell(SnowflakeState* state, int x, int y) {
    if(state->frozen[get_index(x, y)]) return false;

    // Border cells (2 cells from edge) can never be boundary cells
    if(x < 2 || x >= GRID_SIZE - 2 || y < 2 || y >= GRID_SIZE - 2) return false;

    int neighbors_x[6], neighbors_y[6];
    get_hex_neighbors(x, y, neighbors_x, neighbors_y);

    for(int i = 0; i < 6; i++) {
        int nx = neighbors_x[i];
        int ny = neighbors_y[i];

        if(nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE) {
            if(state->frozen[get_index(nx, ny)]) {
                return true;
            }
        }
    }

    return false;
}

// ===================================================================
// Function: Initialize Snowflake
// ===================================================================
static void init_snowflake(SnowflakeState* state) {
    FURI_LOG_I(TAG, "Initializing snowflake");

    // Initialize all cells
    for(int i = 0; i < GRID_SIZE * GRID_SIZE; i++) {
        state->s[i] = state->beta;
        state->u[i] = 0.0f;
        state->frozen[i] = 0;
    }

    // Freeze center cell
    int center = GRID_SIZE / 2;
    int center_idx = get_index(center, center);
    state->s[center_idx] = 1.0f;
    state->frozen[center_idx] = 1;

    state->step = 0;
    FURI_LOG_I(
        TAG,
        "Initialized with α=%f β=%f γ=%f",
        (double)state->alpha,
        (double)state->beta,
        (double)state->gamma);
}

// ===================================================================
// Function: Grow Snowflake (Reiter's model)
// ===================================================================
static void grow_snowflake(SnowflakeState* state) {
    float* u_new = (float*)malloc(GRID_SIZE * GRID_SIZE * sizeof(float));
    if(!u_new) return;

    // Step 1: Classify cells and set u values
    for(int y = 0; y < GRID_SIZE; y++) {
        for(int x = 0; x < GRID_SIZE; x++) {
            int idx = get_index(x, y);
            bool is_receptive = state->frozen[idx] || is_boundary_cell(state, x, y);

            if(is_receptive) {
                state->u[idx] = 0.0f;
            } else {
                state->u[idx] = state->s[idx];
            }
        }
    }

    // Step 2: Diffusion
    for(int y = 0; y < GRID_SIZE; y++) {
        for(int x = 0; x < GRID_SIZE; x++) {
            int idx = get_index(x, y);

            // Border cells (2 from edge) maintain beta level
            if(x < 2 || x >= GRID_SIZE - 2 || y < 2 || y >= GRID_SIZE - 2) {
                u_new[idx] = state->beta;
                continue;
            }

            // Get hex neighbors with proper offset
            int neighbors_x[6], neighbors_y[6];
            get_hex_neighbors(x, y, neighbors_x, neighbors_y);

            float sum = 0.0f;
            int count = 0;

            for(int i = 0; i < 6; i++) {
                int nx = neighbors_x[i];
                int ny = neighbors_y[i];

                if(nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE) {
                    sum += state->u[get_index(nx, ny)];
                    count++;
                }
            }

            float avg = (count > 0) ? (sum / count) : state->u[idx];
            u_new[idx] = state->u[idx] + (state->alpha / 2.0f) * (avg - state->u[idx]);
        }
    }

    memcpy(state->u, u_new, GRID_SIZE * GRID_SIZE * sizeof(float));
    free(u_new);

    // Step 3: Add background vapor and update s = u + (v + gamma)
    // Use two-phase update to avoid directional bias
    float* s_new = (float*)malloc(GRID_SIZE * GRID_SIZE * sizeof(float));
    uint8_t* frozen_new = (uint8_t*)malloc(GRID_SIZE * GRID_SIZE * sizeof(uint8_t));
    if(!s_new || !frozen_new) {
        if(s_new) free(s_new);
        if(frozen_new) free(frozen_new);
        return;
    }

    // Copy current frozen state
    memcpy(frozen_new, state->frozen, GRID_SIZE * GRID_SIZE * sizeof(uint8_t));

    int frozen_count = 0;

    // Phase 1: Calculate new s values and determine which cells should freeze
    // using the CURRENT (unchanged) frozen state
    for(int y = 0; y < GRID_SIZE; y++) {
        for(int x = 0; x < GRID_SIZE; x++) {
            int idx = get_index(x, y);

            // Border cells (2 from edge): always maintain beta, never freeze
            if(x < 2 || x >= GRID_SIZE - 2 || y < 2 || y >= GRID_SIZE - 2) {
                s_new[idx] = state->beta;
                frozen_new[idx] = 0;
                continue;
            }

            // Use OLD frozen state for receptiveness check
            bool is_receptive = state->frozen[idx] || is_boundary_cell(state, x, y);

            if(is_receptive) {
                // Receptive: s_new = u_new + (s_old + gamma)
                s_new[idx] = state->u[idx] + state->s[idx] + state->gamma;

                // Mark for freezing if threshold reached
                if(!state->frozen[idx] && s_new[idx] >= 1.0f) {
                    frozen_new[idx] = 1;
                    frozen_count++;
                }
            } else {
                // Non-receptive: s = u (v=0 for non-receptive)
                s_new[idx] = state->u[idx];
            }
        }
    }

    // Phase 2: Commit all changes atomically
    memcpy(state->s, s_new, GRID_SIZE * GRID_SIZE * sizeof(float));
    memcpy(state->frozen, frozen_new, GRID_SIZE * GRID_SIZE * sizeof(uint8_t));
    free(s_new);
    free(frozen_new);

    state->step++;
    FURI_LOG_I(TAG, "Step %d: froze %d cells", state->step, frozen_count);
}

// ===================================================================
// Function: Draw Callback
// ===================================================================
static void snowflake_draw_callback(Canvas* canvas, void* ctx) {
    SnowflakeState* state = (SnowflakeState*)ctx;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Draw header with icon and title
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
    canvas_draw_str_aligned(canvas, 13, 1, AlignLeft, AlignTop, "Snowflake");
    canvas_set_font(canvas, FontSecondary);

    // Draw parameter info on left side
    char alpha_str[32];
    snprintf(
        alpha_str,
        sizeof(alpha_str),
        "%s alpha:%.1f",
        (state->selected_param == PARAM_ALPHA) ? ">" : " ",
        (double)state->alpha);
    canvas_draw_str(canvas, 2, 18, alpha_str);

    char beta_str[32];
    snprintf(
        beta_str,
        sizeof(beta_str),
        "%s beta:%.2f",
        (state->selected_param == PARAM_BETA) ? ">" : " ",
        (double)state->beta);
    canvas_draw_str(canvas, 2, 27, beta_str);

    char gamma_str[32];
    snprintf(
        gamma_str,
        sizeof(gamma_str),
        "%s gam:%.3f",
        (state->selected_param == PARAM_GAMMA) ? ">" : " ",
        (double)state->gamma);
    canvas_draw_str(canvas, 2, 36, gamma_str);

    // Count frozen cells
    int frozen_total = 0;
    for(int i = 0; i < GRID_SIZE * GRID_SIZE; i++) {
        if(state->frozen[i]) frozen_total++;
    }

    // Draw step counter
    char buffer[42];
    snprintf(buffer, sizeof(buffer), "Step %d: %d frozen", state->step, frozen_total);
    canvas_draw_str(canvas, 2, 50, buffer);

    // Draw all hexagonal cells
    for(int y = 0; y < GRID_SIZE; y++) {
        for(int x = 0; x < GRID_SIZE; x++) {
            int px, py;
            get_hex_center_pixel(x, y, &px, &py);

            // Check bounds
            if(px >= 48 && px < 128 && py >= 0 && py < 64) {
                bool is_frozen = state->frozen[get_index(x, y)];
                fill_hex_cell(canvas, px, py, is_frozen);
            }
        }
    }

    // Draw UI hints
    canvas_draw_icon(canvas, 1, 55, &I_back);
    canvas_draw_str_aligned(canvas, 11, 62, AlignLeft, AlignBottom, "Hold: Exit");
    elements_button_center(canvas, "OK");
}

// ===================================================================
// Function: Input Callback
// ===================================================================
static void snowflake_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

// ===================================================================
// Function: Main Application Entry Point
// ===================================================================
int32_t snowflake_main(void* p) {
    UNUSED(p);

    FURI_LOG_I(TAG, "Snowflake application starting");

    // Allocate state
    SnowflakeState* state = malloc(sizeof(SnowflakeState));
    if(!state) return -1;

    state->s = (float*)malloc(GRID_SIZE * GRID_SIZE * sizeof(float));
    state->u = (float*)malloc(GRID_SIZE * GRID_SIZE * sizeof(float));
    state->frozen = (uint8_t*)malloc(GRID_SIZE * GRID_SIZE * sizeof(uint8_t));

    if(!state->s || !state->u || !state->frozen) {
        if(state->s) free(state->s);
        if(state->u) free(state->u);
        if(state->frozen) free(state->frozen);
        free(state);
        return -1;
    }

    // Initialize default parameters
    state->alpha = ALPHA_INIT;
    state->beta = BETA_INIT;
    state->gamma = GAMMA_INIT;
    state->selected_param = PARAM_ALPHA;
    state->back_press_timer = 0;

    init_snowflake(state);

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    if(!event_queue) {
        free(state->s);
        free(state->u);
        free(state->frozen);
        free(state);
        return -1;
    }

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, snowflake_draw_callback, state);
    view_port_input_callback_set(view_port, snowflake_input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InputEvent event;
    bool running = true;

    while(running) {
        if(furi_message_queue_get(event_queue, &event, 100) == FuriStatusOk) {
            if(event.key == InputKeyBack) {
                if(event.type == InputTypePress) {
                    state->back_press_timer = furi_get_tick();
                } else if(event.type == InputTypeRelease) {
                    uint32_t press_duration = furi_get_tick() - state->back_press_timer;
                    if(press_duration > 500) {
                        // Long press - exit
                        FURI_LOG_I(TAG, "Long press - exiting");
                        running = false;
                    } else {
                        // Short press - reset
                        FURI_LOG_I(TAG, "Short press - reset");
                        init_snowflake(state);
                        view_port_update(view_port);
                    }
                }
            } else if(event.type == InputTypePress || event.type == InputTypeRepeat) {
                if(event.key == InputKeyOk) {
                    grow_snowflake(state);
                    view_port_update(view_port);
                } else if(event.key == InputKeyUp) {
                    // Previous parameter
                    state->selected_param =
                        (state->selected_param + PARAM_COUNT - 1) % PARAM_COUNT;
                    view_port_update(view_port);
                } else if(event.key == InputKeyDown) {
                    // Next parameter
                    state->selected_param = (state->selected_param + 1) % PARAM_COUNT;
                    view_port_update(view_port);
                } else if(event.key == InputKeyRight) {
                    // Increase parameter
                    if(state->selected_param == PARAM_ALPHA) {
                        state->alpha = fminf(state->alpha + ALPHA_STEP, ALPHA_MAX);
                    } else if(state->selected_param == PARAM_BETA) {
                        state->beta = fminf(state->beta + BETA_STEP, BETA_MAX);
                    } else if(state->selected_param == PARAM_GAMMA) {
                        state->gamma = fminf(state->gamma + GAMMA_STEP, GAMMA_MAX);
                    }
                    view_port_update(view_port);
                } else if(event.key == InputKeyLeft) {
                    // Decrease parameter
                    if(state->selected_param == PARAM_ALPHA) {
                        state->alpha = fmaxf(state->alpha - ALPHA_STEP, ALPHA_MIN);
                    } else if(state->selected_param == PARAM_BETA) {
                        state->beta = fmaxf(state->beta - BETA_STEP, BETA_MIN);
                    } else if(state->selected_param == PARAM_GAMMA) {
                        state->gamma = fmaxf(state->gamma - GAMMA_STEP, GAMMA_MIN);
                    }
                    view_port_update(view_port);
                }
            }
        }
    }

    // Cleanup
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    free(state->s);
    free(state->u);
    free(state->frozen);
    free(state);

    FURI_LOG_I(TAG, "Terminated");
    return 0;
}
