#include "holdem_app_internal.h"

// Stage labels are rendered in the footer and result views, so keep them short and stable.
const char* k_stage_name[] = {"PREFLOP", "FLOP", "TURN", "RIVER", "SHOWDOWN"};

void holdem_draw_line(Canvas* canvas, uint8_t y, const char* text) {
    holdem_draw_display_text(canvas, 0, y, text);
}

void holdem_draw_line_right(Canvas* canvas, uint8_t y, const char* text) {
    uint8_t text_width = holdem_display_text_width(canvas, text);
    uint8_t draw_x = (text_width >= 128u) ? 0u : (uint8_t)(128u - text_width);
    holdem_draw_display_text(canvas, draw_x, y, text);
}

#define HOLDEM_BACK_ICON_SIZE     8u
#define HOLDEM_OK_ICON_SIZE       6u
#define HOLDEM_CHIP_ICON_SIZE     7u
#define HOLDEM_UPDOWN_ICON_WIDTH  7u
#define HOLDEM_UPDOWN_ICON_HEIGHT 7u
#define HOLDEM_LR_ICON_SIZE       7u
#define HOLDEM_FF_ICON_SIZE       7u

static const uint8_t k_holdem_back_icon[HOLDEM_BACK_ICON_SIZE] =
    {0x20u, 0x60u, 0xFCu, 0x62u, 0x21u, 0x01u, 0x02u, 0x7Cu};

static const uint8_t k_holdem_ok_icon[HOLDEM_OK_ICON_SIZE] =
    {0x1Eu, 0x3Fu, 0x3Fu, 0x3Fu, 0x3Fu, 0x1Eu};
static const uint8_t k_holdem_chip_icon[HOLDEM_CHIP_ICON_SIZE] =
    {0x08u, 0x1Eu, 0x28u, 0x1Cu, 0x0Au, 0x3Cu, 0x08u};

// Compact triangle-style navigation glyphs are the shared control language across the app.
static const uint8_t k_holdem_updown_icon[HOLDEM_UPDOWN_ICON_HEIGHT] =
    {0x18u, 0x3Cu, 0x7Eu, 0x00u, 0x7Eu, 0x3Cu, 0x18u};
static const uint8_t k_holdem_up_icon[HOLDEM_UPDOWN_ICON_HEIGHT] =
    {0x18u, 0x3Cu, 0x7Eu, 0x00u, 0x00u, 0x00u, 0x00u};
static const uint8_t k_holdem_down_icon[HOLDEM_UPDOWN_ICON_HEIGHT] =
    {0x00u, 0x00u, 0x7Eu, 0x3Cu, 0x18u, 0x00u, 0x00u};
static const uint8_t k_holdem_left_icon[HOLDEM_LR_ICON_SIZE] =
    {0x04u, 0x0Cu, 0x1Cu, 0x1Cu, 0x0Cu, 0x04u, 0x00u};
static const uint8_t k_holdem_right_icon[HOLDEM_LR_ICON_SIZE] =
    {0x10u, 0x18u, 0x1Cu, 0x1Cu, 0x18u, 0x10u, 0x00u};
static const uint8_t k_holdem_ff_icon[HOLDEM_FF_ICON_SIZE] =
    {0x00u, 0x44u, 0x66u, 0x77u, 0x66u, 0x44u, 0x00u};

static void holdem_draw_bitmap_glyph(
    Canvas* canvas,
    uint8_t x,
    uint8_t top_y,
    const uint8_t* rows,
    uint8_t width,
    uint8_t height) {
    for(uint8_t row = 0; row < height; row++) {
        uint8_t row_bits = rows[row];
        for(uint8_t col = 0; col < width; col++) {
            if(row_bits & (uint8_t)(1u << (width - 1u - col))) {
                canvas_draw_dot(canvas, x + col, top_y + row);
            }
        }
    }
}

void holdem_draw_back_icon(Canvas* canvas, uint8_t x, uint8_t baseline_y) {
    holdem_draw_bitmap_glyph(
        canvas,
        x,
        (uint8_t)(baseline_y - HOLDEM_BACK_ICON_SIZE),
        k_holdem_back_icon,
        HOLDEM_BACK_ICON_SIZE,
        HOLDEM_BACK_ICON_SIZE);
}

