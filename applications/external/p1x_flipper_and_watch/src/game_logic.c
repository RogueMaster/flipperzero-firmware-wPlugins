#include "game_logic.h"
#include "game_state.h"

// Fix the calculate_spawn_interval function to accelerate instead of slow down
uint32_t calculate_spawn_interval(GameState* state) {
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

// Update upcoming packets data in the game state
void update_upcoming_packets(GameState* state) {
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

// Main game update function
void update_game(GameState* state) {
    // Only run game logic if we're in play state
    if(state->current_state != GAME_STATE_PLAY) return;

    if(state->game_over) return;

    uint32_t now = furi_get_tick();

    // Update high score immediately if current score exceeds it
    if(state->score > state->high_score) {
        state->high_score = state->score;
        state->new_high_score = true;
    }

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
