#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <stdlib.h>

// Include generated icon assets
#include "panis_icons.h"

// Screen dimensions for Flipper Zero
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

// PANIS configuration
#define MOVEMENT_SPEED 4 // Pixels per frame
#define GROUND_Y       59 // Y position of the ground line

// Jump physics
#define GRAVITY               2
#define SMALL_JUMP_VELOCITY   -10
#define BIG_JUMP_VELOCITY     -20
#define MAX_FALL_SPEED        10
#define MAX_JUMP_HEIGHT       60 // Maximum pixels above ground
#define JUMP_HEIGHT_THRESHOLD 25 // Height to distinguish small/big jump
#define DOUBLE_CLICK_MS       900 // Time window for double-click (milliseconds)

// Map tile configuration
#define TILE_WIDTH      128
#define NUM_TILES       3 // map_tile_0, map_tile_1, map_tile_2
#define TOTAL_MAP_WIDTH (TILE_WIDTH * NUM_TILES)

// Character dimensions
#define CHAR__WIDTH 10
#define CHAR_HEIGHT 10

// Scrolling thresholds
#define START_SCROLL_X (SCREEN_WIDTH / 2) // Start scrolling at 1/2 screen (64px)
#define CHAR_START_X   (SCREEN_WIDTH / 4) // Character starts at 1/4 screen (32px)

// Grid configuration
#define CELL_SIZE           10
#define GRID_ROWS           6
#define GRID_COLS           39 // (128*3)/10 = 38.4, so 39 columns
#define CELL_EMPTY          0
#define CELL_BLOCK          1
#define CELL_PILL           2
#define CELL_DIAMOND        3
#define CELL_DIAMOND_FILLED 4
#define CELL_CLOUD          5

// Grid generation percentages (as decimals)
#define SKY_ROW_THRESHOLD     2 // Rows 0-1 are "sky" rows
#define PERCENT_PILLS         0.02 // 2% pills
#define PERCENT_AIR_BLOCKS    0.005 // 0.5% random air blocks
#define PERCENT_GROUND_BLOCKS 0.02 // ~2% ground/stacked blocks
#define PERCENT_CLOUDS        0.15 // 15% clouds in sky rows

// Game state structure
typedef struct {
    int world_x; // Character's X position in the world (0 to TOTAL_MAP_WIDTH)
    int screen_x; // Character's X position on screen
    int camera_x; // Camera offset (how much the world is scrolled)
    bool facing_right; // True if facing right, false if facing left
    bool running; // Game loop control
    int y_pos; // Character Y position (0 = top)
    int y_velocity; // Vertical velocity
    bool on_ground; // True if character is on ground
    uint32_t last_jump_time; // Time of last jump press
    NotificationApp* notifications; // For vibration feedback
    uint8_t grid[GRID_ROWS][GRID_COLS]; // Collision grid
    int score; // Collected pills score
    int block_count; // Number of blocks in grid
    int pill_count; // Number of pills remaining
    int overall_pills; // Total pills placed
    int overall_diamonds; // Total diamonds placed
    int filled_diamonds; // Number of filled diamonds
    int ground_blocks; // Number of blocks on/near ground
    bool grid_view_enabled; // True when down button is held
    FuriThread* melody_thread; // Thread for playing melody
} GameState;

// Helper function for vibration feedback
static void trigger_vibration(GameState* state) {
    notification_message(state->notifications, &sequence_single_vibro);
}

