#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <storage/storage.h>
#include "p1x_network_defender_icons.h"

// Game constants
#define SCREEN_WIDTH               128
#define SCREEN_HEIGHT              64
#define MAX_PACKETS                16 // Increased DDOS limit to 16 (4 per computer)
#define MAX_PACKETS_PER_COMPUTER   4 // Maximum packets per computer before overflow
#define DDOS_LIMIT                 17 // Game over when total packets reaches this number
#define COMPUTER_OVERFLOW          5 // Game over when one computer reaches this many packets
#define HACK_WARN_TIME             2000 // 2 seconds warning (in ms)
#define HACK_TIME                  5000 // 5 seconds to fully hack (in ms)
#define PATCH_TIME                 2000 // 2 seconds to patch (in ms)
#define PACKET_ANIMATION_TIME      300 // Faster animation for accepted packets
#define PACKET_SPAWN_CHANCE        1 // Reduced packet spawn chance for slower gameplay
#define BASE_PACKET_SPAWN_INTERVAL 1800 // Base interval between packet spawns (ms)
#define ACCELERATION_FACTOR        0.5 // Each dead computer makes game faster (halves the interval)
#define HISCORE_FILENAME           APP_DATA_PATH("hiscore.save") // Updated path using APP_DATA_PATH macro

// Add upcoming packets queue to game state
#define MAX_UPCOMING_PACKETS 3

// Define game states - remove duplicates
#define GAME_STATE_TITLE 0
#define GAME_STATE_MENU  1
#define GAME_STATE_HELP  2
#define GAME_STATE_PLAY  3

// System positions (4 corners)
typedef struct {
    int x, y;
} Position;

static const Position SYSTEM_POSITIONS[4] = {
    {32, 21}, // Top-left (moved down by 5px)
    {96, 21}, // Top-right (moved down by 5px)
    {32, 53}, // Bottom-left (moved down by 5px)
    {96, 53} // Bottom-right (moved down by 5px)
};

// Adjust player positions to be inside the computer boxes
static const Position PLAYER_POSITIONS[4] = {
    {32, 21}, // Inside top-left system
    {96, 21}, // Inside top-right system
    {32, 53}, // Inside bottom-left system
    {96, 53} // Inside bottom-right system
};

// Server position (center)
static const Position SERVER_POSITION = {64, 37}; // Center server (moved down by 5px)

// Game state
typedef struct {
    int player_pos; // 0-3 for the 4 positions
    bool hacking[4]; // Is system being hacked?
    uint32_t hack_start[4]; // When hacking began (ms)
    uint32_t warn_start[4]; // When warning began (ms)
    int packets[4]; // Packets waiting at each position
    bool patching; // Currently patching?
    uint32_t patch_start; // When patching began (ms)
    bool game_over;
    int game_over_reason; // 1=hack, 2=DDOS
    bool packet_animation; // Packet moving to server
    uint32_t anim_start; // When animation began
    int anim_from; // Which system accepted the packet
    int score; // Player's score
    int upcoming_packets[4][MAX_UPCOMING_PACKETS]; // Queue of upcoming packets for each system
    uint32_t last_packet_spawn_time; // Last packet spawn time
    int current_state; // Current game state
    int help_page; // Help page number
    int menu_selection; // Menu selection index
    bool computer_dead[4]; // Is the computer dead (hacked)
    int high_score; // High score from previous games
    bool new_high_score; // Flag to indicate if current score is a new high score
} GameState;

// Game over reasons
#define GAME_OVER_HACK 1
#define GAME_OVER_DDOS 2

// Function to load high score from storage
static bool load_high_score(GameState* state) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool result = false;

    // Check if the high score file exists
    if(storage_common_stat(storage, HISCORE_FILENAME, NULL) == FSE_OK) {
        if(storage_file_open(file, HISCORE_FILENAME, FSAM_READ, FSOM_OPEN_EXISTING)) {
            uint16_t bytes_read = storage_file_read(file, &state->high_score, sizeof(int));
            result = (bytes_read == sizeof(int));
        }
    } else {
        // If file doesn't exist, initialize high score to 0
        state->high_score = 0;
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return result;
}

