/**
 * Puck - A Pac-Man style nn chase game for Flipper Zero
 * 
 * Control a pie-shaped character (Puck girl) through a maze, eating dots while
 * avoiding three ghosts. Power pills make ghosts temporarily eatable.
 */

#include <furi.h> // Flipper Universal Registry Implementation = Core OS functionality
#include <gui/gui.h> // GUI system
#include <input/input.h> // Input handling (buttons)
#include <stdlib.h> // Standard library functions
#include <gui/elements.h> // to access button drawing functions
#include <notification/notification_messages.h> // for haptic feedback
#include <furi_hal_speaker.h> // for sound output
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "puck_icons.h" // Custom icon definitions, header file is auto-magically generated

// Maze dimensions
#define PLAYER_START_X 7
#define PLAYER_START_Y 6 // counting from bottom to top
#define MAZE_WIDTH     12
#define MAZE_HEIGHT    7
#define CELL_SIZE      8 // 7px corridor + 1px wall
// Offsets to position game on screen (128x64 display, maze is 97x57 pixels)
// To center: OFFSET_X = (128-97)/2 = 15, OFFSET_Y = (64-57)/2 = 3
// Note: HUD (score/lives) is drawn at x=100, adjust if using large X offset
#define OFFSET_X       2 // Horizontal offset for entire game area
#define OFFSET_Y       2 // Vertical offset for entire game area

// Game constants
#define MAX_GHOSTS      2
#define POWER_DURATION  180 // Ticks (150 ticks is about 5 seconds at 30 FPS)
#define GHOST_SPEED     12 // Move every N ticks (higher number means slower)
#define PREY_SPEED      18 // Move every N ticks when ghosts are eatable (slower)
#define ANIMATION_SPEED 5 // Animation frame every N ticks

// Patrol ghost behavior
#define PATROL_CHASE_DISTANCE 16 // Distance squared to start chasing (4 cells)
#define PATROL_CORNER_CYCLE   200 // Ticks to stay at each corner

// Directions
typedef enum {
    DIR_NONE = 0,
    DIR_NORTH = 1,
    DIR_EAST = 2,
    DIR_SOUTH = 4,
    DIR_WEST = 8
} Direction;

// Game states
typedef enum {
    STATE_PLAYING,
    STATE_PAUSED, // Back button short press pauses, shows "Resume" button hint
    STATE_DIED,
    STATE_GAME_OVER,
    STATE_WIN
} GameState;

// Ghost personalities
typedef enum {
    GHOST_CHASER, // Directly chases player
    GHOST_PATROLLER // Patrols corners, chases when close
} GhostPersonality;

// Entity structure
typedef struct {
    uint8_t x; // Logical cell X
    uint8_t y; // Logical cell Y
    Direction dir; // Current direction
    bool moving; // Currently moving
} Entity;

// Ghost structure
typedef struct {
    Entity entity;
    bool vulnerable; // Can be eaten
    bool alive; // Is active
    Direction last_dir; // Last direction moved
    GhostPersonality personality; // Behavior type
} Ghost;

// Game context
typedef struct {
    Entity player;
    Ghost ghosts[MAX_GHOSTS];
    uint8_t conn[MAZE_HEIGHT][MAZE_WIDTH]; // Connectivity map
    uint8_t pellets[MAZE_HEIGHT][MAZE_WIDTH]; // Pellets: 0=none, 1=dot, 2=power
    uint32_t score;
    uint8_t lives;
    uint8_t dots_remaining;
    uint8_t power_timer;
    uint32_t tick;
    GameState state;
    bool anim_frame; // Animation toggle
    bool running; // Game is running (set to false to exit)
    FuriMutex* mutex;
    NotificationApp* notifications;
} GameContext;

