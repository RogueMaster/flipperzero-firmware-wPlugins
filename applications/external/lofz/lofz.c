#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <string.h>
#include <storage/storage.h>

// --- Constants and Macros ---
#define GRID_SIZE              4
#define GRID_COUNT             16
#define CELL_W                 20
#define CELL_H                 12
#define PLAYER_CELL_W          16
#define PLAYER_CELL_H          14
#define BOARD_TOP              0
#define BOARD_LEFT             0
#define BOARD_W                (GRID_SIZE * CELL_W)
#define BOARD_H                (GRID_SIZE * CELL_H)
#define MASCOT_X               92
#define MASCOT_Y               14
#define PLAYER_CELL_X          71
#define PLAYER_CELL_Y          0
#define LINE_HEIGHT            11
#define POPUP_WIDTH            100
#define POPUP_X                14
#define POPUP_Y                48
#define SCORE_BOX_X            94
#define SCORE_BOX_Y            48
#define SCORE_BOX_W            32
#define SCORE_BOX_H            14
#define SCROLL_SPEED           650
#define CRAWL_SPEED            40
#define CREDITS_COOLDOWN_MS    180000
#define CREDITS_MOVE_THRESHOLD 4
#define AUDIO_TOGGLE_TIMEOUT   1350
#define LEADERBOARD_ENTRIES    10
#define LEADERBOARD_FILE       "/ext/lofz_leaderboard.txt"
#define MASCOT_MOVE_INTERVAL   200

typedef enum {
    GameState_Loading,
    GameState_PressToPlay,
    GameState_IntroCrawl,
    GameState_IntroFade,
    GameState_PlayerTurn,
    GameState_FlipperTurn,
    GameState_FlipperPopup,
    GameState_Finished,
    GameState_ExitScreen,
    GameState_LostNulls,
    GameState_LostFade,
    GameState_LostMenu,
    GameState_WinScreen,
    GameState_Credits,
    GameState_SecretCode,
    GameState_DeadScreen,
    GameState_WinFade,
    GameState_MischiefScreen,
    GameState_WinMenu
} GameState;

typedef struct {
    int8_t grid[GRID_COUNT];
    uint8_t player_cursor;
    uint8_t flipper_cursor;
    bool solved;
    GameState state;
    uint32_t popup_timer;
    uint8_t popup_phrase;
    uint32_t scroll_start_tick;
    int scroll_offset;
    uint32_t state_tick;
    int intro_crawl_y;
    int fade_level;
    int menu_sel;
    uint8_t flipper_move_index;
    uint32_t right_key_count;
    uint32_t last_right_key_tick;
    uint32_t left_key_count;
    uint32_t last_left_key_tick;
    uint32_t back_key_count;
    uint32_t last_back_key_tick;
    uint32_t up_key_count;
    uint32_t last_up_key_tick;
    int mascot_bounce_offset;
    uint32_t bounce_start_tick;
    bool bounce_up;
    int8_t saved_grid[GRID_COUNT];
    uint8_t saved_player_cursor;
    uint8_t saved_flipper_cursor;
    bool saved_solved;
    uint32_t credits_access_count;
    uint32_t last_credits_tick;
    uint32_t user_move_count;
    uint32_t total_flips;
    uint32_t wins;
    bool audio_enabled;
    uint32_t score;
    uint8_t secret_code_index;
    uint8_t input_sequence[9];
    uint8_t input_sequence_length;
    int mascot_x;
    int mascot_y;
    uint32_t last_mascot_move_tick;
} LOFZGame;

static const uint8_t flipper_move_sequence[] = {0, 1, 3, 0, 2, 4, 1, 0};
#define FLIPPER_SEQUENCE_COUNT (sizeof(flipper_move_sequence) / sizeof(flipper_move_sequence[0]))

// --- Evil Sith Flipper Mascot Bitmap (16x16 px, 1bpp) ---
static const uint8_t sith_flipper_mascot[32] = {
    0b00000111, 0b11100000, 0b00011111, 0b11111000, 0b00111111, 0b11111100, 0b01110000, 0b00001110,
    0b01100000, 0b00000110, 0b11000111, 0b11100011, 0b11001111, 0b11110011, 0b11111111, 0b11111111,
    0b11111111, 0b11111111, 0b11111110, 0b01111111, 0b01111100, 0b00111110, 0b00111000, 0b00011100,
    0b00010000, 0b00001000, 0b00100000, 0b00000100, 0b01000011, 0b11000010, 0b00011111, 0b11111000,
};

// --- Flipper AI phrases ---
static const char* flipper_phrases[] = {
    "Flipper is Playing his Move",
    "Analyzing Board...",
    "Flipper's Turn!",
    "Diffusion in Progress",
    "Watch My Move!",
    "  WMM!  ",
    "Flipper is Thinking...",
    "Hold up, its my move",
    "Wait...Oh yeah!",
    " Hmmm... ",
    "That's a good one",
    "I got this",
    "Please let me move",
    "My TURN!"};
#define FLIPPER_PHRASE_COUNT (sizeof(flipper_phrases) / sizeof(flipper_phrases[0]))

// --- Intro and Credits Lines ---
static const char* intro_lines[] = {
    "  ",
    "LOFZ",
    "Lights Out Flipper Zero",
    "  ",
    "  ",
    "A long time ago in a",
    "galaxy far, far away....",
    "  ",
    "  ",
    "The evil ",
    "Flipper Zero, Sith Lord,",
    "  ",
    "has covered the ",
    "galaxy in light!",
    "  ",
    "No one has slept in ages",
    "  ",
    "  ",
    "You are the last Jedi,",
    "  ",
    "your mission is ",
    "to turn the",
    "lights out ",
    "and vanquish Flipper!",
    "  ",
    "  ",
    "Use D-pad to move. ",
    "OK to select.",
    "  ",
    "Back button 2x+ Exits",
    "  ",
    "Game Goal:",
    "Turn all boxes dark.",
    "  ",
    "If all tiles are lit, the Sith wins.",
    "  ",
    "May the Force be with You",
    "  ",
    "Press OK to continue",
    "  ",
};
#define INTRO_LINES_COUNT (sizeof(intro_lines) / sizeof(intro_lines[0]))