// Function to save high score to storage
static bool save_high_score(GameState* state) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool result = false;

    // Make sure app data directory exists
    storage_common_mkdir(storage, APP_DATA_PATH(""));

    // Create the file and write high score
    if(storage_file_open(file, HISCORE_FILENAME, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint16_t bytes_written = storage_file_write(file, &state->high_score, sizeof(int));
        result = (bytes_written == sizeof(int));
        // Debug: Print to log
        FURI_LOG_I("NetDefender", "Saved high score: %d, result: %d", state->high_score, result);
    } else {
        FURI_LOG_E("NetDefender", "Failed to save high score: %d", state->high_score);
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return result;
}

// Forward declaration of draw_upcoming_packets
static void draw_upcoming_packets(Canvas* canvas, GameState* state);

// Reset game state for new game
static void reset_game_state(GameState* state) {
    state->player_pos = 0;
    state->patching = false;
    state->game_over = false;
    state->game_over_reason = 0;
    state->packet_animation = false;
    state->score = 0;
    state->last_packet_spawn_time = furi_get_tick();
    state->menu_selection = 0;
    state->help_page = 0;
    state->new_high_score = false; // Reset new high score flag
    for(int i = 0; i < 4; i++) {
        state->hacking[i] = false;
        state->hack_start[i] = 0;
        state->warn_start[i] = 0;
        state->packets[i] = 0;
        state->computer_dead[i] = false; // Reset dead state
        for(int j = 0; j < MAX_UPCOMING_PACKETS; j++) {
            state->upcoming_packets[i][j] = rand() % 2;
        }
    }
}

// Initialize game state
static void game_init(GameState* state) {
    state->current_state = GAME_STATE_TITLE;
    state->high_score = 0;
    state->new_high_score = false;

    // Load high score from storage
    load_high_score(state);

    reset_game_state(state);
}

// Draw title screen
static void draw_title_screen(Canvas* canvas) {
    canvas_clear(canvas);

    // Display title graphics - using correct icon names
    canvas_draw_icon(canvas, 9, 10, &I_title_network);
    canvas_draw_icon(canvas, 10, 39, &I_title_defender);
    canvas_draw_icon(canvas, 94, 30, &I_player_left);
    canvas_draw_icon(canvas, 28, 31, &I_pc_monitor);
}

// Draw menu screen with selection indicator
static void draw_menu_screen(Canvas* canvas, int selection) {
    canvas_clear(canvas);

    // Add title without frame
    canvas_draw_icon(canvas, 18, 12, &I_menu_title);
    canvas_draw_icon(canvas, 102, 14, &I_player_left);
    canvas_draw_icon(canvas, 79, 3, &I_pc_monitor);
    // Draw menu options with selection indicator
    canvas_set_font(canvas, FontSecondary);

    // Selection indicator
    canvas_draw_str(canvas, 10, 40 + (selection * 10), ">");

    canvas_draw_str(canvas, 20, 40, "Start Game");
    canvas_draw_str(canvas, 20, 50, "Help");
    canvas_draw_str(canvas, 20, 60, "Quit");
}

// Draw help screen with optimized layout
static void draw_help_screen(Canvas* canvas, int page) {
    canvas_clear(canvas);

    canvas_set_font(canvas, FontSecondary);

    if(page == 0) {
        // Page 1: Icons explanation - more compact layout
        canvas_draw_str(canvas, 5, 10, "ICONS:");

        // Packet icon
        canvas_draw_icon(canvas, 10, 14, &I_packet);
        canvas_draw_str(canvas, 20, 21, "- Data packet");

        // Computer icon
        canvas_draw_icon(canvas, 6, 22, &I_pc_monitor);
        canvas_draw_str(canvas, 20, 32, "- Computer system");

        // Warning icon
        canvas_draw_str(canvas, 10, 43, "!");
        canvas_draw_str(canvas, 20, 43, "- Hack warning");

        // Active hack icon
        canvas_draw_str(canvas, 10, 54, "!!!");
        canvas_draw_str(canvas, 20, 54, "- Active hack attack");
    } else {
        // Page 2: Gameplay instructions - more compact layout
        canvas_draw_str(canvas, 5, 10, "HOW TO PLAY:");
        canvas_draw_str(canvas, 5, 20, "- Move with D-pad");
        canvas_draw_str(canvas, 5, 30, "- Press OK to accept packet");
        canvas_draw_str(canvas, 5, 40, "- Hold OK for 3s to patch");
        canvas_draw_str(canvas, 5, 50, "- Keep packets below limit");
        canvas_draw_str(canvas, 5, 60, "- Max 10 total packets");
    }

    // Small page indicator at bottom right
    canvas_draw_str(canvas, SCREEN_WIDTH - 20, SCREEN_HEIGHT - 2, page == 0 ? "1/2" : "2/2");
}

// Draw callback for GUI
static void draw_callback(Canvas* canvas, void* ctx) {
    GameState* state = (GameState*)ctx;

    if(state->current_state == GAME_STATE_TITLE) {
        draw_title_screen(canvas);
        return;
    } else if(state->current_state == GAME_STATE_MENU) {
        draw_menu_screen(canvas, state->menu_selection);
        return;
    } else if(state->current_state == GAME_STATE_HELP) {
        draw_help_screen(canvas, state->help_page);
        return;
    }

    if(state->game_over) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 30, 20, "GAME OVER");

        canvas_set_font(canvas, FontSecondary);
        if(state->game_over_reason == GAME_OVER_HACK) {
            canvas_draw_str(canvas, 15, 35, "System hacked!");
        } else if(state->game_over_reason == GAME_OVER_DDOS) {
            canvas_draw_str(canvas, 15, 35, "DDOS attack!");
        }

        // Display score and high score on the same line
        char score_text[32];
        snprintf(score_text, sizeof(score_text), "Score: %d", state->score);
        canvas_draw_str(canvas, 15, 45, score_text);

        // Display high score next to current score
        char hiscore_text[32];
        snprintf(hiscore_text, sizeof(hiscore_text), "HI: %04d", state->high_score);
        canvas_draw_str(canvas, 80, 45, hiscore_text);

        // Show exclamation marks for new high score
        if(state->new_high_score) {
            canvas_draw_str(canvas, 115, 45, "!!!");
        }

        canvas_draw_str(canvas, 15, 55, "Press BACK to restart");
        return;
    }

    // Draw server in center using server sprite instead of pc
    canvas_draw_icon(canvas, SERVER_POSITION.x - 8, SERVER_POSITION.y - 8, &I_server);

    // Removed packet count near server (no more counter in center)

    // Draw systems and states
    for(int i = 0; i < 4; i++) {
        int x = SYSTEM_POSITIONS[i].x;
        int y = SYSTEM_POSITIONS[i].y;

        // Draw system (computer icon) with different states
        if(state->computer_dead[i]) {
            // Use pc_hacked for dead (fully hacked) computers
            canvas_draw_icon(canvas, x - 8, y - 8, &I_pc_hacked);
        } else if(state->hacking[i]) {
            // Use pc_using for computers currently being hacked
            canvas_draw_icon(canvas, x - 8, y - 8, &I_pc_using);
        } else {
            // Use normal pc for healthy computers
            canvas_draw_icon(canvas, x - 8, y - 8, &I_pc);
        }

        // Skip drawing packets and hacking warnings on dead computers
        if(!state->computer_dead[i]) {
            // Hacking warning or active hack
            uint32_t now = furi_get_tick();
            if(state->warn_start[i] && !state->hacking[i]) {
                if((now - state->warn_start[i]) / 500 % 2 == 0) { // Flash every 0.5s
                    canvas_draw_str(canvas, x - 4, y - 12, "!");
                }
            } else if(state->hacking[i]) {
                canvas_set_font(canvas, FontSecondary);
                canvas_draw_str(canvas, x - 4, y - 12, "!!!");
                canvas_set_font(canvas, FontPrimary);
            }

            // Draw packets at the system - Updated to show up to 4 packets per computer
            for(int p = 0; p < state->packets[i] && p < MAX_PACKETS_PER_COMPUTER; p++) {
                int packet_x = (i % 2 == 0) ? x + 10 + p * 5 : x - 14 - p * 5;
                canvas_draw_icon(canvas, packet_x, y + 1, &I_packet);
            }
        }
    }

    // Call the draw_upcoming_packets function instead of having the code inline
    draw_upcoming_packets(canvas, state);

    // Draw packet animation if active
    if(state->packet_animation) {
        uint32_t now = furi_get_tick();
        float progress = (float)(now - state->anim_start) / PACKET_ANIMATION_TIME;
        if(progress > 1.0f) progress = 1.0f;

        // Source position
        int sx = SYSTEM_POSITIONS[state->anim_from].x;
        int sy = SYSTEM_POSITIONS[state->anim_from].y;

        // Calculate interpolated position
        int px = sx + (SERVER_POSITION.x - sx) * progress;
        int py = sy + (SERVER_POSITION.y - sy) * progress;

        // Draw packet moving toward server
        canvas_draw_icon(canvas, px - 3, py - 3, &I_packet);
    }

    // Draw player using appropriate sprite based on position
    int px = PLAYER_POSITIONS[state->player_pos].x;
    int py = PLAYER_POSITIONS[state->player_pos].y;

    if(state->patching) {
        // When patching, show a different animation
        if((furi_get_tick() / 200) % 2) {
            canvas_draw_icon(
                canvas,
                (state->player_pos % 2 == 0) ? px + 6 : px - 16,
                py - 14,
                (state->player_pos % 2 == 0) ? &I_player_left : &I_player_right);
        } else {
            // Alternate frame for patching animation
            canvas_draw_icon(
                canvas,
                (state->player_pos % 2 == 0) ? px + 6 : px - 16,
                py - 14,
                (state->player_pos % 2 == 0) ? &I_player_hack_left : &I_player_hack_right);
        }
    } else {
        // Normal player display - use left or right facing sprite based on position
        canvas_draw_icon(
            canvas,
            (state->player_pos % 2 == 0) ? px + 6 : px - 16,
            py - 14,
            (state->player_pos % 2 == 0) ? &I_player_left : &I_player_right);
    }

    // Simplified score display: just padded numbers (0000)
    // Position frame sprite behind score
    char score_text[16];
    snprintf(score_text, sizeof(score_text), "%04d", state->score);

    // Calculate position for score
    int score_x = SCREEN_WIDTH / 2 - canvas_string_width(canvas, score_text) / 2;

    // Draw score
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, score_x, 10, score_text);

    // Draw high score in the corner
    char hiscore_text[16];
    snprintf(hiscore_text, sizeof(hiscore_text), "HI: %04d", state->high_score);
    canvas_draw_str(canvas, 2, 10, hiscore_text);
}

