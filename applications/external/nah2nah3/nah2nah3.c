#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <string.h>
#include <furi_hal.h>

#define SCREEN_WIDTH         128
#define SCREEN_HEIGHT        64
#define PORTRAIT_WIDTH       64
#define PORTRAIT_HEIGHT      128
#define FPS_BASE             22
#define MAX_STREAK_INT       9999 // Arbitrary max for streak to handle overflow
#define COOLDOWN_MS          180000 // 3 minutes
#define NOTIFICATION_MS      1500 // 1.5 seconds
#define FIXED_POINT_SCALE    1000 // Scale for fixed-point arithmetic
#define BACK_BUTTON_COOLDOWN 500 // 500ms cooldown for Back button
#define ORIENTATION_HOLD_MS  1500 // 1.5s for orientation toggle
#define CREDITS_FPS          11700 // 11.7 FPS = 85ms per frame
#define LOADING_MS           1500 // 1.5s loading screen
#define TAP_DRM_MS           300 // 0.3s for tap DRM in Flip Zip
#define MIN_SPEED_BPM        65 // Minimum speed for speed bar
#define SPEED_BAR_Y          (PORTRAIT_HEIGHT - 8)
#define SPEED_BAR_HEIGHT     2
#define SPEED_BAR_X          0
#define SPEED_BAR_WIDTH      PORTRAIT_WIDTH

typedef enum {
    GAME_STATE_LOADING, // Initial loading screen
    GAME_STATE_TITLE,
    GAME_STATE_ROTATE,
    GAME_STATE_ZERO_HERO,
    GAME_STATE_FLIP_ZIP,
    GAME_STATE_CREDITS,
    GAME_STATE_PAUSE
} GameState;

typedef enum {
    GAME_MODE_ZERO_HERO,
    GAME_MODE_FLIP_ZIP,
    GAME_MODE_LINE_CAR,
    GAME_MODE_DROP_PER,
    GAME_MODE_TECTONE_SIM,
    GAME_MODE_SPACE_FLIGHT
} GameMode;

typedef enum {
    DIFFICULTY_EASY,
    DIFFICULTY_MEDIUM,
    DIFFICULTY_HARD
} Difficulty;

typedef struct {
    GameState state;
    GameMode selected_game;
    bool is_left_handed;
    uint32_t last_input_time;
    uint8_t rapid_click_count;
    uint32_t game_start_time;
    bool is_day;
    uint32_t day_night_toggle_time;
    // Title menu
    int selected_side; // 0: left, 1: right
    int selected_row; // 0: row1, 1: row2, 2: row3
    int title_scroll_offset; // For scrolling menu
    uint32_t back_hold_start; // Track back button hold time
    // Rotate animation
    uint32_t rotate_start_time;
    int32_t rotate_angle; // Fixed-point (degrees * FIXED_POINT_SCALE)
    int32_t zoom_factor; // Fixed-point (scale * FIXED_POINT_SCALE)
    bool rotate_skip;
    // Zero Hero
    int streak;
    int prev_streak;
    int highest_streak;
    int streak_sum;
    int streak_count;
    int oflow;
    Difficulty difficulty;
    uint32_t last_difficulty_check;
    int key_columns[5]; // U, L, O, R, D
    int key_positions[5][10]; // Up to 10 keys per column
    bool is_holding[5];
    bool strum_hit[5]; // Highlight strumming bar on hit
    int score;
    int score_oflow;
    uint32_t last_notification_time;
    char notification_text[32];
    uint8_t note_q_a; // 0: none, 1: YES, 2: NO
    int notification_x; // Scrolling position for notifications
    // Flip Zip
    int mascot_lane; // 0 to 4
    int mascot_y; // Vertical position in lanes plane
    int speed_bpm;
    uint32_t last_back_press;
    bool is_jumping;
    int32_t jump_progress; // Fixed-point (progress * FIXED_POINT_SCALE)
    int jump_scale; // Grow/shrink during jump
    uint32_t jump_hold_time; // Track OK button hold duration
    int successful_jumps; // Count for speed increases
    int obstacles[5][10]; // Obstacle type per lane
    int obstacle_positions[5][10];
    uint32_t last_tap_time; // For tap DRM and speed boost
    int tap_count; // Track taps for BPM calculation
    uint32_t tap_window_start; // Start of tap window for BPM
    int jump_y_accumulated; // Track Up presses during jump
    // AI
    uint32_t last_ai_update;
    int ai_beat_counter;
    // Credits
    int credits_y;
    // Pause
    int pause_back_count; // Track Back presses in pause
    // Start menu
    int start_back_count; // Track Back presses in start menu
    // Frame counter for faux multithreading
    int frame_counter;
    // ViewPort
    ViewPort* view_port;
    // Exit flag
    bool should_exit;
    // Back button cooldown
    uint32_t last_back_press_time;
} GameContext;

static const char* credits_lines[] = {
    "",
    "Nah2-Nah3",
    "    ",
    "    ",
    "Nah Nah Nah",
    "    ",
    "   ",
    "to the",
    "    ",
    "    ",
    "Nah",
    ""};
static const char* notification_messages[] = {
    "Whoa!",
    "Is it hot or just you?",
    "Your fingers are lit",
    "GO GO GO",
    "You Got This!",
    "Positive Statement!",
    "Keep Rocking!",
    "You're on Fire!",
    "Smash It!",
    "Unstoppable!",
    "Epic Moves!"};

static const char* menu_titles[][2] = {
    {"Zero Hero", "Flip Zip"},
    {"Line Car", "Drop Per"},
    {"Tectone Sim", "Space Flight"}};
static const char* menu_subtitles[][2] = {
    {"Jamin Banin", "Runin & Jumpin"},
    {"Drift or Nah", "Catch dem All"},
    {"Based Hits", "Star Chase"}};