// Maze layout
static const uint8_t initial_conn[MAZE_HEIGHT][MAZE_WIDTH] = {
    {6, 10, 12, 6, 10, 10, 10, 10, 12, 6, 10, 12},
    {5, 6, 11, 11, 10, 12, 6, 10, 11, 11, 12, 5},
    {5, 7, 14, 10, 14, 15, 15, 14, 10, 14, 13, 5},
    {7, 13, 5, 2, 13, 3, 9, 7, 8, 5, 7, 13},
    {5, 5, 3, 10, 15, 10, 10, 15, 10, 9, 5, 5},
    {5, 3, 10, 10, 11, 12, 6, 11, 10, 10, 9, 5},
    {3, 10, 10, 10, 10, 11, 11, 10, 10, 10, 10, 9},
};
/* Decimal | Binary | Directions allowed (also in words)
 * --------+--------+-------------------------------------------
 *   0     | 0000   | none       (unreachable / wall)
 *   1     | 0001   | N
 *   2     | 0010   | E
 *   3     | 0011   | N, E       (bottom left corner)
 *   4     | 0100   | S
 *   5     | 0101   | N, S       (vertical corridor)
 *   6     | 0110   | E, S       (top left corner)
 *   7     | 0111   | N, E, S    (T-intersection left closed)
 *   8     | 1000   | W
 *   9     | 1001   | N, W       (bottom right corner)
 *  10     | 1010   | E, W       (horizontal corridor)
 *  11     | 1011   | N, E, W    (T-intersection bottom closed)
 *  12     | 1100   | S, W       (top right corner)
 *  13     | 1101   | N, S, W    (T-intersection right closed)
 *  14     | 1110   | E, S, W    (T-intersection top closed)
 *  15     | 1111   | N, E, S, W (4-way intersection)
*/

// Collectible pellets for the maze. NB: These are logical cells, not pixels!
// 0 = no pellet, 1 = normal dot, 2 = power pill
static const uint8_t initial_pellets[MAZE_HEIGHT][MAZE_WIDTH] = {
    {2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2},
};

// Check if movement is allowed in a direction
static bool can_move(GameContext* ctx, uint8_t x, uint8_t y, Direction dir) {
    if(x >= MAZE_WIDTH || y >= MAZE_HEIGHT) return false;
    return (ctx->conn[y][x] & dir) != 0;
}

// Get opposite direction
static Direction opposite_dir(Direction dir) {
    switch(dir) {
    case DIR_NORTH:
        return DIR_SOUTH;
    case DIR_SOUTH:
        return DIR_NORTH;
    case DIR_EAST:
        return DIR_WEST;
    case DIR_WEST:
        return DIR_EAST;
    default:
        return DIR_NONE;
    }
}

// Play ghost eaten sound (crunching/eating effect)
static void play_ghost_eaten_sound(void) {
    if(!furi_hal_speaker_acquire(1000)) {
        return; // Failed to acquire speaker
    }

    // IMPACT - brief mid-high frequency click
    furi_hal_speaker_start(2500, 1.0f);
    furi_delay_ms(2);

    // FRACTURE + CRACKLES - rapid frequency changes simulate noisy crunch
    for(int i = 0; i < 8; i++) {
        // Varying frequencies 800-3200 Hz for chaotic crunch effect
        uint32_t freq = 800 + (i * 300) % 2400;
        furi_hal_speaker_start(freq, 0.7f);
        furi_delay_ms(5 + (i % 3) * 3); // Varying durations: 5, 8, 11 ms

        furi_hal_speaker_stop();
        furi_delay_ms(2); // Brief gap between crackles
    }

    // JAW - low rumble
    furi_hal_speaker_start(150, 0.3f);
    furi_delay_ms(30);

    furi_hal_speaker_stop();
    furi_hal_speaker_release();
}

// Play power pill collection sound (Mario coin-style)
static void play_power_sound(void) {
    if(!furi_hal_speaker_acquire(1000)) {
        return; // Failed to acquire speaker
    }

    // Note 1: E6 (~1319 Hz) for 100ms
    furi_hal_speaker_start(1319, 1.0f);
    furi_delay_ms(100);

    // Note 2: G6 (~1568 Hz) for 40ms
    furi_hal_speaker_start(1568, 1.0f);
    furi_delay_ms(40);

    // Stop and release
    furi_hal_speaker_stop();
    furi_hal_speaker_release();
}

// Move entity in a direction
static bool move_entity(GameContext* ctx, Entity* e, Direction dir) {
    uint8_t new_x = e->x;
    uint8_t new_y = e->y;

    if(!can_move(ctx, e->x, e->y, dir)) return false;

    switch(dir) {
    case DIR_NORTH:
        new_y--;
        break;
    case DIR_SOUTH:
        new_y++;
        break;
    case DIR_EAST:
        new_x++;
        break;
    case DIR_WEST:
        new_x--;
        break;
    default:
        return false;
    }

    if(new_x >= MAZE_WIDTH || new_y >= MAZE_HEIGHT) return false;

    e->x = new_x;
    e->y = new_y;
    e->dir = dir;
    return true;
}