// Input handling
static void input_callback(InputEvent* input, void* ctx) {
    FuriMessageQueue* queue = (FuriMessageQueue*)ctx;
    furi_message_queue_put(queue, input, FuriWaitForever);
}

// Fix the calculate_spawn_interval function to accelerate instead of slow down
static uint32_t calculate_spawn_interval(GameState* state) {
    // Count dead computers
    int dead_count = 0;
    for(int i = 0; i < 4; i++) {
        if(state->computer_dead[i]) {
            dead_count++;
        }
    }

    // Calculate current interval
    // Each dead computer makes the game faster by dividing by ACCELERATION_FACTOR
    float multiplier = 1.0f;
    for(int i = 0; i < dead_count; i++) {
        multiplier *= ACCELERATION_FACTOR; // This makes the interval smaller (faster)
    }

    return (uint32_t)(BASE_PACKET_SPAWN_INTERVAL * multiplier);
}

// Hide packet indicators for hacked computers by only drawing them for active computers
void draw_upcoming_packets(Canvas* canvas, GameState* state) {
    for(int i = 0; i < 4; i++) {
        // Skip drawing network and packets for dead computers
        if(state->computer_dead[i]) {
            continue; // Skip this computer
        }

        int x = SYSTEM_POSITIONS[i].x;
        int y = SYSTEM_POSITIONS[i].y;

        // Determine direction based on system position (left or right)
        bool is_right_side = (i % 2 == 1); // Systems 1 and 3 are on the right

        // Draw network background first - adjust positioning to align correctly
        if(is_right_side) {
            // Right side networks
            canvas_draw_icon(canvas, x + 8, y - 4, &I_network_right);
        } else {
            // Left side networks
            canvas_draw_icon(canvas, x - 35, y - 4, &I_network_left);
        }

        // Then draw the packets on top
        for(int j = 0; j < MAX_UPCOMING_PACKETS; j++) {
            // Calculate packet position with more spacing
            int packet_x = is_right_side ?
                               (x + 12 + j * 8) : // Right side systems, packets on right
                               (x - 18 - j * 8); // Left side systems, packets on left

            if(state->upcoming_packets[i][j] == 1) {
                // Use packet icon for filled slot
                canvas_draw_icon(canvas, packet_x, y - 3, &I_packet);
            } else {
                // Use no_packet icon for empty slot
                canvas_draw_icon(canvas, packet_x, y - 3, &I_no_packet);
            }
        }
    }
}