// Word-wrap text without strtok, safe for Flipper Zero’s limited stdlib
static void draw_word_wrapped_text(
    Canvas* canvas,
    const char* text,
    int x,
    int y,
    int max_width,
    Font font) {
    if(!canvas || !text) return; // Prevent null pointer crashes
    char buffer[32];
    int buffer_idx = 0;
    int current_x = x;
    int current_y = y;
    size_t text_len = strlen(text);
    int char_width = (font == FontPrimary) ? 8 : 6;

    canvas_set_font(canvas, font);
    for(size_t i = 0; i <= text_len; i++) {
        if(text[i] == ' ' || text[i] == '\0' || buffer_idx >= (int)(sizeof(buffer) - 1)) {
            if(buffer_idx > 0) {
                buffer[buffer_idx] = '\0';
                int word_width = buffer_idx * char_width;
                if(current_x + word_width > x + max_width) {
                    current_x = x;
                    current_y += (font == FontPrimary) ? 10 : 8;
                }
                canvas_draw_str(canvas, current_x, current_y, buffer);
                current_x += word_width + char_width;
                buffer_idx = 0;
            }
        } else {
            buffer[buffer_idx++] = text[i];
        }
    }
    canvas_set_font(canvas, FontSecondary);
}

// Draw notifications with scrolling support
static void draw_notification(Canvas* canvas, GameContext* ctx) {
    if(!canvas || !ctx || ctx->notification_text[0] == '\0') return;
    uint32_t elapsed = furi_get_tick() - ctx->last_notification_time;
    if(elapsed > NOTIFICATION_MS && ctx->note_q_a == 0) {
        ctx->notification_text[0] = '\0';
        ctx->notification_x = 0;
        return;
    }
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, PORTRAIT_HEIGHT - 7, PORTRAIT_WIDTH, 7);
    canvas_set_color(canvas, ColorWhite);
    int text_width = strlen(ctx->notification_text) * 6;
    int x = (ctx->note_q_a == 0) ? ctx->notification_x : (PORTRAIT_WIDTH - text_width) / 2;
    draw_word_wrapped_text(
        canvas, ctx->notification_text, x, PORTRAIT_HEIGHT - 1, PORTRAIT_WIDTH, FontSecondary);
}

// Draw title menu with scrolling support (limited to one row at a time)
static void draw_title_menu(Canvas* canvas, GameContext* ctx) {
    if(!canvas || !ctx) return;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_line(canvas, SCREEN_WIDTH / 2, 0, SCREEN_WIDTH / 2, SCREEN_HEIGHT);

    int row = ctx->selected_row; // Display only the selected row
    int y_offset = 3; // Fixed offset to center the single row

    // Left option
    if(ctx->selected_side == 0) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_frame(canvas, 1, y_offset + 11, SCREEN_WIDTH / 2 - 2, 30);
        canvas_draw_frame(canvas, 0, y_offset + 10, SCREEN_WIDTH / 2, 32);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 2, y_offset + 12, SCREEN_WIDTH / 2 - 4, 28);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_frame(canvas, 1, y_offset + 11, SCREEN_WIDTH / 2 - 2, 30);
    }
    canvas_draw_str(canvas, 10, y_offset + 8, menu_titles[row][0]);
    draw_word_wrapped_text(
        canvas, menu_subtitles[row][0], 10, y_offset + 50, SCREEN_WIDTH / 2 - 20, FontSecondary);
    if(ctx->selected_side == 0) {
        if(row == 0) { // Zero Hero
            for(int i = 0; i < 5; i++) {
                int x = 3 + i * 10;
                int y = y_offset + 12 + (furi_get_tick() / 100) % 30;
                canvas_draw_str(canvas, x, y, "v");
            }
        } else if(row == 1) { // Line Car
            int x = 10 + (furi_get_tick() / 100) % 30;
            canvas_draw_str(canvas, x, y_offset + 30, "'.-.\\");
        } else { // Tectone Simulator
            int frame = (furi_get_tick() / 200) % 2;
            canvas_draw_str(
                canvas, 16, y_offset + 28, frame == 0 ? "(o_|o)!" : " !(0 |o)"); // head?
            canvas_draw_str(canvas, 9, y_offset + 35, frame == 0 ? "/| " : " .-."); // Left arm
            canvas_draw_str(canvas, 39, y_offset + 35, frame == 0 ? " ,-." : " |\\"); // Right arm
        }
    }

    // Right option
    if(ctx->selected_side == 1) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_frame(canvas, SCREEN_WIDTH / 2 + 1, y_offset + 11, SCREEN_WIDTH / 2 - 2, 30);
        canvas_draw_frame(canvas, SCREEN_WIDTH / 2, y_offset + 10, SCREEN_WIDTH / 2, 32);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, SCREEN_WIDTH / 2 + 2, y_offset + 12, SCREEN_WIDTH / 2 - 4, 28);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_frame(canvas, SCREEN_WIDTH / 2 + 1, y_offset + 11, SCREEN_WIDTH / 2 - 2, 30);
    }
    canvas_draw_str(canvas, SCREEN_WIDTH / 2 + 10, y_offset + 8, menu_titles[row][1]);
    draw_word_wrapped_text(
        canvas,
        menu_subtitles[row][1],
        SCREEN_WIDTH / 2 + 10,
        y_offset + 50,
        SCREEN_WIDTH / 2 - 20,
        FontSecondary);
    if(ctx->selected_side == 1) {
        if(row == 0) { // Flip Zip
            int x = SCREEN_WIDTH / 2 + 3 + (furi_get_tick() / 100) % 30;
            canvas_draw_str(canvas, x, y_offset + 30, "F");
        } else if(row == 1) { // Drop Per
            for(int i = 0; i < 5; i++) {
                int x = SCREEN_WIDTH / 2 + 3 + i * 8;
                int y = y_offset + 12 + (furi_get_tick() / 100 + i * 5) % 30;
                canvas_draw_str(canvas, x, y, "d");
            }
        } else { // Space Flight
            int x = SCREEN_WIDTH / 2 + 12 + (furi_get_tick() / 100) % 20;
            int y = y_offset + 19 + (furi_get_tick() / 150) % 10;
            canvas_draw_str(canvas, x, y, "C>");
            canvas_draw_circle(canvas, x + 10, y + 12, 5);
        }
    }
}