// Count total dots in maze
static uint8_t count_dots(GameContext* ctx) {
    uint8_t count = 0;
    for(uint8_t y = 0; y < MAZE_HEIGHT; y++) {
        for(uint8_t x = 0; x < MAZE_WIDTH; x++) {
            if(ctx->pellets[y][x] > 0) count++;
        }
    }
    return count;
}

// Check if maze connectivity is consistent (bidirectional)
// Returns true if consistent, false if errors found (logs to console)
static bool check_maze_consistency(void) {
    bool consistent = true;

    for(uint8_t y = 0; y < MAZE_HEIGHT; y++) {
        for(uint8_t x = 0; x < MAZE_WIDTH; x++) {
            uint8_t conn = initial_conn[y][x];

            // Check NORTH connection
            if((conn & DIR_NORTH) && y > 0) {
                if(!(initial_conn[y - 1][x] & DIR_SOUTH)) {
                    FURI_LOG_E(
                        "MAZE",
                        "Inconsistency at (%d,%d): has NORTH but (%d,%d) lacks SOUTH",
                        x,
                        y,
                        x,
                        y - 1);
                    consistent = false;
                }
            }

            // Check SOUTH connection
            if((conn & DIR_SOUTH) && y < MAZE_HEIGHT - 1) {
                if(!(initial_conn[y + 1][x] & DIR_NORTH)) {
                    FURI_LOG_E(
                        "MAZE",
                        "Inconsistency at (%d,%d): has SOUTH but (%d,%d) lacks NORTH",
                        x,
                        y,
                        x,
                        y + 1);
                    consistent = false;
                }
            }

            // Check EAST connection
            if((conn & DIR_EAST) && x < MAZE_WIDTH - 1) {
                if(!(initial_conn[y][x + 1] & DIR_WEST)) {
                    FURI_LOG_E(
                        "MAZE",
                        "Inconsistency at (%d,%d): has EAST but (%d,%d) lacks WEST",
                        x,
                        y,
                        x + 1,
                        y);
                    consistent = false;
                }
            }

            // Check WEST connection
            if((conn & DIR_WEST) && x > 0) {
                if(!(initial_conn[y][x - 1] & DIR_EAST)) {
                    FURI_LOG_E(
                        "MAZE",
                        "Inconsistency at (%d,%d): has WEST but (%d,%d) lacks EAST",
                        x,
                        y,
                        x - 1,
                        y);
                    consistent = false;
                }
            }
        }
    }

    if(consistent) {
        FURI_LOG_I("MAZE", "Maze connectivity is consistent!");
    }

    return consistent;
}

// Initialize game
static void game_init(GameContext* ctx) {
    // Copy maze data
    memcpy(ctx->conn, initial_conn, sizeof(initial_conn));
    memcpy(ctx->pellets, initial_pellets, sizeof(initial_pellets));

    // Initialize player (bottom left)
    ctx->player.x = PLAYER_START_X;
    ctx->player.y = PLAYER_START_Y;
    ctx->player.dir = DIR_EAST;
    ctx->player.moving = false;

    // Initialize ghosts
    for(int i = 0; i < MAX_GHOSTS; i++) {
        ctx->ghosts[i].entity.x = 5 + i; // Inside ghost box
        ctx->ghosts[i].entity.y = 3;
        ctx->ghosts[i].entity.dir = DIR_EAST;
        ctx->ghosts[i].vulnerable = false;
        ctx->ghosts[i].alive = true;
        ctx->ghosts[i].last_dir = DIR_EAST;
        // Assign different personalities
        ctx->ghosts[i].personality = (i == 0) ? GHOST_CHASER : GHOST_PATROLLER;
    }

    ctx->score = 0;
    ctx->lives = 3; // Initial lives
    ctx->dots_remaining = count_dots(ctx);
    ctx->power_timer = 0;
    ctx->tick = 0;
    ctx->state = STATE_PLAYING;
    ctx->anim_frame = false;
    ctx->running = true;
}