// Fixed update_upcoming_packets to give each computer a unique pattern
static void update_upcoming_packets(GameState* state) {
    uint32_t now = furi_get_tick();

    // Calculate current spawn interval based on number of dead computers
    uint32_t current_interval = calculate_spawn_interval(state);

    // Only update packets at dynamically calculated intervals
    if(now - state->last_packet_spawn_time < current_interval) {
        return;
    }

    state->last_packet_spawn_time = now;

    // Process all computers simultaneously - conveyor belt style

    // 1. Check if first packets arrive at computers
    for(int i = 0; i < 4; i++) {
        // If the first slot has a packet and computer can accept it
        if(state->upcoming_packets[i][0] == 1 && !state->computer_dead[i]) {
            // Increment the packet count
            state->packets[i]++;

            // Check for overflow immediately after adding a packet
            if(state->packets[i] >= COMPUTER_OVERFLOW) {
                // End game immediately on overflow
                state->game_over = true;
                state->game_over_reason = GAME_OVER_DDOS;
                furi_hal_vibro_on(true);
                furi_delay_ms(500);
                furi_hal_vibro_on(false);
                return; // Exit function early to prevent further processing
            }

            // Always mark as processed whether it was accepted or not
            state->upcoming_packets[i][0] = 0;
        }
    }

    // 2. Shift everything one step closer (conveyor belt movement)
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < MAX_UPCOMING_PACKETS - 1; j++) {
            state->upcoming_packets[i][j] = state->upcoming_packets[i][j + 1];
        }
    }

    // 3. Generate new packets in the last position - UNIQUE for each computer
    //    but still maintaining synchronized timing
    for(int i = 0; i < 4; i++) {
        // Each computer has its own 40% chance of getting a packet
        bool generate_packet = (rand() % 5 < 2);
        state->upcoming_packets[i][MAX_UPCOMING_PACKETS - 1] =
            (generate_packet && !state->computer_dead[i]) ? 1 : 0;
    }
}

