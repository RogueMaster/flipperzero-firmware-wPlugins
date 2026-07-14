#include "ui.h"

// Draw title screen
void draw_title_screen(Canvas* canvas) {
    canvas_clear(canvas);

    // Display title graphics - using correct icon names
    canvas_draw_icon(canvas, 9, 10, &I_title_network);
    canvas_draw_icon(canvas, 10, 39, &I_title_defender);
    canvas_draw_icon(canvas, 94, 30, &I_player_left);
    canvas_draw_icon(canvas, 28, 31, &I_pc_monitor);
}

// Draw menu screen with selection indicator
void draw_menu_screen(Canvas* canvas, int selection) {
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
void draw_help_screen(Canvas* canvas, int page) {
    canvas_clear(canvas);

    canvas_set_font(canvas, FontSecondary);

    if(page == 0) {
        // Page 1: Computers and packets
        canvas_draw_str(canvas, 5, 10, "COMPUTERS & PACKETS:");

        // Computer icon
        canvas_draw_icon(canvas, 6, 18, &I_pc_monitor);
        canvas_draw_str(canvas, 20, 24, "- Computer system");

        // Packet icon
        canvas_draw_icon(canvas, 10, 32, &I_packet);
        canvas_draw_str(canvas, 20, 38, "- Data packet");
        canvas_draw_str(canvas, 20, 46, "- Accept new packets");
        canvas_draw_str(canvas, 20, 54, "- Max 4 per computer");

    } else if(page == 1) {
        // Page 2: Hacking attempts
        canvas_draw_str(canvas, 5, 10, "HACKING ATTEMPTS:");

        // Normal PC
        canvas_draw_icon(canvas, 6, 18, &I_pc);
        canvas_draw_str(canvas, 20, 24, "- Safe computer");

        // Warning PC and PC Using
        canvas_draw_str(canvas, 20, 34, "  Flashing = hack warning!");

        // Bug icon for active hack
        canvas_draw_icon(canvas, 6, 48, &I_bug);
        canvas_draw_str(canvas, 20, 54, "- Active hack");

    } else {
        // Page 3: Gameplay instructions
        canvas_draw_str(canvas, 5, 10, "HOW TO PLAY:");
        canvas_draw_str(canvas, 5, 20, "- Move with D-pad");
        canvas_draw_str(canvas, 5, 30, "- Press OK to accept packet");
        canvas_draw_str(canvas, 5, 40, "- Hold OK for 3s to patch");
        canvas_draw_str(canvas, 5, 50, "- Keep packets below limit");
        canvas_draw_str(canvas, 5, 60, "- Max 16 total packets");
    }

    // Small page indicator at bottom right
    char page_indicator[16]; // Increased buffer size from 8 to 16
    snprintf(page_indicator, sizeof(page_indicator), "%d/3", page + 1);
    canvas_draw_str(canvas, SCREEN_WIDTH - 20, SCREEN_HEIGHT - 2, page_indicator);
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

// Draw callback for GUI
void draw_callback(Canvas* canvas, void* ctx) {
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
            canvas_draw_str(canvas, 120, 45, "!!!");
        }

        canvas_draw_str(canvas, 15, 55, "Press BACK to restart");
        return;
    }

    // Draw server in center using server sprite instead of pc
    canvas_draw_icon(canvas, SERVER_POSITION.x - 8, SERVER_POSITION.y - 8, &I_server);

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
        } else if(state->warn_start[i]) {
            // Flash between normal PC and PC using icons for warning
            uint32_t now = furi_get_tick();
            if((now - state->warn_start[i]) / 500 % 2 == 0) {
                canvas_draw_icon(canvas, x - 8, y - 8, &I_pc_using);
            } else {
                canvas_draw_icon(canvas, x - 8, y - 8, &I_pc);
            }
        } else {
            // Use normal pc for healthy computers
            canvas_draw_icon(canvas, x - 8, y - 8, &I_pc);
        }

        // Skip drawing packets and hacking warnings on dead computers
        if(!state->computer_dead[i]) {
            // Only show the bug icon for active hacks - remove the warning "!" display
            if(state->hacking[i]) {
                // Use bug icon instead of "!!!" text for active hack
                canvas_draw_icon(canvas, x - 6, y - 14, &I_bug);
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
void input_callback(InputEvent* input, void* ctx) {
    FuriMessageQueue* queue = (FuriMessageQueue*)ctx;
    furi_message_queue_put(queue, input, FuriWaitForever);
}