void holdem_draw_ok_icon(Canvas* canvas, uint8_t x, uint8_t baseline_y) {
    holdem_draw_bitmap_glyph(
        canvas,
        x,
        (uint8_t)(baseline_y - HOLDEM_OK_ICON_SIZE),
        k_holdem_ok_icon,
        HOLDEM_OK_ICON_SIZE,
        HOLDEM_OK_ICON_SIZE);
}

void holdem_draw_chip_icon(Canvas* canvas, uint8_t x, uint8_t baseline_y) {
    holdem_draw_bitmap_glyph(
        canvas,
        x,
        (uint8_t)(baseline_y - HOLDEM_CHIP_ICON_SIZE),
        k_holdem_chip_icon,
        HOLDEM_CHIP_ICON_SIZE,
        HOLDEM_CHIP_ICON_SIZE);
}

uint8_t holdem_display_text_width(Canvas* canvas, const char* text) {
    if(!text || !text[0]) return 0u;

    uint8_t total_width = 0u;
    char run[64];
    size_t run_length = 0u;

    for(size_t text_index = 0; text[text_index]; text_index++) {
        if(text[text_index] == '$') {
            if(run_length > 0u) {
                run[run_length] = '\0';
                total_width = (uint8_t)(total_width + canvas_string_width(canvas, run));
                run_length = 0u;
            }
            total_width = (uint8_t)(total_width + HOLDEM_CHIP_ICON_SIZE);
        } else if(run_length + 1u < sizeof(run)) {
            run[run_length++] = text[text_index];
        }
    }

    if(run_length > 0u) {
        run[run_length] = '\0';
        total_width = (uint8_t)(total_width + canvas_string_width(canvas, run));
    }

    return total_width;
}

void holdem_draw_display_text(Canvas* canvas, uint8_t x, uint8_t baseline_y, const char* text) {
    if(!text || !text[0]) return;

    uint8_t draw_x = x;
    char run[64];
    size_t run_length = 0u;

    for(size_t text_index = 0; text[text_index]; text_index++) {
        if(text[text_index] == '$') {
            if(run_length > 0u) {
                run[run_length] = '\0';
                canvas_draw_str(canvas, draw_x, baseline_y, run);
                draw_x = (uint8_t)(draw_x + canvas_string_width(canvas, run));
                run_length = 0u;
            }
            holdem_draw_chip_icon(canvas, draw_x, baseline_y);
            draw_x = (uint8_t)(draw_x + HOLDEM_CHIP_ICON_SIZE);
        } else if(run_length + 1u < sizeof(run)) {
            run[run_length++] = text[text_index];
        }
    }

    if(run_length > 0u) {
        run[run_length] = '\0';
        canvas_draw_str(canvas, draw_x, baseline_y, run);
    }
}

uint8_t holdem_centered_x_for_width(uint8_t width) {
    if(width >= 128u) return 0u;
    return (uint8_t)((128u - width + 1u) / 2u);
}

void holdem_draw_text_centered(Canvas* canvas, uint8_t baseline_y, const char* text) {
    const char* safe_text = text ? text : "";
    uint8_t draw_x = holdem_centered_x_for_width(holdem_display_text_width(canvas, safe_text));
    holdem_draw_display_text(canvas, draw_x, baseline_y, safe_text);
}