// Draw loading screen
static void draw_loading_screen(Canvas* canvas) {
    if(!canvas) return;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    canvas_set_color(canvas, ColorWhite);
    draw_word_wrapped_text(
        canvas, "Nah to the Nah Nah Nah", 10, SCREEN_HEIGHT / 2, SCREEN_WIDTH - 20, FontPrimary);
}

// Draw pause screen
static void draw_pause_screen(Canvas* canvas) {
    if(!canvas) return;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, PORTRAIT_WIDTH, PORTRAIT_HEIGHT);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_frame(canvas, PORTRAIT_WIDTH / 2 - 20, PORTRAIT_HEIGHT / 2 - 10, 40, 20);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, PORTRAIT_WIDTH / 2 - 18, PORTRAIT_HEIGHT / 2 - 8, 36, 16);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, PORTRAIT_WIDTH / 2 - 12, PORTRAIT_HEIGHT / 2 + 4, "Pause");
}

// Draw rotate screen with Flipper animation
static void draw_rotate_screen(Canvas* canvas, GameContext* ctx) {
    if(!canvas || !ctx) return;
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 0, 0, PORTRAIT_WIDTH, PORTRAIT_HEIGHT);
    canvas_set_color(canvas, ColorBlack);
    uint32_t elapsed = furi_get_tick() - ctx->rotate_start_time;
    if(elapsed < 1000) {
        canvas_draw_frame(canvas, 20, 10, 88, 44);
        canvas_draw_str(canvas, 54, 54, "FLIPPER");
        canvas_draw_circle(canvas, 54, 50, 10);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_disc(canvas, 90, 50, 5); // Filled smaller circle
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 64, 32, ">");
    } else if(elapsed < 6000 && !ctx->rotate_skip) {
        int32_t t = ((elapsed - 1000) * FIXED_POINT_SCALE) / 5000;
        ctx->rotate_angle = (t * 90) / FIXED_POINT_SCALE;
        ctx->zoom_factor = FIXED_POINT_SCALE + (t * 2);
        int w = (88 * ctx->zoom_factor) / FIXED_POINT_SCALE;
        int h = (44 * ctx->zoom_factor) / FIXED_POINT_SCALE;
        int x = (SCREEN_WIDTH - w) / 2;
        int y = (SCREEN_HEIGHT - h) / 2;
        canvas_draw_frame(canvas, x, y, w, h);
        canvas_draw_str(canvas, x + w / 2, y + h + 4, "FLIPPER");
        canvas_draw_circle(
            canvas,
            x + w / 2,
            y + h + (10 * ctx->zoom_factor) / FIXED_POINT_SCALE,
            (10 * ctx->zoom_factor) / FIXED_POINT_SCALE);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_disc(
            canvas,
            x + w - (5 * ctx->zoom_factor) / FIXED_POINT_SCALE,
            y + h + (5 * ctx->zoom_factor) / FIXED_POINT_SCALE,
            (5 * ctx->zoom_factor) / FIXED_POINT_SCALE);
        canvas_set_color(canvas, ColorBlack);
        int arrow_x = x + w / 2;
        int arrow_y = y + h / 2;
        canvas_draw_str(canvas, arrow_x, arrow_y, (t < FIXED_POINT_SCALE / 2) ? ">" : "^");
    } else {
        view_port_set_orientation(
            ctx->view_port,
            ctx->is_left_handed ? ViewPortOrientationVerticalFlip : ViewPortOrientationVertical);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 0, 0, PORTRAIT_WIDTH, PORTRAIT_HEIGHT);
        canvas_set_color(canvas, ColorBlack);
        draw_word_wrapped_text(
            canvas,
            " PLEASE  ROTATE  YOUR  SCREEN     >>>>> ",
            5,
            20,
            PORTRAIT_WIDTH - 10,
            FontPrimary);
    }
}