static const char* credits_lines[] = {
    "  ",
    "LOFZ Credits",
    "  ",
    "Created by 3DPihl",
    "Inspired by Star Wars",
    "Powered by Flipper Zero",
    "  ",
    "Assisted by:",
    "Github",
    "Copilot AI",
    "Grok AI",
    "xTwitter",
    "  ",
    ":Special Thanks:",
    "Thanks to the ",
    "Flipper community",
    "for your support",
    "  ",
    "May the Force be with You",
    "  ",
    "  ",
    "DUDUDUDUO",
    "  ",
    "Press OK to continue",
    "  ",
};
#define CREDITS_LINES_COUNT (sizeof(credits_lines) / sizeof(credits_lines[0]))

static const char* mischief_lines[] = {
    "  ",
    "I solemnly swear",
    "I'm up to",
    "to no good!",
    "  ",
    "  ",
    "UUUDU_UD_",
    "  ",
    "...L or R...",
    "  ",
};
#define MISCHIEF_LINES_COUNT (sizeof(mischief_lines) / sizeof(mischief_lines[0]))

static const char* dead_lines[] = {
    "  ",
    "You Died!",
    "Flipper Wins!",
    "  ",
    "The Dark Side prevails",
    "  ",
    "Press OK to continue",
    "  ",
};
#define DEAD_LINES_COUNT (sizeof(dead_lines) / sizeof(dead_lines[0]))

static const char* win_lines[] = {
    "  ",
    "Victory!",
    "The Jedi Triumph!",
    "  ",
    "The galaxy is dark",
    "  ",
    "  ",
    "Press U 5x to See",
    "  ",
};
#define WIN_LINES_COUNT (sizeof(win_lines) / sizeof(win_lines[0]))

static const char* secret_code_lines[] = {
    "  ",
    "Secret Codes:",
    "Win: UUUDURUDR",
    "Dead: UUUDULUDL",
    "Mischief: DUDUDUDUO",
    "This: LLLLLLLLL",
    "Board: UUUUUUUUU",
    "  ",
    "Press OK to continue",
    "  ",
};
#define SECRET_CODE_LINES_COUNT (sizeof(secret_code_lines) / sizeof(secret_code_lines[0]))

typedef struct {
    uint32_t wins;
    uint32_t presses;
} LeaderboardEntry;

static LeaderboardEntry leaderboard[LEADERBOARD_ENTRIES];

// --- Secret Code Sequences ---
static const InputKey dead_sequence[] = {
    InputKeyUp,
    InputKeyUp,
    InputKeyUp,
    InputKeyDown,
    InputKeyUp,
    InputKeyLeft,
    InputKeyUp,
    InputKeyDown,
    InputKeyLeft};
static const InputKey win_sequence[] = {
    InputKeyUp,
    InputKeyUp,
    InputKeyUp,
    InputKeyDown,
    InputKeyUp,
    InputKeyRight,
    InputKeyUp,
    InputKeyDown,
    InputKeyRight};
static const InputKey mischief_sequence[] = {
    InputKeyDown,
    InputKeyUp,
    InputKeyDown,
    InputKeyUp,
    InputKeyDown,
    InputKeyUp,
    InputKeyDown,
    InputKeyUp,
    InputKeyOk};
#define SEQUENCE_LENGTH 9

// --- Secret Code Display ---
static const char* secret_codes[] = {
    "UUUDURUDR", // Win screen
    "UUUDULUDL", // Dead screen
    "DUDUDUDUO", // Mischief screen
    "", // Black screen
};
#define SECRET_CODE_COUNT 4

// --- Drawing Helpers ---
static void draw_rounded_box(Canvas* canvas, int x, int y, int w, int h, int thickness) {
    canvas_set_color(canvas, ColorBlack);
    for(int i = 0; i < thickness; i++) {
        canvas_draw_rframe(canvas, x + i, y + i, w - 2 * i, h - 2 * i, 2);
    }
}

static void draw_mascot(Canvas* canvas, int x, int y, int bounce_offset, bool bounce_up) {
    int offset = bounce_up ? -bounce_offset : bounce_offset;
    for(int row = 0; row < 16; row++) {
        uint16_t row_bits = (sith_flipper_mascot[row * 2] << 8) | sith_flipper_mascot[row * 2 + 1];
        for(int col = 0; col < 16; col++) {
            if(row_bits & (0x8000 >> col)) canvas_draw_dot(canvas, x + col, y + row + offset);
        }
    }
}

static void draw_crawl_text(Canvas* canvas, int y, const char* const* lines, int line_count) {
    canvas_set_font(canvas, FontPrimary);
    int valid_lines = 0;
    for(int i = 0; i < line_count; i++) {
        if(lines[i][0] != '\0') valid_lines++;
    }
    int valid_idx = 0;
    for(int i = 0; i < line_count; i++) {
        if(lines[i][0] == '\0') continue;
        int y_pos = y + valid_idx * LINE_HEIGHT;
        if(y_pos < -LINE_HEIGHT || y_pos > 64) {
            valid_idx++;
            continue;
        }
        float fade = 1.0f;
        if(y_pos < 10)
            fade = (float)y_pos / 10.0f;
        else if(y_pos > 54)
            fade = (float)(64 - y_pos) / 10.0f;
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, y_pos - 2, 128, LINE_HEIGHT - 1);
        if(fade > 0.0f) {
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str_aligned(canvas, 64, y_pos, AlignCenter, AlignCenter, lines[i]);
        }
        valid_idx++;
    }
    canvas_set_color(canvas, ColorBlack);
}

static void draw_scrolling_text(
    Canvas* canvas,
    int x,
    int y,
    const char* text,
    int width,
    LOFZGame* game,
    bool update_scroll) {
    canvas_set_font(canvas, FontPrimary);
    int text_len = strlen(text);
    int chars_fit = width / 7;
    int scroll_limit = text_len - chars_fit + 1;
    if(scroll_limit < 0) scroll_limit = 0;
    int scroll = game->scroll_offset;
    if(scroll > scroll_limit) scroll = scroll_limit;
    char buf[64];
    if(scroll < text_len) {
        strncpy(buf, text + scroll, chars_fit);
        buf[chars_fit] = '\0';
    } else {
        buf[0] = '\0';
    }
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, x - 2, y - 2, width + 4, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, x, y, buf);
    if(update_scroll && (furi_get_tick() - game->scroll_start_tick > SCROLL_SPEED)) {
        game->scroll_start_tick = furi_get_tick();
        game->scroll_offset++;
        if(game->scroll_offset > scroll_limit) game->scroll_offset = 0;
    }
}