void holdem_draw_centered_inline_lr_text(
    Canvas* canvas,
    uint8_t baseline_y,
    const char* prefix,
    bool left_present,
    const char* middle,
    bool right_present,
    const char* suffix) {
    const char* safe_prefix = prefix ? prefix : "";
    const char* safe_middle = middle ? middle : "";
    const char* safe_suffix = suffix ? suffix : "";
    uint8_t prefix_width = holdem_display_text_width(canvas, safe_prefix);
    uint8_t middle_width = holdem_display_text_width(canvas, safe_middle);
    uint8_t suffix_width = holdem_display_text_width(canvas, safe_suffix);
    uint8_t total_width =
        (uint8_t)(prefix_width + (left_present ? HOLDEM_LR_ICON_SIZE : 0u) + middle_width +
                  (right_present ? HOLDEM_LR_ICON_SIZE : 0u) + suffix_width);
    uint8_t draw_x = holdem_centered_x_for_width(total_width);

    if(prefix_width > 0u) {
        holdem_draw_display_text(canvas, draw_x, baseline_y, safe_prefix);
        draw_x = (uint8_t)(draw_x + prefix_width);
    }
    if(left_present) {
        holdem_draw_left_icon(canvas, draw_x, baseline_y);
        draw_x = (uint8_t)(draw_x + HOLDEM_LR_ICON_SIZE);
    }
    if(middle_width > 0u) {
        holdem_draw_display_text(canvas, draw_x, baseline_y, safe_middle);
        draw_x = (uint8_t)(draw_x + middle_width);
    }
    if(right_present) {
        holdem_draw_right_icon(canvas, draw_x, baseline_y);
        draw_x = (uint8_t)(draw_x + HOLDEM_LR_ICON_SIZE);
    }
    if(suffix_width > 0u) {
        holdem_draw_display_text(canvas, draw_x, baseline_y, safe_suffix);
    }
}

uint8_t holdem_ok_sequence_width_with_gap(
    Canvas* canvas,
    const char* before_text,
    const char* after_text,
    uint8_t after_gap_px) {
    uint8_t before_width =
        (before_text && before_text[0]) ? holdem_display_text_width(canvas, before_text) : 0u;
    uint8_t after_width =
        (after_text && after_text[0]) ? holdem_display_text_width(canvas, after_text) : 0u;
    uint8_t gap_width = (after_text && after_text[0]) ? after_gap_px : 0u;
    return (uint8_t)(before_width + HOLDEM_OK_ICON_SIZE + gap_width + after_width);
}

uint8_t holdem_ok_sequence_width(Canvas* canvas, const char* before_text, const char* after_text) {
    return holdem_ok_sequence_width_with_gap(canvas, before_text, after_text, 0u);
}

void holdem_draw_ok_sequence_with_gap(
    Canvas* canvas,
    uint8_t x,
    uint8_t baseline_y,
    const char* before_text,
    const char* after_text,
    uint8_t after_gap_px) {
    uint8_t draw_x = x;

    if(before_text && before_text[0]) {
        holdem_draw_display_text(canvas, draw_x, baseline_y, before_text);
        draw_x = (uint8_t)(draw_x + holdem_display_text_width(canvas, before_text));
    }

    holdem_draw_ok_icon(canvas, draw_x, baseline_y);
    draw_x = (uint8_t)(draw_x + HOLDEM_OK_ICON_SIZE);

    if(after_text && after_text[0]) {
        draw_x = (uint8_t)(draw_x + after_gap_px);
        holdem_draw_display_text(canvas, draw_x, baseline_y, after_text);
    }
}

void holdem_draw_ok_sequence(
    Canvas* canvas,
    uint8_t x,
    uint8_t baseline_y,
    const char* before_text,
    const char* after_text) {
    holdem_draw_ok_sequence_with_gap(canvas, x, baseline_y, before_text, after_text, 0u);
}

void holdem_draw_ok_sequence_right(
    Canvas* canvas,
    uint8_t baseline_y,
    const char* before_text,
    const char* after_text) {
    uint8_t total_width = holdem_ok_sequence_width(canvas, before_text, after_text);
    uint8_t draw_x = (uint8_t)(128u - total_width);
    holdem_draw_ok_sequence(canvas, draw_x, baseline_y, before_text, after_text);
}

void holdem_draw_ok_sequence_center_with_gap(
    Canvas* canvas,
    uint8_t center_x,
    uint8_t baseline_y,
    const char* before_text,
    const char* after_text,
    uint8_t after_gap_px) {
    uint8_t total_width =
        holdem_ok_sequence_width_with_gap(canvas, before_text, after_text, after_gap_px);
    uint8_t draw_x = (total_width >= 128u) ? 0u : (uint8_t)(center_x - (total_width / 2u));
    holdem_draw_ok_sequence_with_gap(
        canvas, draw_x, baseline_y, before_text, after_text, after_gap_px);
}