// Draw Zero Hero game with arrow symbols
static void draw_zero_hero(Canvas* canvas, GameContext* ctx) {
    if(!canvas || !ctx) return;
    canvas_set_font(canvas, FontSecondary);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 0, 0, PORTRAIT_WIDTH, PORTRAIT_HEIGHT);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, PORTRAIT_WIDTH, 26); // Larger box for score and streak
    canvas_set_color(canvas, ColorBlack);
    for(int i = 0; i < 5; i++) {
        canvas_draw_line(canvas, i * 12 + 2, 26, i * 12 + 2, PORTRAIT_HEIGHT - 4);
        canvas_draw_str(
            canvas,
            i * 12 + 4,
            PORTRAIT_HEIGHT - 5,
            i == 0 ? "^" :
            i == 1 ? "<" :
            i == 2 ? "O" :
            i == 3 ? ">" :
                     "v");
    }
    for(int i = 0; i < 5; i++) {
        canvas_set_color(canvas, ctx->strum_hit[i] ? ColorWhite : ColorBlack);
        canvas_draw_box(canvas, i * 12 + 2, PORTRAIT_HEIGHT - 6, 10, 2);
        if(ctx->strum_hit[i]) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_frame(canvas, i * 12 + 1, PORTRAIT_HEIGHT - 7, 12, 4);
            canvas_set_color(canvas, ColorWhite);
        }
    }
    canvas_draw_box(canvas, 0, PORTRAIT_HEIGHT - 4, PORTRAIT_WIDTH, 4);
    for(int i = 0; i < 5; i++) {
        for(int j = 0; j < 10; j++) {
            if(ctx->key_positions[i][j] > 0) {
                canvas_draw_str(
                    canvas,
                    i * 12 + 4,
                    ctx->key_positions[i][j],
                    i == 0 ? "^" :
                    i == 1 ? "<" :
                    i == 2 ? "O" :
                    i == 3 ? ">" :
                             "v");
            }
        }
    }
    char streak_str[32];
    snprintf(streak_str, sizeof(streak_str), "Streak: %d.%d", ctx->streak, ctx->oflow);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_color(canvas, ColorWhite);
    draw_word_wrapped_text(
        canvas,
        streak_str,
        (PORTRAIT_WIDTH - strlen(streak_str) * 6) / 2,
        17,
        PORTRAIT_WIDTH,
        FontSecondary);
    char score_str[32];
    snprintf(score_str, sizeof(score_str), "Score: %d.%d", ctx->score, ctx->score_oflow);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_color(canvas, ColorWhite);
    draw_word_wrapped_text(
        canvas,
        score_str,
        (PORTRAIT_WIDTH - strlen(score_str) * 6) / 2,
        26,
        PORTRAIT_WIDTH,
        FontSecondary);
    if(ctx->is_day) {
        canvas_draw_circle(canvas, 2, 10, 3);
    } else {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_circle(canvas, 2, 10, 3);
        canvas_set_color(canvas, ColorBlack);
    }
    draw_notification(canvas, ctx);
}

// Draw Flip Zip game with speed bar
static void draw_flip_zip(Canvas* canvas, GameContext* ctx) {
    if(!canvas || !ctx) return;
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 0, 0, PORTRAIT_WIDTH, PORTRAIT_HEIGHT);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, PORTRAIT_WIDTH, 26); // Larger box for score and streak
    canvas_set_color(canvas, ColorWhite);
    for(int i = 0; i < 5; i++) {
        canvas_draw_line(canvas, i * 12 + 2, 26, i * 12 + 2, PORTRAIT_HEIGHT - 5);
    }
    canvas_draw_line(canvas, 0, PORTRAIT_HEIGHT - 5, PORTRAIT_WIDTH, PORTRAIT_HEIGHT - 5);
    canvas_draw_line(canvas, 0, PORTRAIT_HEIGHT - 4, PORTRAIT_WIDTH * 4 / 5, PORTRAIT_HEIGHT - 4);
    canvas_draw_line(canvas, 0, PORTRAIT_HEIGHT - 3, PORTRAIT_WIDTH * 3 / 5, PORTRAIT_HEIGHT - 3);
    canvas_draw_line(canvas, 0, PORTRAIT_HEIGHT - 2, PORTRAIT_WIDTH * 2 / 5, PORTRAIT_HEIGHT - 2);
    canvas_draw_line(canvas, 0, PORTRAIT_HEIGHT - 1, PORTRAIT_WIDTH * 1 / 5, PORTRAIT_HEIGHT - 1);
    canvas_set_color(canvas, ColorBlack);
    const char* mascot_char = ctx->jump_scale > 0 ? "F" : "f";
    int mascot_y = PORTRAIT_HEIGHT - 7 - ctx->mascot_y -
                   (ctx->is_jumping ? (ctx->jump_progress * 10 / FIXED_POINT_SCALE) : 0);
    canvas_draw_str(canvas, ctx->mascot_lane * 12 + 4, mascot_y, mascot_char);
    for(int i = 0; i < 5; i++) {
        for(int j = 0; j < 10; j++) {
            if(ctx->obstacle_positions[i][j] > 0) {
                char symbol = ctx->obstacles[i][j] == 1 ? 'O' :
                              ctx->obstacles[i][j] == 2 ? '-' :
                                                          'S';
                canvas_draw_str(canvas, i * 12 + 4, ctx->obstacle_positions[i][j], &symbol);
            }
        }
    }
    char streak_str[32];
    snprintf(streak_str, sizeof(streak_str), "Streak: %d.%d", ctx->streak, ctx->oflow);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_color(canvas, ColorWhite);
    draw_word_wrapped_text(
        canvas,
        streak_str,
        (PORTRAIT_WIDTH - strlen(streak_str) * 6) / 2,
        17,
        PORTRAIT_WIDTH,
        FontSecondary);
    char score_str[32];
    snprintf(score_str, sizeof(score_str), "Score: %d.%d", ctx->score, ctx->score_oflow);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_color(canvas, ColorWhite);
    draw_word_wrapped_text(
        canvas,
        score_str,
        (PORTRAIT_WIDTH - strlen(score_str) * 6) / 2,
        26,
        PORTRAIT_WIDTH,
        FontSecondary);
    if(ctx->is_day) {
        canvas_draw_circle(canvas, 2, 10, 3);
    } else {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_circle(canvas, 2, 10, 3);
        canvas_set_color(canvas, ColorBlack);
    }
    // Draw speed bar
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, SPEED_BAR_X, SPEED_BAR_Y, SPEED_BAR_WIDTH, SPEED_BAR_HEIGHT);
    //canvas_set_color(canvas, ColorWhite);
    int reward_bpm_x = SPEED_BAR_X + (SPEED_BAR_WIDTH * 2 / 3); // 2/3 mark for reward BPM
    canvas_draw_line(
        canvas,
        reward_bpm_x,
        SPEED_BAR_Y - 2,
        reward_bpm_x,
        SPEED_BAR_Y + SPEED_BAR_HEIGHT + 1); // Reward BPM marker
    int speed_bpm = ctx->speed_bpm < MIN_SPEED_BPM ? MIN_SPEED_BPM : ctx->speed_bpm;
    int speed_bar_pos =
        SPEED_BAR_X + ((speed_bpm - MIN_SPEED_BPM) * SPEED_BAR_WIDTH) /
                          (ctx->speed_bpm - MIN_SPEED_BPM + 1); // Scale BPM to bar width
    canvas_draw_line(
        canvas, speed_bar_pos, SPEED_BAR_Y, speed_bar_pos, SPEED_BAR_Y + SPEED_BAR_HEIGHT - 1);
    draw_notification(canvas, ctx);
}

