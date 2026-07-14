#include "game_state.h"

// Define system position constants
const Position SYSTEM_POSITIONS[4] = {
    {32, 21}, // Top-left (moved down by 5px)
    {96, 21}, // Top-right (moved down by 5px)
    {32, 53}, // Bottom-left (moved down by 5px)
    {96, 53} // Bottom-right (moved down by 5px)
};

// Adjust player positions to be inside the computer boxes
const Position PLAYER_POSITIONS[4] = {
    {32, 21}, // Inside top-left system
    {96, 21}, // Inside top-right system
    {32, 53}, // Inside bottom-left system
    {96, 53} // Inside bottom-right system
};

// Server position (center)
const Position SERVER_POSITION = {64, 37}; // Center server (moved down by 5px)

// Reset game state for new game
void reset_game_state(GameState* state) {
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
void game_init(GameState* state) {
    state->current_state = GAME_STATE_TITLE;
    state->high_score = 0;
    state->new_high_score = false;

    // Load high score from storage
    load_high_score(state);

    reset_game_state(state);
}

// Function to load high score from storage
bool load_high_score(GameState* state) {
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
bool save_high_score(GameState* state) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool result = false;

    // Ensure directory exists
    storage_common_mkdir(storage, APP_DATA_PATH(""));

    // Debug log to show save attempt
    FURI_LOG_I("NetDefender", "Attempting to save high score: %d", state->high_score);

    // Create the file and write high score
    if(storage_file_open(file, HISCORE_FILENAME, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint16_t bytes_written = storage_file_write(file, &state->high_score, sizeof(int));
        result = (bytes_written == sizeof(int));
        FURI_LOG_I(
            "NetDefender",
            "Saved high score: %d, bytes_written: %d, result: %d",
            state->high_score,
            bytes_written,
            result);
    } else {
        FURI_LOG_E("NetDefender", "Failed to open file for saving high score");
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return result;
}