void holdem_draw_left_icon(Canvas* canvas, uint8_t x, uint8_t baseline_y) {
    holdem_draw_bitmap_glyph(
        canvas,
        x,
        (uint8_t)(baseline_y - HOLDEM_LR_ICON_SIZE),
        k_holdem_left_icon,
        HOLDEM_LR_ICON_SIZE,
        HOLDEM_LR_ICON_SIZE);
}

void holdem_draw_right_icon(Canvas* canvas, uint8_t x, uint8_t baseline_y) {
    holdem_draw_bitmap_glyph(
        canvas,
        x,
        (uint8_t)(baseline_y - HOLDEM_LR_ICON_SIZE),
        k_holdem_right_icon,
        HOLDEM_LR_ICON_SIZE,
        HOLDEM_LR_ICON_SIZE);
}

void holdem_draw_ff_icon(Canvas* canvas, uint8_t x, uint8_t baseline_y) {
    holdem_draw_bitmap_glyph(
        canvas,
        x,
        (uint8_t)(baseline_y - HOLDEM_FF_ICON_SIZE),
        k_holdem_ff_icon,
        HOLDEM_FF_ICON_SIZE,
        HOLDEM_FF_ICON_SIZE);
}

void holdem_draw_ok_ff_hint(Canvas* canvas, uint8_t baseline_y) {
    uint8_t total_width = HOLDEM_OK_ICON_SIZE + 1u + HOLDEM_FF_ICON_SIZE;
    uint8_t draw_x = (uint8_t)(128u - total_width);

    holdem_draw_ok_icon(canvas, draw_x, baseline_y);
    holdem_draw_ff_icon(canvas, (uint8_t)(draw_x + HOLDEM_OK_ICON_SIZE + 1u), baseline_y);
}

void holdem_draw_nav_glyph(Canvas* canvas, uint8_t x, uint8_t baseline_y, HoldemNavGlyph glyph) {
    const uint8_t* icon_rows = k_holdem_updown_icon;
    uint8_t icon_baseline_y = baseline_y;

    if(glyph == HoldemNavGlyphDown) {
        icon_rows = k_holdem_down_icon;
    } else if(glyph == HoldemNavGlyphUp) {
        icon_rows = k_holdem_up_icon;
        icon_baseline_y = (uint8_t)(baseline_y + 2u);
    }

    holdem_draw_bitmap_glyph(
        canvas,
        x,
        (uint8_t)(icon_baseline_y - HOLDEM_UPDOWN_ICON_HEIGHT),
        icon_rows,
        HOLDEM_UPDOWN_ICON_WIDTH,
        HOLDEM_UPDOWN_ICON_HEIGHT);
}

void holdem_draw_nav_icon(
    Canvas* canvas,
    uint8_t x,
    uint8_t baseline_y,
    uint8_t page,
    uint8_t total_pages) {
    HoldemNavGlyph glyph = HoldemNavGlyphUpDown;

    if(page == 0) {
        glyph = HoldemNavGlyphDown;
    } else if(page + 1u >= total_pages) {
        glyph = HoldemNavGlyphUp;
    }

    holdem_draw_nav_glyph(canvas, x, baseline_y, glyph);
}

void holdem_draw_back_sequence(
    Canvas* canvas,
    uint8_t x,
    uint8_t baseline_y,
    const char* before_text,
    const char* after_text) {
    uint8_t draw_x = x;

    if(before_text && before_text[0]) {
        canvas_draw_str(canvas, draw_x, baseline_y, before_text);
        draw_x = (uint8_t)(draw_x + canvas_string_width(canvas, before_text));
    }

    holdem_draw_back_icon(canvas, draw_x, baseline_y);
    draw_x = (uint8_t)(draw_x + HOLDEM_BACK_ICON_SIZE);

    if(after_text && after_text[0]) {
        canvas_draw_str(canvas, draw_x, baseline_y, after_text);
    }
}