// Update Zero Hero game (AI-driven strumming)
static void update_zero_hero(GameContext* ctx) {
    if(!ctx) return;
    int fps = FPS_BASE + ctx->difficulty * 5;
    if(furi_get_tick() - ctx->last_ai_update < (uint32_t)(1000 / fps)) return;
    ctx->last_ai_update = furi_get_tick();
    for(int i = 0; i < 5; i++) {
        ctx->strum_hit[i] = false;
        for(int j = 0; j < 10; j++) {
            if(ctx->key_positions[i][j] > 0) {
                ctx->key_positions[i][j] += 1;
                if(ctx->key_positions[i][j] >= PORTRAIT_HEIGHT - 6 &&
                   ctx->key_positions[i][j] <= PORTRAIT_HEIGHT - 4) {
                    if(ctx->is_holding[i]) {
                        ctx->streak++;
                        ctx->score++;
                        ctx->key_positions[i][j] = 0;
                        ctx->strum_hit[i] = true;
                        if(ctx->streak >= MAX_STREAK_INT) {
                            ctx->streak = 0;
                            ctx->oflow++;
                        }
                        if(ctx->streak == 5) {
                            strcpy(ctx->notification_text, "! Perfect !");
                            ctx->last_notification_time = furi_get_tick();
                            ctx->notification_x =
                                (PORTRAIT_WIDTH - strlen(ctx->notification_text) * 6) / 2;
                        } else if(ctx->streak == 6) {
                            strcpy(ctx->notification_text, "! STREAK STARTED !");
                            ctx->last_notification_time = furi_get_tick();
                            ctx->notification_x =
                                (PORTRAIT_WIDTH - strlen(ctx->notification_text) * 6) / 2;
                        }
                        ctx->streak_sum += ctx->streak;
                        ctx->streak_count++;
                        if(ctx->streak > ctx->highest_streak) ctx->highest_streak = ctx->streak;
                    }
                } else if(ctx->key_positions[i][j] > PORTRAIT_HEIGHT - 5) {
                    ctx->key_positions[i][j] = 0;
                    ctx->streak = 0;
                    strcpy(ctx->notification_text, "! Miss !");
                    ctx->last_notification_time = furi_get_tick();
                    ctx->notification_x =
                        (PORTRAIT_WIDTH - strlen(ctx->notification_text) * 6) / 2;
                }
            }
        }
    }
    if(ctx->ai_beat_counter++ % 10 == 0) {
        int lane = rand() % 5;
        for(int j = 0; j < 10; j++) {
            if(ctx->key_positions[lane][j] == 0) {
                ctx->key_positions[lane][j] = 7;
                break;
            }
        }
    }
    if(furi_get_tick() - ctx->last_difficulty_check > COOLDOWN_MS && ctx->streak > 5) {
        int avg_streak = ctx->streak_count > 0 ? ctx->streak_sum / ctx->streak_count : 0;
        if(ctx->streak >= avg_streak * 3) {
            if(ctx->difficulty < DIFFICULTY_HARD) ctx->difficulty++;
            ctx->last_difficulty_check = furi_get_tick();
            int msg_idx =
                rand() % (sizeof(notification_messages) / sizeof(notification_messages[0]));
            strcpy(ctx->notification_text, notification_messages[msg_idx]);
            ctx->last_notification_time = furi_get_tick();
            ctx->notification_x = (PORTRAIT_WIDTH - strlen(ctx->notification_text) * 6) / 2;
        }
    }
}