static void draw_3d_board(Canvas* canvas, LOFZGame* game) {
    canvas_set_color(canvas, ColorWhite);
    int top_left_x = 0, top_left_y = 0;
    int top_right_x = 64, top_right_y = 0;
    int bottom_left_x = 0, bottom_left_y = 64;
    int bottom_right_x = 85, bottom_right_y = 64;
    canvas_draw_line(canvas, top_left_x, top_left_y, top_right_x, top_right_y);
    canvas_draw_line(canvas, top_left_x, top_left_y, bottom_left_x, bottom_left_y);
    canvas_draw_line(canvas, top_right_x, top_right_y, bottom_right_x, bottom_right_y);
    canvas_draw_line(canvas, bottom_left_x, bottom_left_y, bottom_right_x, bottom_right_y);

    for(uint8_t row = 0; row < GRID_SIZE; row++) {
        for(uint8_t col = 0; col < GRID_SIZE; col++) {
            int idx = row * GRID_SIZE + col;
            float t = (float)row / GRID_SIZE;
            int x1 = top_left_x + (int)((bottom_left_x - top_left_x) * t);
            int x2 = top_right_x + (int)((bottom_right_x - top_right_x) * t);
            int y = top_left_y + (int)((bottom_left_y - top_left_y) * t);
            int cell_x = x1 + (int)((x2 - x1) * (float)col / GRID_SIZE);
            int cell_x_next = x1 + (int)((x2 - x1) * (float)(col + 1) / GRID_SIZE);
            int cell_y = y;
            int cell_y_next =
                top_left_y + (int)((bottom_left_y - top_left_y) * (float)(row + 1) / GRID_SIZE);
            int cell_w = cell_x_next - cell_x;
            int cell_h = cell_y_next - cell_y;

            bool is_cursor = (idx == game->player_cursor && game->state == GameState_PlayerTurn);

            if(game->grid[idx] != 0) {
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_rbox(canvas, cell_x + 1, cell_y + 1, cell_w - 2, cell_h - 2, 2);
            } else {
                canvas_set_color(canvas, ColorWhite);
                canvas_draw_rframe(canvas, cell_x + 1, cell_y + 1, cell_w - 2, cell_h - 2, 2);
            }

            if(is_cursor) {
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_rframe(canvas, cell_x, cell_y, cell_w, cell_h, 2);
                canvas_set_color(canvas, game->grid[idx] ? ColorWhite : ColorBlack);
                canvas_draw_str(
                    canvas,
                    cell_x + cell_w / 2 - 2,
                    cell_y + cell_h / 2 + 2,
                    game->grid[idx] ? "-" : "+");
            }
        }
    }

    bool is_player_cell =
        (game->player_cursor == GRID_COUNT && game->state == GameState_PlayerTurn);
    if(game->grid[GRID_COUNT - 1] != 0) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rbox(
            canvas, PLAYER_CELL_X + 1, PLAYER_CELL_Y + 1, PLAYER_CELL_W - 2, PLAYER_CELL_H - 2, 2);
    } else {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_rframe(
            canvas, PLAYER_CELL_X + 1, PLAYER_CELL_Y + 1, PLAYER_CELL_W - 2, PLAYER_CELL_H - 2, 2);
    }
    if(is_player_cell) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rframe(canvas, PLAYER_CELL_X, PLAYER_CELL_Y, PLAYER_CELL_W, PLAYER_CELL_H, 2);
        canvas_set_color(canvas, game->grid[GRID_COUNT - 1] ? ColorWhite : ColorBlack);
        canvas_draw_str(
            canvas,
            PLAYER_CELL_X + PLAYER_CELL_W / 2 - 2,
            PLAYER_CELL_Y + PLAYER_CELL_H / 2 + 2,
            game->grid[GRID_COUNT - 1] ? "-" : "+");
    }

    canvas_set_color(canvas, ColorBlack);
}

static bool is_lost_nulls(LOFZGame* game) {
    for(uint8_t i = 0; i < GRID_COUNT - 1; i++) // Exclude offset block
        if(game->grid[i] == 0) return false;
    return true;
}

static void lofz_update(LOFZGame* game, uint8_t idx) {
    static const int8_t dx[4] = {0, -1, 0, 1};
    static const int8_t dy[4] = {-1, 0, 1, 0};
    uint8_t x = idx % GRID_SIZE, y = idx / GRID_SIZE;
    game->grid[idx] = (game->grid[idx] == 0) ? 1 : 0;

    if(idx == GRID_COUNT - 1) {
        game->grid[5] = (game->grid[5] == 0) ? 1 : 0;
        game->grid[6] = (game->grid[6] == 0) ? 1 : 0;
        game->grid[9] = (game->grid[9] == 0) ? 1 : 0;
        game->grid[10] = (game->grid[10] == 0) ? 1 : 0;
    } else {
        for(int n = 0; n < 4; n++) {
            int nx = x + dx[n], ny = y + dy[n];
            if(nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE) {
                game->grid[ny * GRID_SIZE + nx] = (game->grid[ny * GRID_SIZE + nx] == 0) ? 1 : 0;
            } else if(idx >= 5 && idx <= 10 && (idx == 5 || idx == 6 || idx == 9 || idx == 10)) {
                if(n == (rand() % 4)) {
                    game->grid[GRID_COUNT - 1] = (game->grid[GRID_COUNT - 1] == 0) ? 1 : 0;
                }
            }
        }
    }

    if(game->state == GameState_PlayerTurn) {
        game->score++;
        game->user_move_count++;
        game->total_flips++;
        if(game->score > 999) {
            game->score = 1;
        }
    }
}