void holdem_draw_back_sequence_right(
    Canvas* canvas,
    uint8_t baseline_y,
    const char* before_text,
    const char* after_text) {
    uint8_t before_width =
        (before_text && before_text[0]) ? holdem_display_text_width(canvas, before_text) : 0u;
    uint8_t after_width =
        (after_text && after_text[0]) ? holdem_display_text_width(canvas, after_text) : 0u;
    uint8_t total_width = (uint8_t)(before_width + HOLDEM_BACK_ICON_SIZE + after_width);
    uint8_t draw_x = (uint8_t)(128u - total_width);

    holdem_draw_back_sequence(canvas, draw_x, baseline_y, before_text, after_text);
}

void holdem_draw_back_cancel_hint(Canvas* canvas, uint8_t baseline_y) {
    holdem_draw_back_sequence_right(canvas, baseline_y, "Back ", "");
}

const char* holdem_ai_label(uint8_t level_pct) {
    if(level_pct >= HOLDEM_AI_EXTREME_LEVEL) return "Extreme";
    if(level_pct >= HOLDEM_AI_HARD_LEVEL) return "Hard";
    if(level_pct <= HOLDEM_AI_EASY_LEVEL) return "Easy";
    return "Medium";
}

uint8_t holdem_prev_ai_level(uint8_t level_pct) {
    if(level_pct >= HOLDEM_AI_EXTREME_LEVEL) return HOLDEM_AI_HARD_LEVEL;
    if(level_pct >= HOLDEM_AI_HARD_LEVEL) return HOLDEM_AI_MEDIUM_LEVEL;
    if(level_pct > HOLDEM_AI_EASY_LEVEL) return HOLDEM_AI_EASY_LEVEL;
    return HOLDEM_AI_EASY_LEVEL;
}

uint8_t holdem_next_ai_level(uint8_t level_pct) {
    if(level_pct < HOLDEM_AI_MEDIUM_LEVEL) return HOLDEM_AI_MEDIUM_LEVEL;
    if(level_pct < HOLDEM_AI_HARD_LEVEL) return HOLDEM_AI_HARD_LEVEL;
    if(level_pct < HOLDEM_AI_EXTREME_LEVEL) return HOLDEM_AI_EXTREME_LEVEL;
    return HOLDEM_AI_EXTREME_LEVEL;
}

bool holdem_is_menu_mode(UiMode mode) {
    return mode == UiModeHelp || mode == UiModeBlindEdit || mode == UiModeBotCountEdit ||
           mode == UiModeNewGameConfirm || mode == UiModeExitPrompt;
}

void holdem_set_foreground_mode(HoldemApp* app, UiMode mode) {
    if(holdem_is_menu_mode(app->mode)) {
        app->return_mode = mode;
    } else {
        app->mode = mode;
    }
}

const char* holdem_display_name(const Player* player) {
    if(!player->is_bot) return "You";
    return player->name;
}

bool holdem_player_is_eliminated(const HoldemGame* game, const Player* player) {
    if(player->stack > 0) return false;
    return !game->hand_in_progress || !player->in_hand;
}

char holdem_role_char(const HoldemGame* game, int player_index) {
    if(player_index == game->button) return 'D';
    if(player_index == game->sb_idx) return 'S';
    if(player_index == game->bb_idx) return 'B';
    return '-';
}

char holdem_status_char(const HoldemGame* game, const Player* player) {
    if(holdem_player_is_eliminated(game, player)) return 'X';
    if(!player->in_hand) return 'F';
    if(player->all_in) return 'A';
    return 'P';
}

void holdem_build_display_order(const HoldemGame* game, size_t order[HOLDEM_MAX_PLAYERS]) {
    size_t write_index = 0;

    for(size_t player_index = 0; player_index < game->player_count; player_index++) {
        const Player* player = &game->players[player_index];
        if(player->stack > 0 || player->in_hand) order[write_index++] = player_index;
    }

    for(size_t player_index = 0; player_index < game->player_count; player_index++) {
        const Player* player = &game->players[player_index];
        if(player->stack <= 0 && !player->in_hand) order[write_index++] = player_index;
    }
}