// Reset level (after death)
static void reset_positions(GameContext* ctx) {
    ctx->player.x = PLAYER_START_X;
    ctx->player.y = PLAYER_START_Y;
    ctx->player.dir = DIR_EAST;
    ctx->player.moving = false;

    for(int i = 0; i < MAX_GHOSTS; i++) {
        ctx->ghosts[i].entity.x = 5 + i;
        ctx->ghosts[i].entity.y = 3;
        ctx->ghosts[i].entity.dir = DIR_EAST;
        ctx->ghosts[i].vulnerable = false;
        ctx->ghosts[i].alive = true;
        ctx->ghosts[i].last_dir = DIR_EAST;
        // Maintain personalities across resets
        ctx->ghosts[i].personality = (i == 0) ? GHOST_CHASER : GHOST_PATROLLER;
    }

    ctx->power_timer = 0;
    ctx->state = STATE_PLAYING;
}

// Simple ghost AI with personality system
static void update_ghost(GameContext* ctx, Ghost* ghost, int ghost_index) {
    UNUSED(ghost_index); // Parameter reserved for future per-ghost logic
    if(!ghost->alive) return;

    // Determine target position based on personality
    uint8_t target_x = ctx->player.x;
    uint8_t target_y = ctx->player.y;

    if(ghost->personality == GHOST_PATROLLER && !ghost->vulnerable) {
        // Calculate distance to player (squared to avoid sqrt)
        int dx = (int)ghost->entity.x - (int)ctx->player.x;
        int dy = (int)ghost->entity.y - (int)ctx->player.y;
        int dist_sq = dx * dx + dy * dy;

        if(dist_sq >= PATROL_CHASE_DISTANCE) {
            // Far from player - patrol corners
            int corner = ((int)(ctx->tick / PATROL_CORNER_CYCLE)) % 4;
            target_x = (corner & 1) ? (MAZE_WIDTH - 1) : 0;
            target_y = (corner & 2) ? (MAZE_HEIGHT - 1) : 0;
        }
        // Otherwise chase player (target already set to player position)
    }

    // When vulnerable, flee from player instead of chase
    if(ghost->vulnerable) {
        // Reverse target to opposite side of maze
        target_x = (MAZE_WIDTH - 1) - ctx->player.x;
        target_y = (MAZE_HEIGHT - 1) - ctx->player.y;
    }

    // Find possible moves
    Direction possible[4];
    uint8_t possible_count = 0;

    // Check all possible directions (except backwards)
    Direction dirs[] = {DIR_NORTH, DIR_EAST, DIR_SOUTH, DIR_WEST};
    for(int i = 0; i < 4; i++) {
        if(dirs[i] == opposite_dir(ghost->last_dir)) continue;
        if(can_move(ctx, ghost->entity.x, ghost->entity.y, dirs[i])) {
            possible[possible_count++] = dirs[i];
        }
    }

    if(possible_count == 0) {
        // If stuck, allow reverse
        Direction rev = opposite_dir(ghost->last_dir);
        if(can_move(ctx, ghost->entity.x, ghost->entity.y, rev)) {
            possible[possible_count++] = rev;
        }
    }

    if(possible_count > 0) {
        Direction best_dir = possible[0];

        if(possible_count > 1) {
            // Pick direction that reduces distance to target
            int best_dist = 999;
            for(uint8_t i = 0; i < possible_count; i++) {
                uint8_t test_x = ghost->entity.x;
                uint8_t test_y = ghost->entity.y;

                switch(possible[i]) {
                case DIR_NORTH:
                    test_y--;
                    break;
                case DIR_SOUTH:
                    test_y++;
                    break;
                case DIR_EAST:
                    test_x++;
                    break;
                case DIR_WEST:
                    test_x--;
                    break;
                default:
                    break;
                }

                int dist = abs((int)test_x - (int)target_x) + abs((int)test_y - (int)target_y);

                if(dist < best_dist) {
                    best_dist = dist;
                    best_dir = possible[i];
                }
            }
        }

        if(move_entity(ctx, &ghost->entity, best_dir)) {
            ghost->last_dir = best_dir;
        }
    }
}