// Update Flip Zip game (AI-driven speed, improved jump, tap DRM, and speed boost)
static void update_flip_zip(GameContext* ctx) {
    if(!ctx) return;
    int fps = FPS_BASE + (ctx->speed_bpm > 0 ? ctx->speed_bpm / 10 : 0);
    if(furi_get_tick() - ctx->last_ai_update < (uint32_t)(1000 / fps)) return;
    ctx->last_ai_update = furi_get_tick();
    int speed_modifier = 1 + ctx->speed_bpm / 60;
    for(int i = 0; i < 5; i++) {
        for(int j = 0; j < 10; j++) {
            if(ctx->obstacle_positions[i][j] > 0) {
                ctx->obstacle_positions[i][j] += speed_modifier;
                if(ctx->obstacle_positions[i][j] > PORTRAIT_HEIGHT - 7) {
                    ctx->obstacle_positions[i][j] = 0;
                    ctx->obstacles[i][j] = 0;
                    ctx->score++;
                    if(i == ctx->mascot_lane - 1 || i == ctx->mascot_lane + 1) {
                        ctx->successful_jumps++;
                        if(ctx->successful_jumps % 5 == 0) {
                            ctx->speed_bpm += 10;
                            if(ctx->speed_bpm > 120) ctx->speed_bpm = 120;
                        }
                    }
                }
            }
        }
    }
    if(ctx->ai_beat_counter++ % 15 == 0) {
        int lane = rand() % 5;
        int type = rand() % 3 + 1;
        for(int j = 0; j < 10; j++) {
            if(ctx->obstacle_positions[lane][j] == 0) {
                ctx->obstacle_positions[lane][j] = 7;
                ctx->obstacles[lane][j] = type;
                break;
            }
        }
    }
    if(ctx->is_jumping) {
        ctx->jump_progress += 100;
        if(ctx->jump_progress < FIXED_POINT_SCALE / 2) {
            ctx->jump_scale = ctx->jump_progress / (FIXED_POINT_SCALE / 4);
        } else if(ctx->jump_progress < FIXED_POINT_SCALE) {
            ctx->jump_scale = (FIXED_POINT_SCALE - ctx->jump_progress) / (FIXED_POINT_SCALE / 4);
        } else {
            if(ctx->jump_hold_time > 0 &&
               furi_get_tick() - ctx->jump_hold_time < (uint32_t)(ctx->speed_bpm * 250)) {
                ctx->jump_progress = FIXED_POINT_SCALE / 2;
                ctx->jump_scale = 1;
            } else {
                ctx->is_jumping = false;
                ctx->jump_progress = 0;
                ctx->jump_scale = 0;
                ctx->successful_jumps++;
                ctx->mascot_y += ctx->jump_y_accumulated; // Apply accumulated Up presses
                if(ctx->mascot_y > 20) ctx->mascot_y = 20; // Cap max height
                ctx->jump_y_accumulated = 0; // Reset after landing
                if(ctx->successful_jumps % 5 == 0) {
                    ctx->speed_bpm += 10;
                    if(ctx->speed_bpm > 120) ctx->speed_bpm = 120;
                }
            }
        }
        // Move forward 1 pixel per 10ms while airborne
        if(ctx->jump_scale > 0) {
            uint32_t airborne_time = furi_get_tick() - ctx->jump_hold_time;
            ctx->mascot_y += airborne_time / 10;
            if(ctx->mascot_y > 20) ctx->mascot_y = 20; // Cap max height
        }
    }
}