// Helper function for melody 'Little things'
void play_melody(void) {
    // Melody: frequency (Hz), duration (ms)
    typedef struct {
        float freq;
        uint32_t duration;
    } Note;
    // Acquire speaker before use
    if(!furi_hal_speaker_acquire(1000)) { // Failed to acquire speaker, exit
        return;
    }

    Note melody[] = {
        // Measure 1: E4, C5, E5, C5, E5
        {329.63f, 454}, // E4 quarter
        {523.25f, 227}, // C5 eighth
        {659.25f, 227}, // E5 eighth
        {523.25f, 227}, // C5 eighth
        {659.25f, 227}, // E5 eighth

        // Measure 2: B4 (dotted half)
        {493.88f, 1364}, // B4 dotted half

        // Measure 3: B4, C5, E5, C5, E5
        {493.88f, 454}, // B4 quarter
        {523.25f, 227}, // C5 eighth
        {659.25f, 227}, // E5 eighth
        {523.25f, 227}, // C5 eighth
        {659.25f, 227}, // E5 eighth

        // Measure 4: B4 (dotted half)
        {493.88f, 1364} // B4 dotted half
    };

    for(size_t i = 0; i < COUNT_OF(melody); i++) {
        furi_hal_speaker_start(melody[i].freq, 1.0f);
        furi_delay_ms(melody[i].duration * 0.9);

        furi_hal_speaker_stop();
        furi_delay_ms(melody[i].duration * 0.1);
    }

    furi_hal_speaker_stop();
    furi_hal_speaker_release();
}

// Thread function to play melody without blocking
static int32_t melody_thread(void* context) {
    UNUSED(context);
    play_melody();
    return 0;
}

// Helper function to start melody in background thread, and clean up automatically
static void play_melody_async(GameState* state) {
    // If a melody is already playing, don't start another
    if(state->melody_thread != NULL) {
        if(furi_thread_get_state(state->melody_thread) != FuriThreadStateStopped) {
            return; // Thread still running
        }
        // Clean up old thread
        furi_thread_free(state->melody_thread);
        state->melody_thread = NULL;
    }
    // Create and start new thread
    state->melody_thread = furi_thread_alloc();
    furi_thread_set_name(state->melody_thread, "MelodyThread");
    furi_thread_set_stack_size(state->melody_thread, 2048);
    furi_thread_set_callback(state->melody_thread, melody_thread);
    furi_thread_start(state->melody_thread);
}

// Initialize the collision grid
static void init_grid(GameState* state) {
    // Clear grid
    for(int row = 0; row < GRID_ROWS; row++) {
        for(int col = 0; col < GRID_COLS; col++) {
            state->grid[row][col] = CELL_EMPTY;
        }
    }

    int total_cells = GRID_ROWS * GRID_COLS;
    int num_pills = (int)(total_cells * PERCENT_PILLS);
    int num_air_blocks = (int)(total_cells * PERCENT_AIR_BLOCKS);
    int num_ground_blocks = (int)(total_cells * PERCENT_GROUND_BLOCKS);

    state->score = 0;
    state->block_count = 0;
    state->pill_count = 0;
    state->overall_pills = 0;
    state->overall_diamonds = 0;
    state->filled_diamonds = 0;
    state->ground_blocks = 0;

    // Place clouds in sky rows only (rows 0-1)
    int sky_cells = SKY_ROW_THRESHOLD * GRID_COLS;
    int num_clouds = (int)(sky_cells * PERCENT_CLOUDS);

    int clouds_placed = 0;
    int cloud_attempts = 0;
    while(clouds_placed < num_clouds && cloud_attempts < sky_cells * 2) {
        int row = rand() % SKY_ROW_THRESHOLD; // Only rows 0-1
        int col = rand() % GRID_COLS;
        if(state->grid[row][col] == CELL_EMPTY) {
            state->grid[row][col] = CELL_CLOUD;
            clouds_placed++;
        }
        cloud_attempts++;
    }

    // Place air blocks (random positions in rows 0-4)
    for(int i = 0; i < num_air_blocks; i++) {
        int row = rand() % 5; // Rows 0-4 (not on ground)
        int col = rand() % GRID_COLS;
        if(state->grid[row][col] == CELL_EMPTY) {
            state->grid[row][col] = CELL_BLOCK;
            state->block_count++;
        }
    }

    // Place ground blocks (on row 5 or stacked)
    for(int i = 0; i < num_ground_blocks; i++) {
        int col = rand() % GRID_COLS;
        // Find lowest empty cell in this column
        for(int row = GRID_ROWS - 1; row >= 0; row--) {
            if(state->grid[row][col] == CELL_EMPTY) {
                state->grid[row][col] = CELL_BLOCK;
                state->block_count++;
                state->ground_blocks++;
                break;
            }
        }
    }

    // Place pills in empty cells
    int pills_placed = 0;
    int attempts = 0;
    while(pills_placed < num_pills && attempts < total_cells * 2) {
        int row = rand() % GRID_ROWS;
        int col = rand() % GRID_COLS;
        if(state->grid[row][col] == CELL_EMPTY) {
            state->grid[row][col] = CELL_PILL;
            state->pill_count++;
            state->overall_pills++;
            pills_placed++;
        }
        attempts++;
    }

    // Create bridge: columns 13-17, rows 3 (two above ground)
    // First clear any blocks underneath the bridge
    for(int col = 13; col <= 17; col++) {
        for(int row = 4; row <= 5; row++) {
            if(state->grid[row][col] == CELL_BLOCK) {
                state->block_count--;
                state->ground_blocks--;
            }
            state->grid[row][col] = CELL_EMPTY;
        }
    }
    // Place bridge blocks
    for(int col = 13; col <= 17; col++) {
        state->grid[3][col] = CELL_BLOCK;
        state->block_count += 2;
        // Place diamonds on top of bridge
        state->grid[2][col] = CELL_DIAMOND_FILLED;
        state->overall_diamonds++;
    }

    // Place diamonds below pills in sky rows (0-1)
    for(int row = 0; row < SKY_ROW_THRESHOLD; row++) {
        for(int col = 0; col < GRID_COLS; col++) {
            if(state->grid[row][col] == CELL_PILL) {
                // Place diamond in cell below if empty
                int below_row = row + 1;
                if(below_row < GRID_ROWS && state->grid[below_row][col] == CELL_EMPTY) {
                    state->grid[below_row][col] = CELL_DIAMOND_FILLED;
                    state->overall_diamonds++;
                }
            }
        }
    }
}