// Check collisions
static void check_collisions(GameContext* ctx) {
    // Check player vs pellets
    uint8_t pellet = ctx->pellets[ctx->player.y][ctx->player.x];
    if(pellet > 0) {
        ctx->pellets[ctx->player.y][ctx->player.x] = 0;
        ctx->dots_remaining--;

        if(pellet == 1) {
            ctx->score += 10;
        } else if(pellet == 2) {
            ctx->score += 50;
            ctx->power_timer = POWER_DURATION;
            for(int i = 0; i < MAX_GHOSTS; i++) {
                if(ctx->ghosts[i].alive) {
                    ctx->ghosts[i].vulnerable = true;
                }
            }
            play_power_sound();
        }
    }

    // Check player vs ghosts
    for(int i = 0; i < MAX_GHOSTS; i++) {
        if(!ctx->ghosts[i].alive) continue;

        if(ctx->ghosts[i].entity.x == ctx->player.x && ctx->ghosts[i].entity.y == ctx->player.y) {
            if(ctx->ghosts[i].vulnerable) {
                // Eat ghost
                ctx->ghosts[i].alive = false;
                play_ghost_eaten_sound();
                ctx->score += 200;
            } else {
                // Player dies
                notification_message(ctx->notifications, &sequence_single_vibro);
                ctx->lives--;
                if(ctx->lives == 0) {
                    ctx->state = STATE_GAME_OVER;
                } else {
                    ctx->state = STATE_DIED;
                }
            }
        }
    }

    // Check win condition
    if(ctx->dots_remaining == 0) {
        ctx->state = STATE_WIN;
    }
}

// Update game state
static void game_update(GameContext* ctx) {
    if(ctx->state != STATE_PLAYING) return;

    ctx->tick++;

    // Update animation
    if(ctx->tick % ANIMATION_SPEED == 0) {
        ctx->anim_frame = !ctx->anim_frame;
    }

    // Update power timer
    if(ctx->power_timer > 0) {
        ctx->power_timer--;
        if(ctx->power_timer == 0) {
            for(int i = 0; i < MAX_GHOSTS; i++) {
                // Respawn dead ghosts in ghost house
                if(!ctx->ghosts[i].alive) {
                    ctx->ghosts[i].alive = true;
                    ctx->ghosts[i].entity.x = 5 + i;
                    ctx->ghosts[i].entity.y = 3;
                    ctx->ghosts[i].entity.dir = DIR_EAST;
                    ctx->ghosts[i].last_dir = DIR_EAST;
                }
                ctx->ghosts[i].vulnerable = false;
            }
        }
    }

    // Update ghosts
    for(int i = 0; i < MAX_GHOSTS; i++) {
        // Use slower speed when vulnerable
        uint8_t speed = ctx->ghosts[i].vulnerable ? PREY_SPEED : GHOST_SPEED;

        if(ctx->tick % speed == 0) {
            update_ghost(ctx, &ctx->ghosts[i], i);
        }
    }

    // Check collisions
    check_collisions(ctx);
}

// Draw pellets
static void draw_pellets(Canvas* canvas, GameContext* ctx) {
    canvas_set_color(canvas, ColorBlack);

    for(uint8_t y = 0; y < MAZE_HEIGHT; y++) {
        for(uint8_t x = 0; x < MAZE_WIDTH; x++) {
            if(ctx->pellets[y][x] == 0) continue;

            uint8_t px = OFFSET_X + x * CELL_SIZE + 3;
            uint8_t py = OFFSET_Y + y * CELL_SIZE + 3;

            if(ctx->pellets[y][x] == 1) {
                // Regular dot
                canvas_draw_dot(canvas, px, py);
            } else {
                // Power pill
                canvas_draw_circle(canvas, px, py, 2);
            }
        }
    }
}

// Draw entity (player or ghost)
static void draw_entity(Canvas* canvas, Entity* e, const Icon* icon) {
    uint8_t px = OFFSET_X + e->x * CELL_SIZE;
    uint8_t py = OFFSET_Y + e->y * CELL_SIZE;
    canvas_draw_icon(canvas, px, py, icon);
}

// Draw modal dialog box with text
static void draw_simple_modal(Canvas* canvas, const char* text) {
    const int box_w = 68;
    const int box_h = 20;
    const int box_x = (9 * MAZE_WIDTH - box_w) / 2;
    const int box_y = 20;
    // White filled rectangle
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, box_x, box_y, box_w, box_h);
    // Black border
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, box_x, box_y, box_w, box_h);
    // Text inside
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, box_x + 2, box_y + 14, text);
}