static bool lofz_is_solved(LOFZGame* game) {
    for(uint8_t i = 0; i < GRID_COUNT - 1; i++) // Exclude offset block
        if(game->grid[i] != 0) return false;
    return true;
}

static void lofz_flipper_move(LOFZGame* game) {
    uint8_t best_idx = 0;
    int min_dist = 999;
    for(uint8_t i = 0; i < GRID_COUNT; i++) {
        if(game->grid[i] != 0) {
            int px = game->player_cursor % GRID_SIZE, py = game->player_cursor / GRID_SIZE;
            int fx = i % GRID_SIZE, fy = i / GRID_SIZE;
            int dist = abs(px - fx) + abs(py - fy);
            if(dist < min_dist) {
                best_idx = i;
                min_dist = dist;
            }
        }
    }
    game->flipper_cursor = best_idx;
    lofz_update(game, best_idx);
    game->bounce_start_tick = furi_get_tick();
    game->mascot_bounce_offset = 6;
    game->bounce_up = (rand() % 2) == 0;
}

static void save_leaderboard(LOFZGame* game) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, LEADERBOARD_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        LeaderboardEntry new_entry = {game->wins, game->total_flips};
        int insert_idx = -1;
        for(int i = 0; i < LEADERBOARD_ENTRIES; i++) {
            if(leaderboard[i].wins == 0 || (new_entry.wins > leaderboard[i].wins) ||
               (new_entry.wins == leaderboard[i].wins &&
                new_entry.presses >= leaderboard[i].presses)) {
                insert_idx = i;
                break;
            }
        }
        if(insert_idx >= 0) {
            for(int i = LEADERBOARD_ENTRIES - 1; i > insert_idx; i--) {
                leaderboard[i] = leaderboard[i - 1];
            }
            leaderboard[insert_idx] = new_entry;
        }
        for(int i = 0; i < LEADERBOARD_ENTRIES; i++) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%lu:%lu\n", leaderboard[i].wins, leaderboard[i].presses);
            storage_file_write(file, buf, strlen(buf));
            storage_file_write(file, "  \n", 3);
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void load_leaderboard(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    memset(leaderboard, 0, sizeof(leaderboard));
    if(storage_file_open(file, LEADERBOARD_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buffer[64];
        uint32_t bytes_read;
        int idx = 0;
        FuriString* line = furi_string_alloc();
        while((bytes_read = storage_file_read(file, buffer, sizeof(buffer) - 1)) > 0 &&
              idx < LEADERBOARD_ENTRIES) {
            buffer[bytes_read] = '\0';
            furi_string_set(line, buffer);
            if(furi_string_empty(line) || furi_string_cmp_str(line, "  ") == 0) continue;
            uint32_t wins, presses;
            if(sscanf(furi_string_get_cstr(line), "%lu:%lu", &wins, &presses) == 2) {
                leaderboard[idx].wins = wins;
                leaderboard[idx].presses = presses;
                idx++;
            }
        }
        furi_string_free(line);
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// --- Input Handler ---
static void lofz_input(InputEvent* event, void* ctx) {
    LOFZGame* game = (LOFZGame*)ctx;
    if(game->state == GameState_Loading || game->state == GameState_IntroFade ||
       game->state == GameState_LostFade || game->state == GameState_WinFade)
        return;

    if(event->type == InputTypePress) {
        if(game->state == GameState_SecretCode || game->state == GameState_DeadScreen ||
           game->state == GameState_MischiefScreen) {
            if(event->key == InputKeyOk) {
                game->state = GameState_IntroCrawl;
                game->intro_crawl_y = 64;
                game->state_tick = furi_get_tick();
                game->left_key_count = 0;
                return;
            }
        }

        if(event->key == InputKeyUp &&
           (game->state == GameState_PlayerTurn || game->state == GameState_PressToPlay ||
            game->state == GameState_IntroCrawl || game->state == GameState_WinScreen ||
            game->state == GameState_LostMenu || game->state == GameState_WinMenu)) {
            if(furi_get_tick() - game->last_up_key_tick <= 1800) {
                game->up_key_count++;
                if(game->up_key_count >= 10) {
                    game->state = GameState_IntroCrawl;
                    game->intro_crawl_y = 64;
                    game->state_tick = furi_get_tick();
                    game->up_key_count = 0;
                    return;
                }
            } else {
                game->up_key_count = 1;
            }
            game->last_up_key_tick = furi_get_tick();
        } else {
            game->up_key_count = 0;
        }

        if(event->key == InputKeyLeft && game->state == GameState_Credits) {
            if(furi_get_tick() - game->last_left_key_tick <= 1800) {
                game->left_key_count++;
                if(game->left_key_count >= 9) {
                    game->state = GameState_SecretCode;
                    game->secret_code_index = (game->secret_code_index + 1) % SECRET_CODE_COUNT;
                    game->intro_crawl_y = 64;
                    game->state_tick = furi_get_tick();
                    game->left_key_count = 0;
                    return;
                }
            } else {
                game->left_key_count = 1;
            }
            game->last_left_key_tick = furi_get_tick();
        } else if(
            event->key == InputKeyLeft &&
            (game->state == GameState_PlayerTurn || game->state == GameState_PressToPlay ||
             game->state == GameState_IntroCrawl || game->state == GameState_WinScreen ||
             game->state == GameState_LostMenu || game->state == GameState_WinMenu)) {
            if(furi_get_tick() - game->last_left_key_tick <= 1800) {
                game->left_key_count++;
                if(game->left_key_count >= 3) {
                    game->audio_enabled = !game->audio_enabled;
                    game->left_key_count = 0;
                }
            } else {
                game->left_key_count = 1;
            }
            game->last_left_key_tick = furi_get_tick();
        } else {
            game->left_key_count = 0;
        }

        if(game->state == GameState_Credits) {
            if(game->input_sequence_length < SEQUENCE_LENGTH) {
                game->input_sequence[game->input_sequence_length++] = event->key;
            }
            if(event->key == InputKeyOk || game->input_sequence_length >= SEQUENCE_LENGTH) {
                bool matched = false;
                if(game->input_sequence_length == SEQUENCE_LENGTH) {
                    if(memcmp(game->input_sequence, dead_sequence, sizeof(dead_sequence)) == 0) {
                        game->state = GameState_DeadScreen;
                        game->intro_crawl_y = 64;
                        game->state_tick = furi_get_tick();
                        matched = true;
                    } else if(memcmp(game->input_sequence, win_sequence, sizeof(win_sequence)) == 0) {
                        game->state = GameState_WinScreen;
                        game->intro_crawl_y = 64;
                        game->state_tick = furi_get_tick();
                        matched = true;
                    } else if(
                        memcmp(
                            game->input_sequence, mischief_sequence, sizeof(mischief_sequence)) ==
                        0) {
                        game->state = GameState_MischiefScreen;
                        game->intro_crawl_y = 64;
                        game->state_tick = furi_get_tick();
                        matched = true;
                    }
                }
                if(!matched && event->key == InputKeyOk) {
                    game->state = GameState_IntroCrawl;
                    game->intro_crawl_y = 64;
                    game->state_tick = furi_get_tick();
                    memcpy(game->saved_grid, game->grid, sizeof(game->grid));
                    game->saved_player_cursor = game->player_cursor;
                    game->saved_flipper_cursor = game->flipper_cursor;
                    game->saved_solved = game->solved;
                }
                game->input_sequence_length = 0;
                memset(game->input_sequence, 0, sizeof(game->input_sequence));
                return;
            }
            return;
        }

        if(game->state == GameState_PressToPlay && event->key == InputKeyOk) {
            game->state = GameState_IntroCrawl;
            game->state_tick = furi_get_tick();
            return;
        }
        if(game->state == GameState_IntroCrawl && event->key == InputKeyOk) {
            game->state = GameState_IntroFade;
            game->fade_level = 0;
            game->state_tick = furi_get_tick();
            return;
        }
        if(game->state == GameState_LostMenu) {
            if(event->key == InputKeyUp || event->key == InputKeyRight) {
                game->menu_sel = 0; // Yes
            } else if(event->key == InputKeyDown || event->key == InputKeyLeft) {
                game->menu_sel = 1; // No
            } else if(event->key == InputKeyOk) {
                if(game->menu_sel == 0) { // Yes
                    memset(game->grid, 0, sizeof(game->grid));
                    game->grid[GRID_COUNT - 1] = 1;
                    game->player_cursor = GRID_COUNT;
                    game->flipper_cursor = 0;
                    game->solved = false;
                    game->flipper_move_index = 0;
                    game->user_move_count = 0;
                    game->score = 0;
                    game->state = GameState_PlayerTurn;
                } else { // No
                    game->state = GameState_ExitScreen;
                    game->state_tick = furi_get_tick();
                }
            }
            return;
        }
        if(game->state == GameState_WinMenu) {
            if(event->key == InputKeyUp || event->key == InputKeyRight) {
                game->menu_sel = 0; // Yes
            } else if(event->key == InputKeyDown || event->key == InputKeyLeft) {
                game->menu_sel = 1; // No
            } else if(event->key == InputKeyOk) {
                if(game->menu_sel == 0) { // Yes
                    memset(game->grid, 0, sizeof(game->grid));
                    game->grid[GRID_COUNT - 1] = 1;
                    game->player_cursor = GRID_COUNT;
                    game->flipper_cursor = 0;
                    game->solved = false;
                    game->flipper_move_index = 0;
                    game->user_move_count = 0;
                    game->score = 0;
                    game->state = GameState_PlayerTurn;
                } else { // No
                    game->state = GameState_ExitScreen;
                    game->state_tick = furi_get_tick();
                }
            }
            return;
        }
        if(game->state == GameState_WinScreen && event->key == InputKeyOk) {
            game->state = GameState_WinMenu;
            game->menu_sel = 0;
            game->state_tick = furi_get_tick();
            return;
        }
        if(game->state == GameState_WinScreen && event->key == InputKeyBack) {
            if(furi_get_tick() - game->last_back_key_tick <= 1800) {
                game->back_key_count++;
                if(game->back_key_count >= 2) {
                    game->state = GameState_ExitScreen;
                    game->state_tick = furi_get_tick();
                    game->back_key_count = 0;
                    return;
                }
            } else {
                game->back_key_count = 1;
            }
            game->last_back_key_tick = furi_get_tick();
            return;
        }
        if(game->state == GameState_PlayerTurn && !game->solved) {
            if(event->key == InputKeyRight) {
                if(furi_get_tick() - game->last_right_key_tick <= 1800) {
                    game->right_key_count++;
                    if(game->right_key_count >= 3) {
                        bool allow_credits = false;
                        if(furi_get_tick() - game->last_credits_tick >= CREDITS_COOLDOWN_MS ||
                           game->user_move_count >= CREDITS_MOVE_THRESHOLD) {
                            allow_credits = true;
                            game->credits_access_count = 0;
                            game->user_move_count = 0;
                        } else if(game->credits_access_count % 2 == 0) {
                            allow_credits = true;
                        }
                        if(allow_credits) {
                            game->state = GameState_Credits;
                            game->intro_crawl_y = 64;
                            game->state_tick = furi_get_tick();
                            memcpy(game->saved_grid, game->grid, sizeof(game->grid));
                            game->saved_player_cursor = game->player_cursor;
                            game->saved_flipper_cursor = game->flipper_cursor;
                            game->saved_solved = game->solved;
                            game->credits_access_count++;
                            game->last_credits_tick = furi_get_tick();
                            game->right_key_count = 0;
                            return;
                        }
                        game->right_key_count = 0;
                    }
                } else {
                    game->right_key_count = 1;
                }
                game->last_right_key_tick = furi_get_tick();
            }
            if(event->key == InputKeyBack) {
                if(furi_get_tick() - game->last_back_key_tick <= 1800) {
                    game->back_key_count++;
                    if(game->back_key_count >= 2) {
                        game->state = GameState_ExitScreen;
                        game->state_tick = furi_get_tick();
                        game->back_key_count = 0;
                        return;
                    }
                } else {
                    game->back_key_count = 1;
                }
                game->last_back_key_tick = furi_get_tick();
                return;
            }
            switch(event->key) {
            case InputKeyUp:
                if(game->player_cursor == GRID_COUNT) {
                    game->player_cursor = 4; // Move to top row, second column
                } else if(game->player_cursor >= GRID_SIZE) {
                    game->player_cursor -= GRID_SIZE;
                }
                break;
            case InputKeyDown:
                if(game->player_cursor == GRID_COUNT) {
                    game->player_cursor = 12; // Move to bottom row, first column
                } else if(game->player_cursor < GRID_COUNT - GRID_SIZE) {
                    game->player_cursor += GRID_SIZE;
                }
                break;
            case InputKeyLeft:
                if(game->player_cursor == GRID_COUNT) {
                    game->player_cursor = 3; // Move to top-right corner
                } else if(game->player_cursor % GRID_SIZE) {
                    game->player_cursor--;
                }
                break;
            case InputKeyRight:
                if(game->player_cursor == GRID_COUNT) {
                    game->player_cursor = 0; // Move to top-left corner
                } else if(game->player_cursor % GRID_SIZE < GRID_SIZE - 1) {
                    game->player_cursor++;
                } else if(game->player_cursor < GRID_COUNT) {
                    game->player_cursor = GRID_COUNT; // Move to offset block from right edge
                }
                break;
            case InputKeyOk:
                lofz_update(game, game->player_cursor);
                if(lofz_is_solved(game)) {
                    game->solved = true;
                    game->wins++;
                    save_leaderboard(game);
                    game->state = GameState_WinFade;
                    game->fade_level = 0;
                    game->state_tick = furi_get_tick();
                    memcpy(game->saved_grid, game->grid, sizeof(game->grid));
                    game->saved_player_cursor = game->player_cursor;
                    game->saved_flipper_cursor = game->flipper_cursor;
                    game->saved_solved = game->solved;
                } else if(is_lost_nulls(game)) {
                    game->state = GameState_LostFade;
                    game->fade_level = 0;
                    game->state_tick = furi_get_tick();
                    memcpy(game->saved_grid, game->grid, sizeof(game->grid));
                    game->saved_player_cursor = game->player_cursor;
                    game->saved_flipper_cursor = game->flipper_cursor;
                    game->saved_solved = game->solved;
                } else {
                    game->state = GameState_FlipperPopup;
                    game->popup_timer = furi_get_tick();
                    game->popup_phrase = rand() % FLIPPER_PHRASE_COUNT;
                    game->scroll_offset = 0;
                    game->scroll_start_tick = furi_get_tick();
                }
                break;
            default:
                break;
            }
        }
    }
}

// --- Main Draw Function ---
static void lofz_draw(Canvas* canvas, void* ctx) {
    LOFZGame* game = (LOFZGame*)ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    // Check for stress conditions (<20% off or score > 981)
    int off_count = 0;
    for(uint8_t i = 0; i < GRID_COUNT; i++) {
        if(game->grid[i] == 0) off_count++;
    }
    bool is_stressed = (off_count < (GRID_COUNT * 20 / 100) || game->score > 981) &&
                       game->score != 1;
    if(is_stressed && game->mascot_bounce_offset > 0 &&
       (furi_get_tick() - game->last_mascot_move_tick > MASCOT_MOVE_INTERVAL)) {
        int move = rand() % 4;
        int new_x = game->mascot_x, new_y = game->mascot_y;
        if(move == 0 && new_x > 80)
            new_x -= 2;
        else if(move == 1 && new_x < 100)
            new_x += 2;
        else if(move == 2 && new_y > 10)
            new_y -= 2;
        else if(move == 3 && new_y < 20)
            new_y += 2;
        game->mascot_x = new_x;
        game->mascot_y = new_y;
        game->last_mascot_move_tick = furi_get_tick();
    } else if(!is_stressed) {
        game->mascot_x = MASCOT_X;
        game->mascot_y = MASCOT_Y;
    }

    if(game->state == GameState_Loading) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, 128, 64);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Loading...");
        return;
    }
    if(game->state == GameState_PressToPlay) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, 128, 64);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Press OK to Play");
        return;
    }
    if(game->state == GameState_IntroCrawl) {
        if(game->up_key_count >= 10) {
            char leaderboard_lines[LEADERBOARD_ENTRIES * 2][32];
            int line_idx = 0;
            for(int i = 0; i < LEADERBOARD_ENTRIES; i++) {
                snprintf(
                    leaderboard_lines[line_idx],
                    32,
                    "%d : %lu :: %lu",
                    i + 1,
                    leaderboard[i].wins,
                    leaderboard[i].presses);
                leaderboard_lines[line_idx + 1][0] = '\0';
                line_idx += 2;
            }
            draw_crawl_text(
                canvas, game->intro_crawl_y, (const char* const*)leaderboard_lines, line_idx);
        } else {
            draw_crawl_text(canvas, game->intro_crawl_y, intro_lines, INTRO_LINES_COUNT);
        }
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignCenter, "Press OK to continue");
        return;
    }
    if(game->state == GameState_Credits) {
        draw_crawl_text(canvas, game->intro_crawl_y, credits_lines, CREDITS_LINES_COUNT);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignCenter, "Press OK to continue");
        return;
    }
    if(game->state == GameState_MischiefScreen) {
        draw_crawl_text(canvas, game->intro_crawl_y, mischief_lines, MISCHIEF_LINES_COUNT);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignCenter, "Press OK to continue");
        return;
    }
    if(game->state == GameState_DeadScreen) {
        draw_crawl_text(canvas, game->intro_crawl_y, dead_lines, DEAD_LINES_COUNT);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignCenter, "Press OK to continue");
        return;
    }
    if(game->state == GameState_WinScreen) {
        draw_crawl_text(canvas, game->intro_crawl_y, win_lines, WIN_LINES_COUNT);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignCenter, "Press OK to continue");
        return;
    }
    if(game->state == GameState_SecretCode) {
        if(secret_codes[game->secret_code_index][0] == '\0') {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 0, 0, 128, 64);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str_aligned(
                canvas, 64, 60, AlignCenter, AlignCenter, "Press OK to continue");
        } else {
            draw_crawl_text(
                canvas, game->intro_crawl_y, secret_code_lines, SECRET_CODE_LINES_COUNT);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str_aligned(
                canvas, 64, 60, AlignCenter, AlignCenter, "Press OK to continue");
        }
        return;
    }
    if(game->state == GameState_IntroFade || game->state == GameState_LostFade ||
       game->state == GameState_WinFade) {
        draw_3d_board(canvas, game);
        int lvl = game->fade_level;
        for(int i = 0; i < 128; i += 2)
            canvas_draw_box(canvas, 0, 0, 128, lvl);
        return;
    }
    if(game->state == GameState_LostNulls) {
        draw_crawl_text(canvas, game->intro_crawl_y, dead_lines, DEAD_LINES_COUNT);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignCenter, "Press OK to continue");
        return;
    }
    if(game->state == GameState_LostMenu) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, 128, 64);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "You Died, Play Again?");
        canvas_draw_str(canvas, 108, 56, game->menu_sel == 0 ? ">Yes" : " Yes");
        canvas_draw_str(canvas, 20, 56, game->menu_sel == 1 ? ">No" : " No");
        return;
    }
    if(game->state == GameState_WinMenu) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, 128, 64);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Victory, Play Again?");
        canvas_draw_str(canvas, 108, 56, game->menu_sel == 0 ? ">Yes" : " Yes");
        canvas_draw_str(canvas, 20, 56, game->menu_sel == 1 ? ">No" : " No");
        return;
    }
    if(game->state == GameState_ExitScreen) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, 128, 64);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Enjoy your day");
        canvas_set_font(canvas, FontSecondary);
        char wins_str[32];
        snprintf(wins_str, sizeof(wins_str), "WINS: %lu", game->wins);
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignCenter, wins_str);
        char flips_str[32];
        snprintf(flips_str, sizeof(flips_str), "FLIPS: %lu", game->total_flips);
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignCenter, flips_str);
        return;
    }
    if(game->state == GameState_FlipperPopup || game->state == GameState_FlipperTurn) {
        draw_3d_board(canvas, game);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_box(canvas, POPUP_X, POPUP_Y, POPUP_WIDTH, 14);
        canvas_set_color(canvas, ColorWhite);
        draw_scrolling_text(
            canvas,
            POPUP_X + 2,
            POPUP_Y + 11,
            flipper_phrases[game->popup_phrase],
            POPUP_WIDTH - 4,
            game,
            game->state == GameState_FlipperPopup);
        canvas_set_color(canvas, ColorBlack);
        draw_mascot(
            canvas, game->mascot_x, game->mascot_y, game->mascot_bounce_offset, game->bounce_up);
        canvas_draw_str(canvas, 92, 12, "Flipper");
        canvas_set_font(canvas, FontSecondary);
        char wins_str[32];
        snprintf(wins_str, sizeof(wins_str), "WINS: %lu", game->wins);
        canvas_draw_str(canvas, 92, 47, wins_str);
        draw_rounded_box(canvas, SCORE_BOX_X, SCORE_BOX_Y, SCORE_BOX_W, SCORE_BOX_H, 2);
        canvas_set_color(canvas, ColorBlack);
        char score_str[32];
        if(game->score >= 100) {
            snprintf(
                score_str, sizeof(score_str), "+%lu", game->score % 100 ? game->score % 100 : 100);
        } else {
            snprintf(score_str, sizeof(score_str), "%lu", game->score);
        }
        canvas_draw_str_aligned(
            canvas,
            SCORE_BOX_X + SCORE_BOX_W / 2,
            SCORE_BOX_Y + SCORE_BOX_H / 2 + 2,
            AlignCenter,
            AlignCenter,
            score_str);
        return;
    }
    // Main gameplay
    draw_3d_board(canvas, game);
    draw_mascot(
        canvas, game->mascot_x, game->mascot_y, game->mascot_bounce_offset, game->bounce_up);
    canvas_draw_str(canvas, 92, 12, "Flipper");
    canvas_set_font(canvas, FontSecondary);
    char wins_str[32];
    snprintf(wins_str, sizeof(wins_str), "WINS: %lu", game->wins);
    canvas_draw_str(canvas, 92, 47, wins_str);
    draw_rounded_box(canvas, SCORE_BOX_X, SCORE_BOX_Y, SCORE_BOX_W, SCORE_BOX_H, 2);
    canvas_set_color(canvas, ColorBlack);
    char score_str[32];
    if(game->score >= 100) {
        snprintf(
            score_str, sizeof(score_str), "+%lu", game->score % 100 ? game->score % 100 : 100);
    } else {
        snprintf(score_str, sizeof(score_str), "%lu", game->score);
    }
    canvas_draw_str_aligned(
        canvas,
        SCORE_BOX_X + SCORE_BOX_W / 2,
        SCORE_BOX_Y + SCORE_BOX_H / 2 + 2,
        AlignCenter,
        AlignCenter,
        score_str);
}