static void input_callback(InputEvent* input, void* ctx_ptr) {
    GameContext* ctx = ctx_ptr;
    if(!ctx) return;
    uint32_t now = furi_get_tick();
    if(now - ctx->last_input_time < TAP_DRM_MS)
        ctx->rapid_click_count++;
    else {
        ctx->rapid_click_count = 1;
        ctx->tap_count = 0;
        ctx->tap_window_start = now;
    }
    ctx->last_input_time = now;
    bool is_press = input->type == InputTypePress;
    bool is_release = input->type == InputTypeRelease;
    bool is_short = input->type == InputTypeShort;

    if(input->key == InputKeyBack && (now - ctx->last_back_press_time < BACK_BUTTON_COOLDOWN)) {
        return;
    }
    if(input->key == InputKeyBack && is_short) {
        ctx->last_back_press_time = now;
    }

    if(ctx->state == GAME_STATE_LOADING) {
        // No input during loading
    } else if(ctx->state == GAME_STATE_TITLE) {
        if(is_short && input->key == InputKeyLeft) {
            ctx->selected_side = 0;
        } else if(is_short && input->key == InputKeyRight) {
            ctx->selected_side = 1;
        } else if(is_short && input->key == InputKeyUp && ctx->selected_row > 0) {
            ctx->selected_row--;
        } else if(is_short && input->key == InputKeyDown && ctx->selected_row < 2) {
            ctx->selected_row++;
        } else if(is_short && input->key == InputKeyOk) {
            ctx->selected_game = ctx->selected_row * 2 + ctx->selected_side;
            ctx->state = GAME_STATE_ROTATE;
            ctx->rotate_start_time = now;
            ctx->rotate_angle = 0;
            ctx->zoom_factor = FIXED_POINT_SCALE;
            ctx->rotate_skip = false;
        } else if(is_short && input->key == InputKeyBack && ctx->note_q_a == 0) {
            ctx->start_back_count++;
            if(ctx->start_back_count >= 3) { // Changed from 7 to 3
                ctx->state = GAME_STATE_CREDITS;
                ctx->credits_y =
                    SCREEN_HEIGHT +
                    10 * ((int)(sizeof(credits_lines) / sizeof(credits_lines[0])) - 1);
                ctx->start_back_count = 0;
            }
        } else if(is_press && input->key == InputKeyBack) {
            ctx->back_hold_start = now;
        } else if(is_release && input->key == InputKeyBack && ctx->back_hold_start > 0) {
            if(now - ctx->back_hold_start >= ORIENTATION_HOLD_MS) {
                ctx->is_left_handed = !ctx->is_left_handed;
            }
            ctx->back_hold_start = 0;
        }
    } else if(ctx->state == GAME_STATE_ROTATE) {
        if(is_press) {
            ctx->rotate_skip = true;
            ctx->state = ctx->selected_game == GAME_MODE_ZERO_HERO ?
                             GAME_STATE_ZERO_HERO :
                         ctx->selected_game == GAME_MODE_FLIP_ZIP ?
                             GAME_STATE_FLIP_ZIP :
                             GAME_STATE_ZERO_HERO; // Placeholder for new games
            ctx->streak = 0; // Initialize streak to 0
            ctx->game_start_time = now;
            ctx->day_night_toggle_time = now + 300000;
            ctx->is_day = true;
        }
    } else if(ctx->state == GAME_STATE_ZERO_HERO) {
        if(is_short && input->key == InputKeyBack) {
            ctx->state = GAME_STATE_PAUSE;
            ctx->pause_back_count = 0;
            ctx->back_hold_start = 0;
        } else if(is_press && input->key == InputKeyBack) {
            ctx->back_hold_start = now;
        } else if(input->key == InputKeyBack && now - ctx->back_hold_start >= ORIENTATION_HOLD_MS) {
            ctx->is_left_handed = !ctx->is_left_handed; // Flip orientation even if held
        } else {
            int key_idx = input->key == InputKeyUp    ? 0 :
                          input->key == InputKeyLeft  ? 1 :
                          input->key == InputKeyOk    ? 2 :
                          input->key == InputKeyRight ? 3 :
                          input->key == InputKeyDown  ? 4 :
                                                        -1;
            if(key_idx >= 0) ctx->is_holding[key_idx] = is_press;
        }
    } else if(ctx->state == GAME_STATE_FLIP_ZIP) {
        if(is_short && input->key == InputKeyBack) {
            ctx->state = GAME_STATE_PAUSE;
            ctx->pause_back_count = 0;
            ctx->back_hold_start = 0;
        } else if(is_press && input->key == InputKeyBack) {
            ctx->back_hold_start = now;
        } else if(input->key == InputKeyBack && now - ctx->back_hold_start >= ORIENTATION_HOLD_MS) {
            ctx->is_left_handed = !ctx->is_left_handed; // Flip orientation even if held
        } else {
            if(is_short && input->key == InputKeyLeft && ctx->mascot_lane > 0) {
                ctx->mascot_lane--;
                // Update tap count for BPM
                ctx->tap_count++;
                if(now - ctx->tap_window_start >= 60000) { // 1 minute window
                    ctx->tap_count = 1;
                    ctx->tap_window_start = now;
                }
                // Check if tap rate matches reward BPM
                float tap_bpm = (ctx->tap_count * 60000.0f) / (now - ctx->tap_window_start + 1);
                if(fabs(tap_bpm - ctx->speed_bpm) < 5) { // Within 5 BPM of reward
                    ctx->speed_bpm += 10; // Speed boost
                    if(ctx->speed_bpm > 120) ctx->speed_bpm = 120;
                }
            }
            if(is_short && input->key == InputKeyRight && ctx->mascot_lane < 4) {
                ctx->mascot_lane++;
                ctx->tap_count++;
                if(now - ctx->tap_window_start >= 60000) {
                    ctx->tap_count = 1;
                    ctx->tap_window_start = now;
                }
                float tap_bpm = (ctx->tap_count * 60000.0f) / (now - ctx->tap_window_start + 1);
                if(fabs(tap_bpm - ctx->speed_bpm) < 5) {
                    ctx->speed_bpm += 10;
                    if(ctx->speed_bpm > 120) ctx->speed_bpm = 120;
                }
            }
            if(is_short && input->key == InputKeyUp && ctx->mascot_y < 20) {
                ctx->mascot_y++;
                if(ctx->is_jumping) {
                    ctx->jump_y_accumulated++; // Accumulate Up presses during jump
                }
            }
            if(is_short && input->key == InputKeyDown && ctx->mascot_y > 0) {
                ctx->mascot_y--;
            }
            if(is_press && input->key == InputKeyOk && !ctx->is_jumping) {
                ctx->is_jumping = true;
                ctx->jump_progress = 0;
                ctx->jump_scale = 0;
                ctx->jump_hold_time = now;
                ctx->jump_y_accumulated = 0; // Reset on new jump
            } else if(is_release && input->key == InputKeyOk) {
                ctx->jump_hold_time = 0;
            }
        }
    } else if(ctx->state == GAME_STATE_PAUSE) {
        if(is_short && input->key == InputKeyOk) {
            ctx->state = ctx->selected_game == GAME_MODE_ZERO_HERO ? GAME_STATE_ZERO_HERO :
                         ctx->selected_game == GAME_MODE_FLIP_ZIP  ? GAME_STATE_FLIP_ZIP :
                                                                     GAME_STATE_ZERO_HERO;
            ctx->pause_back_count = 0;
        } else if(is_short && input->key == InputKeyBack) {
            ctx->pause_back_count++;
            if(ctx->pause_back_count >= 2) {
                ctx->state = GAME_STATE_TITLE;
                ctx->pause_back_count = 0;
            }
        } else if(is_press && input->key == InputKeyBack) {
            ctx->back_hold_start = now;
        } else if(input->key == InputKeyBack && now - ctx->back_hold_start >= ORIENTATION_HOLD_MS) {
            ctx->is_left_handed = !ctx->is_left_handed; // Flip orientation even if held
        } else if(is_release && input->key == InputKeyBack && ctx->back_hold_start > 0) {
            ctx->back_hold_start = 0;
        }
    } else if(ctx->state == GAME_STATE_CREDITS) {
        if(is_short && input->key == InputKeyBack) {
            ctx->should_exit = true;
        }
    }
}

static void render_callback(Canvas* canvas, void* ctx_ptr) {
    GameContext* ctx = ctx_ptr;
    if(!ctx || !ctx->view_port || !canvas) return;
    canvas_clear(canvas);
    if(ctx->state == GAME_STATE_LOADING) {
        view_port_set_orientation(ctx->view_port, ViewPortOrientationHorizontal);
        draw_loading_screen(canvas);
    } else if(ctx->state == GAME_STATE_TITLE) {
        view_port_set_orientation(
            ctx->view_port,
            ctx->is_left_handed ? ViewPortOrientationHorizontalFlip :
                                  ViewPortOrientationHorizontal);
        draw_title_menu(canvas, ctx);
    } else if(ctx->state == GAME_STATE_ROTATE) {
        draw_rotate_screen(canvas, ctx);
    } else if(ctx->state == GAME_STATE_CREDITS) {
        view_port_set_orientation(
            ctx->view_port,
            ctx->is_left_handed ? ViewPortOrientationHorizontalFlip :
                                  ViewPortOrientationHorizontal);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        canvas_set_color(canvas, ColorWhite);
        for(size_t i = 0; i < sizeof(credits_lines) / sizeof(credits_lines[0]); i++) {
            int y = ctx->credits_y - i * 10;
            if(y > -10 && y < SCREEN_HEIGHT) {
                int text_width = strlen(credits_lines[i]) * 8;
                int x = (SCREEN_WIDTH - text_width) / 2;
                draw_word_wrapped_text(
                    canvas, credits_lines[i], x, y, SCREEN_WIDTH - 20, FontPrimary);
            }
        }
    } else if(ctx->state == GAME_STATE_PAUSE) {
        view_port_set_orientation(
            ctx->view_port,
            ctx->is_left_handed ? ViewPortOrientationVerticalFlip : ViewPortOrientationVertical);
        draw_pause_screen(canvas);
    } else {
        view_port_set_orientation(
            ctx->view_port,
            ctx->is_left_handed ? ViewPortOrientationVerticalFlip : ViewPortOrientationVertical);
        if(ctx->state == GAME_STATE_ZERO_HERO) {
            draw_zero_hero(canvas, ctx);
        } else {
            draw_flip_zip(canvas, ctx);
        }
    }
}