// Render callback
static void render_callback(Canvas* canvas, void* ctx_void) {
    GameContext* ctx = ctx_void;
    furi_mutex_acquire(ctx->mutex, FuriWaitForever);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    // Draw the background maze image
    canvas_draw_icon(canvas, 1, 1, &I_maze);

    // Draw pellets on top of maze
    draw_pellets(canvas, ctx);

    // Draw player with animation
    const Icon* player_icon = NULL;
    if(ctx->state == STATE_PLAYING) {
        bool open = ctx->anim_frame;
        switch(ctx->player.dir) {
        case DIR_EAST:
            player_icon = open ? &I_puck_r_o_7x7 : &I_puck_r_c_7x7;
            break;
        case DIR_WEST:
            player_icon = open ? &I_puck_l_o_7x7 : &I_puck_l_c_7x7;
            break;
        case DIR_NORTH:
            player_icon = open ? &I_puck_u_o_7x7 : &I_puck_u_c_7x7;
            break;
        case DIR_SOUTH:
            player_icon = open ? &I_puck_d_o_7x7 : &I_puck_d_c_7x7;
            break;
        default:
            player_icon = &I_puck_r_c_7x7;
            break;
        }
        draw_entity(canvas, &ctx->player, player_icon);
    }

    // Draw ghosts
    for(int i = 0; i < MAX_GHOSTS; i++) {
        if(!ctx->ghosts[i].alive) continue;

        const Icon* ghost_icon;
        if(ctx->ghosts[i].vulnerable) {
            ghost_icon = ctx->anim_frame ? &I_prey_1 : &I_prey_2;
        } else {
            ghost_icon = ctx->anim_frame ? &I_monster_1 : &I_monster_2;
        }
        draw_entity(canvas, &ctx->ghosts[i].entity, ghost_icon);
    }

    // Draw Score
    canvas_draw_str_aligned(canvas, 99, 31, AlignLeft, AlignBottom, "Score:");
    char buf[32];
    snprintf(buf, sizeof(buf), "%05lu", ctx->score);
    canvas_draw_str(canvas, 99, 39, buf);

    // Draw power-up countdown (if active)
    if(ctx->power_timer > 0) {
        uint8_t seconds = (ctx->power_timer + 29) / 30; // Round up
        snprintf(buf, sizeof(buf), "P: %u s", seconds);
        canvas_draw_str(canvas, 99, 47, buf);
    }

    // Draw lives (always show 3 hearts: full for remaining lives, empty for lost)
    for(uint8_t i = 0; i < 3; i++) {
        if(i < ctx->lives) {
            canvas_draw_icon(canvas, 99 + i * 8, 15, &I_heart_full_7x7);
        } else {
            canvas_draw_icon(canvas, 99 + i * 8, 15, &I_heart_7x7);
        }
    }
    // Version info
    canvas_draw_str_aligned(canvas, 99, 55, AlignLeft, AlignBottom, "v0.3");
    // Draw navigation hint
    canvas_draw_icon(canvas, 121, 57, &I_back);
    if(ctx->state == STATE_PAUSED || ctx->state == STATE_DIED || ctx->state == STATE_GAME_OVER ||
       ctx->state == STATE_WIN) {
        canvas_draw_str_aligned(canvas, 120, 63, AlignRight, AlignBottom, "Exit");
    } else {
        canvas_draw_str_aligned(canvas, 120, 63, AlignRight, AlignBottom, "Brk");
    }

    // Draw game state messages
    if(ctx->state == STATE_PAUSED) {
        elements_button_center(canvas, "Continue");
    } else if(ctx->state == STATE_DIED) {
        draw_simple_modal(canvas, "You died.");
        elements_button_center(canvas, "OK");
    } else if(ctx->state == STATE_GAME_OVER) {
        draw_simple_modal(canvas, "GAME OVER");
        elements_button_center(canvas, "Restart");
    } else if(ctx->state == STATE_WIN) {
        draw_simple_modal(canvas, "YOU WIN!");
        elements_button_center(canvas, "Restart");
    }

    furi_mutex_release(ctx->mutex);
}