// Get grid cell at world position
static uint8_t get_cell_at(GameState* state, int world_x, int world_y) {
    int col = world_x / CELL_SIZE;
    int row = world_y / CELL_SIZE;

    if(row < 0 || row >= GRID_ROWS || col < 0 || col >= GRID_COLS) {
        return CELL_EMPTY;
    }

    // Clouds don't affect collision
    return (state->grid[row][col] == CELL_CLOUD) ? CELL_EMPTY : state->grid[row][col];
}

// Check if character collides with a block at given position
static bool check_block_collision(GameState* state, int world_x, int y_pos) {
    // Check all four corners of character
    int left = world_x;
    int right = world_x + CHAR__WIDTH - 1;
    int top = y_pos;
    int bottom = y_pos + CHAR_HEIGHT - 1;

    // Check corners
    if(get_cell_at(state, left, top) == CELL_BLOCK) return true;
    if(get_cell_at(state, right, top) == CELL_BLOCK) return true;
    if(get_cell_at(state, left, bottom) == CELL_BLOCK) return true;
    if(get_cell_at(state, right, bottom) == CELL_BLOCK) return true;

    return false;
}

// Collect pills at character position
static void collect_pills(GameState* state) {
    int left = state->world_x;
    int right = state->world_x + CHAR__WIDTH - 1;
    int top = state->y_pos;
    int bottom = state->y_pos + CHAR_HEIGHT - 1;

    // Check cells that character overlaps
    int col_start = left / CELL_SIZE;
    int col_end = right / CELL_SIZE;
    int row_start = top / CELL_SIZE;
    int row_end = bottom / CELL_SIZE;

    for(int row = row_start; row <= row_end && row < GRID_ROWS; row++) {
        for(int col = col_start; col <= col_end && col < GRID_COLS; col++) {
            if(row >= 0 && col >= 0 && state->grid[row][col] == CELL_PILL) {
                state->grid[row][col] = CELL_EMPTY;
                state->score += 10;
                state->pill_count--;
            }
        }
    }
    // Empty diamonds when jumping through them
    for(int row = row_start; row <= row_end && row < GRID_ROWS; row++) {
        for(int col = col_start; col <= col_end && col < GRID_COLS; col++) {
            if(row >= 0 && col >= 0 && state->grid[row][col] == CELL_DIAMOND_FILLED) {
                if(!state->on_ground) { // Only fill when jumping/falling
                    state->grid[row][col] = CELL_DIAMOND;
                    state->filled_diamonds++;
                }
            }
        }
    }
}