static void timer_callback(void* ctx_ptr) {
    GameContext* ctx = ctx_ptr;
    if(!ctx) return;
    uint32_t now = furi_get_tick();
    ctx->frame_counter = (ctx->frame_counter + 1) % 3;

    // Faux multithreading: 3-frame cycle
    if(ctx->frame_counter == 0) {
        // Frame 1: Process input (handled in input_callback)
    } else if(ctx->frame_counter == 1) {
        // Frame 2: Process notification text
        if(ctx->notification_text[0] != '\0' && ctx->note_q_a == 0) {
            uint32_t elapsed = now - ctx->last_notification_time;
            if(elapsed < NOTIFICATION_MS) {
                int text_width = strlen(ctx->notification_text) * 6;
                ctx->notification_x =
                    (PORTRAIT_WIDTH - text_width) / 2 - (elapsed * text_width / NOTIFICATION_MS);
                if(ctx->notification_x < -text_width) ctx->notification_x += text_width;
            } else {
                ctx->notification_text[0] = '\0';
                ctx->notification_x = 0;
            }
        }
    } else if(ctx->frame_counter == 2) {
        // Frame 3: Process game updates
        if(ctx->state == GAME_STATE_ZERO_HERO) {
            update_zero_hero(ctx);
        } else if(ctx->state == GAME_STATE_FLIP_ZIP) {
            update_flip_zip(ctx);
        }
    }

    // Common updates
    if(now > ctx->day_night_toggle_time) {
        ctx->is_day = !ctx->is_day;
        ctx->day_night_toggle_time = now + 300000;
    }
    if(ctx->state == GAME_STATE_CREDITS) {
        static uint32_t last_credits_update = 0;
        if(now - last_credits_update >= 85) {
            ctx->credits_y--;
            last_credits_update = now;
        }
        if(ctx->credits_y < -10) {
            ctx->should_exit = true;
        }
    }
}

int32_t nah2nah3_app(void* p) {
    UNUSED(p);
    // Allocate game context
    GameContext* ctx = malloc(sizeof(GameContext));
    if(!ctx) return -1;
    memset(ctx, 0, sizeof(GameContext));
    ctx->state = GAME_STATE_LOADING;
    ctx->game_start_time = furi_get_tick();
    ctx->is_day = true;
    ctx->day_night_toggle_time = furi_get_tick() + 300000;
    ctx->mascot_lane = 2;
    ctx->streak = 0; // Initialize streak to 0
    srand(furi_get_tick());

    // Initialize GUI with extended delay for stability
    Gui* gui = furi_record_open(RECORD_GUI);
    if(!gui) {
        free(ctx);
        return -1;
    }
    ViewPort* view_port = view_port_alloc();
    if(!view_port) {
        furi_record_close(RECORD_GUI);
        free(ctx);
        return -1;
    }
    ctx->view_port = view_port;
    view_port_draw_callback_set(view_port, render_callback, ctx);
    view_port_input_callback_set(view_port, input_callback, ctx);
    view_port_set_orientation(view_port, ViewPortOrientationHorizontal);
    furi_delay_ms(500); // Extended delay to stabilize GUI
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    furi_delay_ms(100); // Additional delay post-add

    // Start timer
    FuriTimer* timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, ctx);
    if(!timer) {
        view_port_free(view_port);
        furi_record_close(RECORD_GUI);
        free(ctx);
        return -1;
    }
    if(furi_timer_start(timer, 1000 / FPS_BASE) != FuriStatusOk) {
        furi_timer_free(timer);
        view_port_free(view_port);
        furi_record_close(RECORD_GUI);
        free(ctx);
        return -1;
    }

    // Main loop with loading screen transition
    while(!ctx->should_exit) {
        if(ctx->state == GAME_STATE_LOADING &&
           furi_get_tick() - ctx->game_start_time >= LOADING_MS) {
            ctx->state = GAME_STATE_TITLE;
            ctx->selected_side = 0;
            ctx->selected_row = 0;
            ctx->title_scroll_offset = 0;
            view_port_input_callback_set(ctx->view_port, input_callback, ctx); // Re-register input
        }
        furi_delay_ms(100);
    }

    // Cleanup
    if(timer) {
        furi_timer_stop(timer);
        furi_timer_free(timer);
    }
    if(view_port) {
        gui_remove_view_port(gui, view_port);
        view_port_draw_callback_set(view_port, NULL, NULL);
        view_port_input_callback_set(view_port, NULL, NULL);
        view_port_free(view_port);
    }
    if(gui) {
        furi_record_close(RECORD_GUI);
    }
    if(ctx) {
        free(ctx);
    }
    return 0;
}