// --- Main App ---
int32_t lofz_app(void* p) {
    (void)p;
    LOFZGame game;
    memset(&game, 0, sizeof(game));
    game.grid[GRID_COUNT - 1] = 1;
    game.player_cursor = GRID_COUNT;
    game.flipper_cursor = 0;
    game.menu_sel = 0;
    game.intro_crawl_y = 64;
    game.state = GameState_Loading;
    game.state_tick = furi_get_tick();
    game.flipper_move_index = 0;
    game.audio_enabled = true;
    game.score = 0;
    game.wins = 0;
    game.total_flips = 0;
    game.secret_code_index = 0;
    game.mascot_x = MASCOT_X;
    game.mascot_y = MASCOT_Y;

    load_leaderboard();

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    ViewPort* viewport = view_port_alloc();
    view_port_draw_callback_set(viewport, lofz_draw, &game);
    view_port_input_callback_set(viewport, lofz_input, &game);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, viewport, GuiLayerFullscreen);

    bool running = true;
    bool loading_done = false;
    uint8_t flipper_moves_left = 0;

    while(running) {
        if(game.state == GameState_Finished) break;

        if(game.mascot_bounce_offset > 0 && furi_get_tick() - game.bounce_start_tick > 300) {
            game.mascot_bounce_offset = 0;
        }

        if(game.state == GameState_Loading && !loading_done) {
            if(furi_get_tick() - game.state_tick >= 7280) {
                game.state = GameState_PressToPlay;
                loading_done = true;
            }
        } else if(
            game.state == GameState_IntroCrawl || game.state == GameState_Credits ||
            game.state == GameState_MischiefScreen || game.state == GameState_DeadScreen ||
            game.state == GameState_WinScreen || game.state == GameState_SecretCode) {
            int valid_lines = 0;
            const char* const* lines = NULL;
            int line_count = 0;
            if(game.state == GameState_Credits) {
                lines = credits_lines;
                line_count = CREDITS_LINES_COUNT;
            } else if(game.state == GameState_MischiefScreen) {
                lines = mischief_lines;
                line_count = MISCHIEF_LINES_COUNT;
            } else if(game.state == GameState_DeadScreen) {
                lines = dead_lines;
                line_count = DEAD_LINES_COUNT;
            } else if(game.state == GameState_WinScreen) {
                lines = win_lines;
                line_count = WIN_LINES_COUNT;
            } else if(game.state == GameState_SecretCode) {
                lines = secret_code_lines;
                line_count = SECRET_CODE_LINES_COUNT;
            } else {
                lines = (game.up_key_count >= 10 ? NULL : intro_lines);
                line_count =
                    (game.up_key_count >= 10 ? LEADERBOARD_ENTRIES * 2 : INTRO_LINES_COUNT);
            }
            if(lines) {
                for(int i = 0; i < line_count; i++) {
                    if(lines[i][0] != '\0') valid_lines++;
                }
            } else if(game.up_key_count >= 10) {
                for(int i = 0; i < LEADERBOARD_ENTRIES; i++) {
                    valid_lines += 2;
                }
            }
            int crawl_end = -(valid_lines * LINE_HEIGHT);
            if(game.intro_crawl_y > crawl_end) {
                if(furi_get_tick() - game.state_tick > CRAWL_SPEED) {
                    game.intro_crawl_y -= 1;
                    game.state_tick = furi_get_tick();
                }
            } else if(game.state == GameState_WinScreen) {
                game.state = GameState_WinMenu;
                game.menu_sel = 0;
                game.state_tick = furi_get_tick();
            } else if(game.state == GameState_LostNulls) {
                game.state = GameState_LostMenu;
                game.menu_sel = 0;
                game.state_tick = furi_get_tick();
            } else {
                game.state = GameState_IntroCrawl;
                game.intro_crawl_y = 64;
                game.state_tick = furi_get_tick();
                if(game.state != GameState_IntroCrawl) {
                    memcpy(game.grid, game.saved_grid, sizeof(game.grid));
                    game.player_cursor = game.saved_player_cursor;
                    game.flipper_cursor = game.saved_flipper_cursor;
                    game.solved = game.saved_solved;
                }
            }
        } else if(game.state == GameState_IntroFade || game.state == GameState_WinFade) {
            if(game.fade_level < 128) {
                game.fade_level += 8;
                game.state_tick = furi_get_tick();
            } else {
                game.state =
                    (game.state == GameState_WinFade ? GameState_WinScreen : GameState_PlayerTurn);
                game.fade_level = 0;
                game.intro_crawl_y = 64;
            }
        } else if(game.state == GameState_LostFade) {
            if(game.fade_level < 128) {
                game.fade_level += 8;
                game.state_tick = furi_get_tick();
            } else {
                game.state = GameState_LostNulls;
                game.intro_crawl_y = 64;
                game.state_tick = furi_get_tick();
            }
        } else if(game.state == GameState_FlipperPopup && !game.solved) {
            int chars_fit = (POPUP_WIDTH - 4) / 7;
            int scroll_limit = strlen(flipper_phrases[game.popup_phrase]) - chars_fit + 1;
            if(scroll_limit < 0) scroll_limit = 0;
            uint32_t display_time = scroll_limit > 0 ? (scroll_limit * SCROLL_SPEED + 1000) : 2000;
            if(furi_get_tick() - game.popup_timer > display_time) {
                game.state = GameState_FlipperTurn;
                flipper_moves_left = flipper_move_sequence[game.flipper_move_index];
                game.flipper_move_index = (game.flipper_move_index + 1) % FLIPPER_SEQUENCE_COUNT;
                if(flipper_moves_left > 0) lofz_flipper_move(&game);
            }
        } else if(game.state == GameState_FlipperTurn && !game.solved) {
            if(flipper_moves_left > 0) {
                if(furi_get_tick() - game.bounce_start_tick > 300) {
                    lofz_flipper_move(&game);
                    flipper_moves_left--;
                }
            } else {
                if(lofz_is_solved(&game)) {
                    game.solved = true;
                    game.wins++;
                    save_leaderboard(&game);
                    game.state = GameState_WinFade;
                    game.fade_level = 0;
                    game.state_tick = furi_get_tick();
                    memcpy(game.saved_grid, game.grid, sizeof(game.grid));
                    game.saved_player_cursor = game.player_cursor;
                    game.saved_flipper_cursor = game.flipper_cursor;
                    game.saved_solved = game.solved;
                } else if(is_lost_nulls(&game)) {
                    game.state = GameState_LostFade;
                    game.fade_level = 0;
                    game.state_tick = furi_get_tick();
                    memcpy(game.saved_grid, game.grid, sizeof(game.grid));
                    game.saved_player_cursor = game.player_cursor;
                    game.saved_flipper_cursor = game.flipper_cursor;
                    game.solved = game.saved_solved;
                } else {
                    game.state = GameState_PlayerTurn;
                }
            }
        } else if(game.state == GameState_ExitScreen) {
            save_leaderboard(&game);
            if(furi_get_tick() - game.state_tick > 3140) {
                game.state = GameState_Finished;
            }
        }

        InputEvent event;
        if(furi_message_queue_get(event_queue, &event, 50) == FuriStatusOk) {
            lofz_input(&event, &game);
        }
        view_port_update(viewport);
        furi_delay_ms(25);
        if(game.solved) furi_delay_ms(2200);
    }

    gui_remove_view_port(gui, viewport);
    furi_record_close(RECORD_GUI);
    view_port_free(viewport);
    furi_message_queue_free(event_queue);
    return 0;
}