// Check if character can stand on a block
static bool check_ground_support(GameState* state, int world_x, int y_pos) {
    // Check one pixel below character's feet
    int feet_y = y_pos + CHAR_HEIGHT;
    int left = world_x;
    int right = world_x + CHAR__WIDTH - 1;

    if(get_cell_at(state, left, feet_y) == CELL_BLOCK) return true;
    if(get_cell_at(state, right, feet_y) == CELL_BLOCK) return true;

    return false;
}

// Draw the grid overlay when enabled
static void draw_grid_overlay(Canvas* canvas, GameState* state) {
    // Set font for labels
    canvas_set_font(canvas, FontSecondary);

    // Calculate which columns are visible on screen
    int first_col = state->camera_x / CELL_SIZE;
    int last_col = (state->camera_x + SCREEN_WIDTH) / CELL_SIZE + 1;

    // Clamp to grid boundaries
    if(first_col < 0) first_col = 0;
    if(last_col >= GRID_COLS) last_col = GRID_COLS;

    // Draw vertical lines for each column
    for(int col = first_col; col <= last_col; col++) {
        int world_x = col * CELL_SIZE;
        int screen_x = world_x - state->camera_x;

        // Only draw if on screen
        if(screen_x >= 0 && screen_x < SCREEN_WIDTH) {
            canvas_draw_line(canvas, screen_x, 0, screen_x, SCREEN_HEIGHT - 1);

            // Draw column number label at every 5th column in row 1 (2nd row from top)
            if(col % 5 == 0) {
                char label[8];
                snprintf(label, sizeof(label), "%d", col);

                // Position label in the center of the cell in row 1
                int label_x = screen_x + 2; // Small offset from left edge
                int label_y = CELL_SIZE + 7; // Row 1, vertically centered

                // Only draw if label fits on screen
                if(label_x >= 0 && label_x < SCREEN_WIDTH - 10) {
                    canvas_draw_str(canvas, label_x, label_y, label);
                }
            }
        }
    }

    // Draw horizontal lines for each row
    for(int row = 0; row <= GRID_ROWS; row++) {
        int y = row * CELL_SIZE;
        canvas_draw_line(canvas, 0, y, SCREEN_WIDTH - 1, y);
    }
}