// Input callback
// - Back short press: Pause/unpause
// - Back long press: Exit game
// - OK: Resume from pause, continue after death, restart after game over/win
// - D-pad: Move player (press = single move, hold = continuous)
static void input_callback(InputEvent* input, void* ctx_void) {
    GameContext* ctx = ctx_void;
    furi_mutex_acquire(ctx->mutex, FuriWaitForever);

    if(input->type == InputTypePress) {
        if(ctx->state == STATE_PLAYING) {
            Direction dir = DIR_NONE;
            switch(input->key) {
            case InputKeyUp:
                dir = DIR_NORTH;
                break;
            case InputKeyDown:
                dir = DIR_SOUTH;
                break;
            case InputKeyLeft:
                dir = DIR_WEST;
                break;
            case InputKeyRight:
                dir = DIR_EAST;
                break;
            default:
                break;
            }

            if(dir != DIR_NONE) {
                move_entity(ctx, &ctx->player, dir);
            }
        } else if(ctx->state == STATE_PAUSED) {
            // Unpause with OK button
            if(input->key == InputKeyOk) {
                ctx->state = STATE_PLAYING;
            }
        } else if(ctx->state == STATE_DIED) {
            if(input->key == InputKeyOk) {
                reset_positions(ctx);
            }
        } else if(ctx->state == STATE_GAME_OVER || ctx->state == STATE_WIN) {
            if(input->key == InputKeyOk) {
                game_init(ctx);
            }
        }
    } else if(input->type == InputTypeShort) {
        // Short back button press - pause or exit
        if(input->key == InputKeyBack) {
            if(ctx->state == STATE_PLAYING) {
                ctx->state = STATE_PAUSED;
            } else if(
                ctx->state == STATE_PAUSED || ctx->state == STATE_DIED ||
                ctx->state == STATE_GAME_OVER || ctx->state == STATE_WIN) {
                ctx->running = false;
            }
        }
    } else if(input->type == InputTypeLong) {
        // Long back button press - exit game
        if(input->key == InputKeyBack) {
            ctx->running = false;
        }
    } else if(input->type == InputTypeRepeat) {
        // Handle continuous movement
        if(ctx->state == STATE_PLAYING) {
            Direction dir = DIR_NONE;
            switch(input->key) {
            case InputKeyUp:
                dir = DIR_NORTH;
                break;
            case InputKeyDown:
                dir = DIR_SOUTH;
                break;
            case InputKeyLeft:
                dir = DIR_WEST;
                break;
            case InputKeyRight:
                dir = DIR_EAST;
                break;
            default:
                break;
            }

            if(dir != DIR_NONE) {
                move_entity(ctx, &ctx->player, dir);
                ctx->player.moving = true;
            }
        }
    } else if(input->type == InputTypeRelease) {
        ctx->player.moving = false;
    }

    furi_mutex_release(ctx->mutex);
}

// Timer callback for game updates
static void timer_callback(void* ctx_void) {
    GameContext* ctx = ctx_void;
    furi_mutex_acquire(ctx->mutex, FuriWaitForever);
    game_update(ctx);
    furi_mutex_release(ctx->mutex);
}

// Main entry point
int32_t puckgirl_main(void* p) {
    UNUSED(p);
    // Check maze consistency at startup
    check_maze_consistency();

    GameContext* ctx = malloc(sizeof(GameContext));
    ctx->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    ctx->notifications = furi_record_open(RECORD_NOTIFICATION);
    game_init(ctx);

    // Setup GUI
    Gui* gui = furi_record_open(RECORD_GUI);
    ViewPort* view_port = view_port_alloc();

    view_port_draw_callback_set(view_port, render_callback, ctx);
    view_port_input_callback_set(view_port, input_callback, ctx);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // Setup timer for game updates (30 FPS)
    FuriTimer* timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, ctx);
    furi_timer_start(timer, furi_ms_to_ticks(33));

    // Main loop - wait for running flag to become false (back button pressed)
    while(ctx->running) {
        furi_delay_ms(100);
    }

    // Cleanup
    furi_timer_stop(timer);
    furi_timer_free(timer);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_mutex_free(ctx->mutex);
    free(ctx);

    return 0;
}