// Update game logic - remove redundant packet handling code
static void update_game(GameState* state) {
    // Only run game logic if we're in play state
    if(state->current_state != GAME_STATE_PLAY) return;

    if(state->game_over) return;

    uint32_t now = furi_get_tick();

    // Update packet animation
    if(state->packet_animation && now - state->anim_start >= PACKET_ANIMATION_TIME) {
        state->packet_animation = false;
    }

    // Check for hacks and warnings
    for(int i = 0; i < 4; i++) {
        if(state->warn_start[i] && !state->hacking[i] &&
           now - state->warn_start[i] >= HACK_WARN_TIME) {
            state->hacking[i] = true;
            state->hack_start[i] = now;
            state->warn_start[i] = 0;
            furi_hal_vibro_on(true); // Vibrate when hack starts
            furi_delay_ms(200);
            furi_hal_vibro_on(false);
        }
        if(state->hacking[i] && now - state->hack_start[i] >= HACK_TIME) {
            state->computer_dead[i] = true; // Mark computer as dead
            state->hacking[i] = false; // No longer actively being hacked
            state->hack_start[i] = 0;

            furi_hal_vibro_on(true); // Vibrate to indicate computer death
            furi_delay_ms(300);
            furi_hal_vibro_on(false);

            // Check if all computers are dead or if 3 out of 4 computers are dead (endgame condition)
            bool all_dead = true;
            int dead_count = 0;

            for(int j = 0; j < 4; j++) {
                if(state->computer_dead[j]) {
                    dead_count++;
                } else {
                    all_dead = false;
                }
            }

            // End game if all computers are dead or if 3 out of 4 are dead
            if(all_dead || dead_count >= 3) {
                state->game_over = true;
                state->game_over_reason = GAME_OVER_HACK;

                // Check for high score
                if(state->score > state->high_score) {
                    state->high_score = state->score;
                    state->new_high_score = true;
                    save_high_score(state);
                }

                furi_hal_vibro_on(true); // Long vibration for game over
                furi_delay_ms(500);
                furi_hal_vibro_on(false);
                return;
            }
        }
    }

    // Patching logic
    if(state->patching && now - state->patch_start >= PATCH_TIME) {
        int pos = state->player_pos;
        if(state->hacking[pos]) {
            state->hacking[pos] = false;
            state->hack_start[pos] = 0;
            state->score += 30; // Add 30 points for preventing hacking
            furi_hal_vibro_on(true); // Short vibrate for success
            furi_delay_ms(100);
            furi_hal_vibro_on(false);
        }
        state->patching = false;
    }

    // Randomly spawn warnings or packets - limit to 2 active hack attempts
    if(rand() % 100 < 1) { // 1% chance for hack warnings
        // Count active warnings/hacking
        int active_hacks = 0;
        for(int i = 0; i < 4; i++) {
            if(state->hacking[i] || state->warn_start[i]) {
                active_hacks++;
            }
        }

        // Only spawn new warning if we have fewer than 2 active hack attempts
        if(active_hacks < 2) {
            // Find a non-dead computer to target
            int attempts = 0;
            int target;
            do {
                target = rand() % 4;
                attempts++;
                // Prevent infinite loop if all computers are nearly dead
                if(attempts > 10) break;
            } while(state->computer_dead[target] || state->hacking[target] ||
                    state->warn_start[target]);

            if(attempts <= 10 && !state->computer_dead[target] && !state->hacking[target] &&
               !state->warn_start[target]) {
                state->warn_start[target] = now;
                furi_hal_vibro_on(true); // Short vibrate for warning
                furi_delay_ms(100);
                furi_hal_vibro_on(false);
            }
        }
    } else {
        // Existing packet spawn logic continues here
        // ...existing code...
    }

    // Check DDOS - enhanced check for both total packets and per-computer overflow
    int total_packets = 0;
    bool computer_overflow = false;

    for(int i = 0; i < 4; i++) {
        total_packets += state->packets[i];
        // Check if any single computer has too many packets
        if(state->packets[i] >= COMPUTER_OVERFLOW) {
            computer_overflow = true;
            break;
        }
    }

    // Game over if either condition is met
    if(total_packets >= DDOS_LIMIT || computer_overflow) {
        state->game_over = true;
        state->game_over_reason = GAME_OVER_DDOS;

        // Check for high score
        if(state->score > state->high_score) {
            state->high_score = state->score;
            state->new_high_score = true;
            save_high_score(state);
        }

        // Vibrate to indicate game over
        furi_hal_vibro_on(true);
        furi_delay_ms(500);
        furi_hal_vibro_on(false);
    }

    // Update upcoming packets
    update_upcoming_packets(state);
}