// Draw callback function
static void draw_callback(Canvas* canvas, void* ctx) {
    GameState* state = (GameState*)ctx;
    canvas_clear(canvas);

    // Calculate which tiles are visible
    int first_tile = state->camera_x / TILE_WIDTH;
    int last_tile = (state->camera_x + SCREEN_WIDTH) / TILE_WIDTH;

    // Clamp tile indices
    if(first_tile < 0) first_tile = 0;
    if(last_tile >= NUM_TILES) last_tile = NUM_TILES - 1;

    // Draw background tiles
    for(int i = first_tile; i <= last_tile; i++) {
        int tile_world_x = i * TILE_WIDTH;
        int tile_screen_x = tile_world_x - state->camera_x;

        // Select the appropriate tile icon
        const Icon* tile_icon = NULL;
        switch(i) {
        case 0:
            tile_icon = &I_map_tile_0;
            break;
        case 1:
            tile_icon = &I_map_tile_1;
            break;
        case 2:
            tile_icon = &I_map_tile_2;
            break;
        }

        if(tile_icon != NULL) {
            canvas_draw_icon(canvas, tile_screen_x, 0, tile_icon);
        }
    }

    // Draw grid overlay if enabled
    if(state->grid_view_enabled) {
        draw_grid_overlay(canvas, state);
    }
    // Draw clouds first (so they appear behind everything else)
    for(int row = 0; row < GRID_ROWS; row++) {
        for(int col = 0; col < GRID_COLS; col++) {
            if(state->grid[row][col] == CELL_CLOUD) {
                int world_x = col * CELL_SIZE;
                int screen_x = world_x - state->camera_x;

                // Only draw if visible on screen
                if(screen_x >= -CELL_SIZE && screen_x < SCREEN_WIDTH) {
                    int y = row * CELL_SIZE;
                    canvas_draw_icon(canvas, screen_x, y, &I_cloud);
                }
            }
        }
    }

    // Draw grid cells (blocks and pills)
    for(int row = 0; row < GRID_ROWS; row++) {
        for(int col = 0; col < GRID_COLS; col++) {
            if(state->grid[row][col] != CELL_EMPTY && state->grid[row][col] != CELL_CLOUD) {
                int world_x = col * CELL_SIZE;
                int screen_x = world_x - state->camera_x;

                // Only draw if visible on screen
                if(screen_x >= -CELL_SIZE && screen_x < SCREEN_WIDTH) {
                    int y = row * CELL_SIZE;

                    if(state->grid[row][col] == CELL_BLOCK) {
                        // Draw block as filled rectangle
                        canvas_draw_box(canvas, screen_x, y, CELL_SIZE, CELL_SIZE);
                    } else if(state->grid[row][col] == CELL_PILL) {
                        // Draw pill as circle
                        canvas_draw_disc(canvas, screen_x + CELL_SIZE / 2, y + CELL_SIZE / 2, 3);
                    } else if(state->grid[row][col] == CELL_DIAMOND) {
                        canvas_draw_icon(canvas, screen_x, y, &I_diamond_empty);
                    } else if(state->grid[row][col] == CELL_DIAMOND_FILLED) {
                        canvas_draw_icon(canvas, screen_x, y, &I_diamond_full);
                    }
                }
            }
        }
    }
    // Draw solid ground box
    canvas_draw_box(canvas, 0, GROUND_Y, SCREEN_WIDTH, SCREEN_HEIGHT - GROUND_Y);

    // Draw character PANIS
    const Icon* char_icon = state->facing_right ? &I_bread_r : &I_bread_l;
    canvas_draw_icon(canvas, state->screen_x, state->y_pos, char_icon);

    // Draw stats at top
    char stats_str[32];
    canvas_set_font(canvas, FontSecondary);

    // Left: Block counter "B: [ground]/[total]"
    snprintf(stats_str, sizeof(stats_str), "B:%d(%d)", state->ground_blocks, state->block_count);
    canvas_draw_str(canvas, 1, 7, stats_str);

    // Center: Diamond counter "D: [filled]/[overall]"
    snprintf(
        stats_str, sizeof(stats_str), "D:%d(%d)", state->filled_diamonds, state->overall_diamonds);
    int text_width = canvas_string_width(canvas, stats_str);
    canvas_draw_str(canvas, (SCREEN_WIDTH - text_width) / 2, 7, stats_str);

    // Right: Pill counter "P: [collected]/[overall]"
    int collected_pills = state->score / 10;
    snprintf(stats_str, sizeof(stats_str), "P:%d(%d)", collected_pills, state->overall_pills);
    text_width = canvas_string_width(canvas, stats_str);
    canvas_draw_str(canvas, SCREEN_WIDTH - text_width - 1, 7, stats_str);

    // Reset color to black for other drawing
    canvas_set_color(canvas, ColorBlack);
}

// Input callback function
static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

// Apply gravity and update Y position
static void update_physics(GameState* state) {
    // Apply gravity
    if(!state->on_ground) {
        state->y_velocity += GRAVITY;
        if(state->y_velocity > MAX_FALL_SPEED) {
            state->y_velocity = MAX_FALL_SPEED;
        }
    }

    // Try to update Y position
    int new_y = state->y_pos + state->y_velocity;

    // Limit maximum jump height
    int max_height_y = GROUND_Y - CHAR_HEIGHT - MAX_JUMP_HEIGHT;
    if(new_y < max_height_y) {
        new_y = max_height_y;
        state->y_velocity = 0; // Stop upward movement
    }

    // Check collision with blocks
    if(state->y_velocity > 0) { // Falling down
        // Check if we hit a block below
        if(check_ground_support(state, state->world_x, new_y - 1)) {
            // Land on the block
            int feet_y = new_y + CHAR_HEIGHT;
            int block_row = feet_y / CELL_SIZE;
            state->y_pos = block_row * CELL_SIZE - CHAR_HEIGHT;
            state->y_velocity = 0;
            state->on_ground = true;
            return;
        }
    } else if(state->y_velocity < 0) { // Moving up
        // Check if we hit a block above
        if(check_block_collision(state, state->world_x, new_y)) {
            state->y_velocity = 0;
            trigger_vibration(state);
            return;
        }
    }

    state->y_pos = new_y;

    // Ground collision
    int ground_pos = GROUND_Y - CHAR_HEIGHT;
    if(state->y_pos >= ground_pos) {
        state->y_pos = ground_pos;
        state->y_velocity = 0;
        state->on_ground = true;
    } else {
        state->on_ground = false;
    }

    // Collect any pills at current position
    collect_pills(state);
}

// Handle jump input
static void handle_jump(GameState* state) {
    if(state->on_ground) {
        uint32_t current_time = furi_get_tick();
        bool is_double_click = (current_time - state->last_jump_time) < DOUBLE_CLICK_MS;

        // Double-click = big jump, single click = small jump
        state->y_velocity = is_double_click ? BIG_JUMP_VELOCITY : SMALL_JUMP_VELOCITY;

        state->last_jump_time = current_time;
        state->on_ground = false;
    }
}

// Update horizontal movement logic
static void update_game(GameState* state, InputKey key) {
    int old_world_x = state->world_x;
    int new_world_x = state->world_x;
    int new_screen_x = state->screen_x;
    int new_camera_x = state->camera_x;

    if(key == InputKeyRight) {
        state->facing_right = true;

        // Check if we can move right
        if(state->world_x < TOTAL_MAP_WIDTH - CHAR__WIDTH) {
            // Calculate new position
            new_world_x = state->world_x + MOVEMENT_SPEED;

            // Check for block collision
            if(check_block_collision(state, new_world_x, state->y_pos)) {
                trigger_vibration(state);
                return;
            }

            // Determine if we should scroll or move character
            if(state->screen_x >= START_SCROLL_X &&
               state->camera_x < TOTAL_MAP_WIDTH - SCREEN_WIDTH) {
                // Scroll the world
                new_camera_x = state->camera_x + MOVEMENT_SPEED;

                // Clamp camera
                if(new_camera_x > TOTAL_MAP_WIDTH - SCREEN_WIDTH) {
                    int overflow = new_camera_x - (TOTAL_MAP_WIDTH - SCREEN_WIDTH);
                    new_camera_x = TOTAL_MAP_WIDTH - SCREEN_WIDTH;
                    new_screen_x = state->screen_x + overflow;
                } else {
                    new_screen_x = state->screen_x;
                }
            } else {
                // Move character on screen
                new_screen_x = state->screen_x + MOVEMENT_SPEED;
                new_camera_x = state->camera_x;

                // Clamp to screen edge
                if(new_screen_x > SCREEN_WIDTH - CHAR__WIDTH) {
                    new_screen_x = SCREEN_WIDTH - CHAR__WIDTH;
                }
            }

            // Clamp world position
            if(new_world_x > TOTAL_MAP_WIDTH - CHAR__WIDTH) {
                new_world_x = TOTAL_MAP_WIDTH - CHAR__WIDTH;
            }

            state->world_x = new_world_x;
            state->screen_x = new_screen_x;
            state->camera_x = new_camera_x;
        }
    } else if(key == InputKeyLeft) {
        state->facing_right = false;

        // Check if we can move left
        if(state->world_x > 0) {
            // Calculate new position
            new_world_x = state->world_x - MOVEMENT_SPEED;

            // Check for block collision
            if(check_block_collision(state, new_world_x, state->y_pos)) {
                trigger_vibration(state);
                return;
            }

            // Determine if we should scroll or move character
            if(state->screen_x <= START_SCROLL_X && state->camera_x > 0) {
                // Scroll the world (move camera left)
                new_camera_x = state->camera_x - MOVEMENT_SPEED;

                // Clamp camera
                if(new_camera_x < 0) {
                    int overflow = -new_camera_x;
                    new_camera_x = 0;
                    new_screen_x = state->screen_x - overflow;
                } else {
                    new_screen_x = state->screen_x;
                }
            } else {
                // Move character on screen
                new_screen_x = state->screen_x - MOVEMENT_SPEED;
                new_camera_x = state->camera_x;

                // Clamp to screen edge
                if(new_screen_x < 0) {
                    new_screen_x = 0;
                }
            }

            // Clamp world position
            if(new_world_x < 0) {
                new_world_x = 0;
            }

            state->world_x = new_world_x;
            state->screen_x = new_screen_x;
            state->camera_x = new_camera_x;
        }
    }

    // Vibrate if we hit a boundary
    if((old_world_x != state->world_x) &&
       (state->world_x == 0 || state->world_x == TOTAL_MAP_WIDTH - CHAR__WIDTH)) {
        trigger_vibration(state);
    }
}