// Main app entry
int32_t system_defender_app(void* p) {
    UNUSED(p);

    // Setup event queue
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    // Setup game state
    GameState* game_state = malloc(sizeof(GameState));
    game_init(game_state);

    // Setup GUI
    Gui* gui = furi_record_open(RECORD_GUI);
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, game_state);
    view_port_input_callback_set(view_port, input_callback, event_queue);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // Main loop
    InputEvent event;
    bool running = true;

    while(running) {
        if(furi_message_queue_get(event_queue, &event, 100) == FuriStatusOk) {
            if(event.type == InputTypePress) {
                // Handle input based on current state
                if(game_state->current_state == GAME_STATE_TITLE) {
                    // Title screen inputs
                    if(event.key == InputKeyOk) {
                        game_state->current_state = GAME_STATE_MENU; // Go to menu instead of play
                    } else if(event.key == InputKeyBack) {
                        running = false; // Quit from title screen
                    }
                } else if(game_state->current_state == GAME_STATE_MENU) {
                    // Menu screen inputs
                    if(event.key == InputKeyUp && game_state->menu_selection > 0) {
                        game_state->menu_selection--;
                    } else if(event.key == InputKeyDown && game_state->menu_selection < 2) {
                        game_state->menu_selection++;
                    } else if(event.key == InputKeyOk) {
                        if(game_state->menu_selection == 0) {
                            game_state->current_state = GAME_STATE_PLAY;
                            reset_game_state(game_state); // Reset game when starting
                        } else if(game_state->menu_selection == 1) {
                            game_state->current_state = GAME_STATE_HELP;
                            game_state->help_page = 0; // Start at first help page
                        } else if(game_state->menu_selection == 2) {
                            running = false; // Quit
                        }
                    } else if(event.key == InputKeyBack) {
                        game_state->current_state = GAME_STATE_TITLE;
                    }
                } else if(game_state->current_state == GAME_STATE_HELP) {
                    // Help screen inputs
                    if(event.key == InputKeyOk) {
                        if(game_state->help_page == 0) {
                            game_state->help_page = 1; // Go to next page
                        } else {
                            game_state->current_state = GAME_STATE_MENU; // Return to menu
                        }
                    } else if(event.key == InputKeyBack) {
                        game_state->current_state = GAME_STATE_MENU;
                    }
                } else if(game_state->current_state == GAME_STATE_PLAY) {
                    // Game screen inputs
                    if(game_state->game_over) {
                        if(event.key == InputKeyBack) {
                            game_state->current_state = GAME_STATE_TITLE;
                            reset_game_state(game_state);
                        }
                    } else {
                        // Don't allow movement or actions if player is patching
                        if(game_state->patching) {
                            // Only allow back button to exit game while patching
                            if(event.key == InputKeyBack) {
                                game_state->current_state = GAME_STATE_TITLE;
                            }
                        } else {
                            switch(event.key) {
                            case InputKeyUp:
                                game_state->player_pos =
                                    (game_state->player_pos == 0 || game_state->player_pos == 1) ?
                                        game_state->player_pos :
                                        game_state->player_pos - 2;
                                break;
                            case InputKeyDown:
                                game_state->player_pos =
                                    (game_state->player_pos == 2 || game_state->player_pos == 3) ?
                                        game_state->player_pos :
                                        game_state->player_pos + 2;
                                break;
                            case InputKeyLeft:
                                game_state->player_pos =
                                    (game_state->player_pos == 0 || game_state->player_pos == 2) ?
                                        game_state->player_pos :
                                        game_state->player_pos - 1;
                                break;
                            case InputKeyRight:
                                game_state->player_pos =
                                    (game_state->player_pos == 1 || game_state->player_pos == 3) ?
                                        game_state->player_pos :
                                        game_state->player_pos + 1;
                                break;
                            case InputKeyOk:
                                if(game_state->hacking[game_state->player_pos] &&
                                   !game_state->patching) {
                                    game_state->patching = true;
                                    game_state->patch_start = furi_get_tick();
                                } else if(game_state->packets[game_state->player_pos] > 0) {
                                    game_state->packets
                                        [game_state->player_pos]--; // Correctly remove packet
                                    game_state->score +=
                                        10; // Increment score by 10 for each packet delivered

                                    // Start packet animation
                                    game_state->packet_animation = true;
                                    game_state->anim_start = furi_get_tick();
                                    game_state->anim_from = game_state->player_pos;

                                    furi_hal_vibro_on(true); // Vibrate for packet accept
                                    furi_delay_ms(100);
                                    furi_hal_vibro_on(false);
                                }
                                break;
                            case InputKeyBack:
                                game_state->current_state = GAME_STATE_TITLE; // Return to title
                                break;
                            default:
                                break;
                            }
                        }
                    }
                }
            }
        }

        // Only update game logic if we're in play mode
        if(game_state->current_state == GAME_STATE_PLAY) {
            update_game(game_state);
        }

        view_port_update(view_port);
    }

    // Cleanup
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    free(game_state);

    return 0;
}