// Main application entry point
int32_t panis_main(void* p) {
    UNUSED(p);

    // Create event queue for input events
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    // Initialize game state
    GameState* state = malloc(sizeof(GameState));
    state->world_x = CHAR_START_X; // Start at 1/4 of screen width
    state->screen_x = CHAR_START_X;
    state->camera_x = 0;
    state->facing_right = true;
    state->running = true;
    state->y_pos = GROUND_Y - CHAR_HEIGHT;
    state->y_velocity = 0;
    state->on_ground = true;
    state->last_jump_time = 0;
    state->notifications = furi_record_open(RECORD_NOTIFICATION);
    state->grid_view_enabled = false; // Grid view starts disabled
    state->melody_thread = NULL; // No melody thread initially

    // Initialize collision grid
    init_grid(state);

    // Set up view port
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, state);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    // Register view port in GUI
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // Main game loop
    InputEvent event;
    while(state->running) {
        // Process input events
        if(furi_message_queue_get(event_queue, &event, 100) == FuriStatusOk) {
            // Handle back button
            if(event.key == InputKeyBack && event.type == InputTypePress) {
                state->running = false;
                continue;
            }

            // Handle grid view toggle with down button
            if(event.key == InputKeyDown) {
                if(event.type == InputTypePress) {
                    state->grid_view_enabled = true;
                } else if(event.type == InputTypeRelease) {
                    state->grid_view_enabled = false;
                }
            }

            // Handle OK button to play melody
            if(event.key == InputKeyOk && event.type == InputTypePress) {
                play_melody_async(state);
            }

            // Handle jumping
            if(event.key == InputKeyUp) {
                if(event.type == InputTypePress) {
                    handle_jump(state);
                }
            }

            // Handle horizontal movement
            if((event.type == InputTypePress || event.type == InputTypeRepeat) &&
               (event.key == InputKeyLeft || event.key == InputKeyRight)) {
                update_game(state, event.key);
            }
        }

        // Update physics every frame (independent of input)
        update_physics(state);

        // Request redraw
        view_port_update(view_port);
    }

    // Stop and cleanup melody thread if still running
    if(state->melody_thread != NULL) {
        FuriThreadState thread_state = furi_thread_get_state(state->melody_thread);
        if(thread_state != FuriThreadStateStopped) {
            // Thread is still running, wait for it with timeout
            furi_thread_join(state->melody_thread);
        }
        // Free the thread
        if(state->melody_thread != NULL) {
            furi_thread_free(state->melody_thread);
            state->melody_thread = NULL;
        }
    }

    // Cleanup
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_message_queue_free(event_queue);
    free(state);

    return 0;
}
