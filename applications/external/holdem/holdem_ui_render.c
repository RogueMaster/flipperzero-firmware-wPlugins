#include "holdem_app_internal.h"

#define HOLDEM_CARD_SLOT_WIDTH        16u
#define HOLDEM_SUIT_ICON_WIDTH        8u
#define HOLDEM_SUIT_BITMAP_SIZE       7u
#define HOLDEM_CARD_AREA_START_X      96u
#define HOLDEM_HIDDEN_TOKEN_EXTRA_GAP 1u
#define HOLDEM_TURN_MARK_X            0u
#define HOLDEM_NAME_X                 6u
#define HOLDEM_ROLE_X                 37u
#define HOLDEM_STATUS_X               43u
#define HOLDEM_STACK_DOLLAR_X         58u
#define HOLDEM_STACK_VALUE_X          65u
#define HOLDEM_UPDOWN_ICON_WIDTH      7u
#define HOLDEM_LR_ICON_SIZE           7u

static const uint8_t k_holdem_suit_bitmap[4][HOLDEM_SUIT_BITMAP_SIZE] = {
    {0x1Cu, 0x1Cu, 0x6Bu, 0x7Fu, 0x6Bu, 0x08u, 0x1Cu},
    {0x08u, 0x14u, 0x22u, 0x41u, 0x22u, 0x14u, 0x08u},
    {0x22u, 0x55u, 0x49u, 0x41u, 0x22u, 0x14u, 0x08u},
    {0x08u, 0x1Cu, 0x3Eu, 0x7Fu, 0x6Bu, 0x08u, 0x1Cu},
};

static uint8_t holdem_text_width(const char* text) {
    return (uint8_t)(strlen(text) * 6u);
}

static void holdem_draw_action_footer(
    Canvas* canvas,
    int32_t bet_total,
    const char* ok_action,
    HoldemNavGlyph center_glyph,
    bool show_center_glyph,
    uint8_t baseline_y) {
    const char* left_text = "Fold";
    char center_text[16];
    char right_text[24];
    int left_x = 0;
    int center_x;
    int left_width;
    int center_width;
    int center_text_width;
    int right_width;
    const int min_gap = 2;

    snprintf(center_text, sizeof(center_text), "$%d", (int)bet_total);
    snprintf(right_text, sizeof(right_text), "%s ", ok_action);

    left_width = HOLDEM_LR_ICON_SIZE + 1u + holdem_text_width(left_text);
    center_text_width = holdem_display_text_width(canvas, center_text);
    center_width = center_text_width + (show_center_glyph ? HOLDEM_UPDOWN_ICON_WIDTH : 0);
    right_width = holdem_ok_sequence_width(canvas, right_text, "");

    center_x = (int)holdem_centered_x_for_width((uint8_t)center_width);

    if(center_x < (left_x + left_width + min_gap)) {
        center_x = left_x + left_width + min_gap;
    }
    if((center_x + center_width + min_gap) > (127 - right_width)) {
        center_x = (127 - right_width) - center_width - min_gap;
    }
    if(center_x < (left_x + left_width + min_gap)) {
        center_x = left_x + left_width + min_gap;
    }

    holdem_draw_left_icon(canvas, left_x, baseline_y);
    canvas_draw_str(canvas, (uint8_t)(left_x + HOLDEM_LR_ICON_SIZE + 1u), baseline_y, left_text);
    holdem_draw_display_text(canvas, (uint8_t)center_x, baseline_y, center_text);
    if(show_center_glyph) {
        holdem_draw_nav_glyph(
            canvas, (uint8_t)(center_x + center_text_width + 1), baseline_y, center_glyph);
    }
    holdem_draw_ok_sequence_right(canvas, baseline_y, right_text, "");
}

static void holdem_draw_table_header(Canvas* canvas, const HoldemGame* game) {
    char left_text[20];
    char center_text[16];
    char right_text[24];
    int left_width;
    int center_width;
    int right_width;
    int center_x;
    const float min_gap = 6.0f;

    snprintf(left_text, sizeof(left_text), "Hand %d", (int)(game->hand_no + 1));
    snprintf(center_text, sizeof(center_text), "%s", k_stage_name[game->stage]);
    snprintf(right_text, sizeof(right_text), "Pot: $%d", (int)game->pot);

    left_width = canvas_string_width(canvas, left_text);
    center_width = canvas_string_width(canvas, center_text);
    right_width = holdem_display_text_width(canvas, right_text);

    float left_end = (float)left_width;
    float right_start = 127.0f - (float)right_width;
    float available_gap = right_start - left_end;

    if(available_gap < (float)center_width + (min_gap * 2.0f)) {
        snprintf(left_text, sizeof(left_text), "H%d", (int)(game->hand_no + 1));
        left_width = canvas_string_width(canvas, left_text);
        left_end = (float)left_width;
        available_gap = right_start - left_end;
    }

    if(available_gap < (float)center_width + (min_gap * 2.0f)) {
        snprintf(right_text, sizeof(right_text), "P: $%d", (int)game->pot);
        right_width = holdem_display_text_width(canvas, right_text);
        right_start = 127.0f - (float)right_width;
        available_gap = right_start - left_end;
    }

    center_x = (int)(left_end + ((right_start - left_end - (float)center_width) / 2.0f) + 0.5f);

    if((float)center_x < (left_end + min_gap)) {
        center_x = (int)(left_end + min_gap + 0.5f);
    }
    if(((float)center_x + (float)center_width + min_gap) > right_start) {
        center_x = (int)(right_start - (float)center_width - min_gap + 0.5f);
    }
    if((float)center_x < (left_end + min_gap)) {
        center_x = (int)(left_end + min_gap + 0.5f);
    }

    canvas_draw_str(canvas, 0, 8, left_text);
    canvas_draw_str(canvas, (uint8_t)center_x, 8, center_text);
    holdem_draw_line_right(canvas, 8, right_text);
}

static void holdem_draw_suit_icon(Canvas* canvas, uint8_t x, uint8_t baseline_y, uint8_t suit) {
    int icon_x = (int)x;
    int icon_y = (int)baseline_y - 7;

    for(uint8_t row = 0; row < HOLDEM_SUIT_BITMAP_SIZE; row++) {
        uint8_t row_bits = k_holdem_suit_bitmap[suit & 0x03u][row];
        for(uint8_t col = 0; col < HOLDEM_SUIT_BITMAP_SIZE; col++) {
            if(row_bits & (uint8_t)(1u << (HOLDEM_SUIT_BITMAP_SIZE - 1u - col))) {
                canvas_draw_dot(canvas, icon_x + col, icon_y + row);
            }
        }
    }
}

static uint8_t
    holdem_draw_card_token(Canvas* canvas, uint8_t x, uint8_t baseline_y, const char* token_text) {
    size_t token_len = strlen(token_text);
    uint8_t token_width = holdem_text_width(token_text);
    uint8_t token_x = (uint8_t)(x + ((HOLDEM_CARD_SLOT_WIDTH - token_width) / 2u) - 2u);

    if(token_len == 2 && token_text[0] == token_text[1]) {
        char left_char[2] = {token_text[0], '\0'};
        char right_char[2] = {token_text[1], '\0'};
        canvas_draw_str(canvas, token_x, baseline_y, left_char);
        canvas_draw_str(
            canvas,
            (uint8_t)(token_x + 6u + HOLDEM_HIDDEN_TOKEN_EXTRA_GAP),
            baseline_y,
            right_char);
    } else {
        canvas_draw_str(canvas, token_x, baseline_y, token_text);
    }

    return (uint8_t)(x + HOLDEM_CARD_SLOT_WIDTH);
}

static uint8_t holdem_draw_card(Canvas* canvas, uint8_t x, uint8_t baseline_y, Card card) {
    char rank_text[2] = {0};
    char card_text[3];
    uint8_t suit_x;

    card_to_str(card, card_text);
    rank_text[0] = card_text[0];
    canvas_draw_str(canvas, x, baseline_y, rank_text);

    suit_x = (uint8_t)(x + 6u + ((HOLDEM_SUIT_ICON_WIDTH - HOLDEM_SUIT_BITMAP_SIZE) / 2u));
    holdem_draw_suit_icon(canvas, suit_x, baseline_y, card.suit);
    return (uint8_t)(x + HOLDEM_CARD_SLOT_WIDTH);
}

static void holdem_draw_board_row(Canvas* canvas, const HoldemGame* game) {
    if(game->board_count == 0) {
        holdem_draw_text_centered(canvas, 16, "--");
        return;
    }

    uint8_t total_width = (uint8_t)(game->board_count * HOLDEM_CARD_SLOT_WIDTH);
    uint8_t draw_x = (total_width >= 128u) ? 0u : (uint8_t)((128u - total_width) / 2u);

    for(size_t board_index = 0; board_index < game->board_count; board_index++) {
        draw_x = holdem_draw_card(canvas, draw_x, 16, game->board[board_index]);
    }
}

static void holdem_draw_pause_cards_centered(
    Canvas* canvas,
    uint8_t baseline_y,
    const Card* cards,
    size_t card_count) {
    uint8_t total_width = (uint8_t)(card_count * HOLDEM_CARD_SLOT_WIDTH);
    uint8_t draw_x = (total_width >= 128u) ? 0u : (uint8_t)((128u - total_width) / 2u);

    for(size_t card_index = 0; card_index < card_count; card_index++) {
        draw_x = holdem_draw_card(canvas, draw_x, baseline_y, cards[card_index]);
    }
}

static void holdem_draw_hidden_pair_strikethrough(
    Canvas* canvas,
    uint8_t first_token_x,
    uint8_t baseline_y) {
    uint8_t strike_y = (uint8_t)(baseline_y - 4u);
    uint8_t strike_x0 = first_token_x;
    uint8_t strike_x1 = (uint8_t)(first_token_x + (2u * HOLDEM_CARD_SLOT_WIDTH) - 5u);
    canvas_draw_line(canvas, strike_x0, strike_y, strike_x1, strike_y);
}

static void holdem_draw_busted_row_strikethrough(Canvas* canvas, uint8_t baseline_y) {
    uint8_t strike_y = (uint8_t)(baseline_y - 4u);
    canvas_draw_line(canvas, 0, strike_y, 127, strike_y);
}

static void holdem_draw_firework(Canvas* canvas, int cx, int cy, uint8_t phase) {
    static const int8_t burst_dx[8] = {0, 5, 7, 5, 0, -5, -7, -5};
    static const int8_t burst_dy[8] = {-7, -5, 0, 5, 7, 5, 0, -5};
    uint8_t radius = (uint8_t)(2u + (phase % 4u));

    canvas_draw_dot(canvas, cx, cy);
    for(size_t ray = 0; ray < 8; ray++) {
        int x = cx + ((burst_dx[ray] * (int)radius) / 4);
        int y = cy + ((burst_dy[ray] * (int)radius) / 4);
        if(x >= 0 && x < 128 && y >= 0 && y < 64) canvas_draw_dot(canvas, x, y);
    }
}

static void holdem_draw_interstitial_screen(Canvas* canvas, const HoldemApp* app) {
    uint32_t anim_tick = furi_get_tick() / 120u;
    const char* second_line = strchr(app->interstitial_text, '\n');
    if(second_line) {
        char first_line[40];
        size_t first_len = (size_t)(second_line - app->interstitial_text);
        if(first_len >= sizeof(first_line)) first_len = sizeof(first_line) - 1u;
        memcpy(first_line, app->interstitial_text, first_len);
        first_line[first_len] = '\0';
        second_line++;

        holdem_draw_text_centered(canvas, 28, first_line);
        holdem_draw_text_centered(canvas, 36, second_line);
    } else {
        holdem_draw_text_centered(canvas, 36, app->interstitial_text);
    }

    if(app->interstitial_fireworks) {
        holdem_draw_firework(canvas, 24, 18, (uint8_t)(anim_tick & 0x03u));
        holdem_draw_firework(canvas, 104, 18, (uint8_t)((anim_tick + 1u) & 0x03u));
        holdem_draw_firework(canvas, 20, 48, (uint8_t)((anim_tick + 2u) & 0x03u));
        holdem_draw_firework(canvas, 108, 48, (uint8_t)((anim_tick + 3u) & 0x03u));
    }

    if(app->interstitial_show_continue_hint) {
        holdem_draw_ok_sequence_right(canvas, 64, "Continue ", "");
    }
}

static uint8_t holdem_result_block_baseline(size_t line_index, size_t line_count) {
    static const uint8_t k_three_line_baselines[3] = {30u, 38u, 46u};
    static const uint8_t k_four_line_baselines[4] = {26u, 34u, 42u, 50u};

    if(line_count == 3u && line_index < 3u) {
        return k_three_line_baselines[line_index];
    }
    if(line_count == 4u && line_index < 4u) {
        return k_four_line_baselines[line_index];
    }

    const uint8_t center_baseline_y = 38u;
    const uint8_t line_step = 8u;
    uint8_t first_baseline_y =
        (uint8_t)(center_baseline_y - (((line_count - 1u) * line_step) / 2u));
    return (uint8_t)(first_baseline_y + (line_step * line_index));
}

static void holdem_draw_player_row(
    Canvas* canvas,
    const HoldemGame* game,
    const Player* player,
    int player_index,
    int active_idx,
    bool showdown_waiting,
    uint8_t showdown_winner_mask,
    bool reveal_all,
    uint8_t baseline_y) {
    char stack_text[7];
    uint8_t draw_x = HOLDEM_CARD_AREA_START_X;
    char role_text[2] = {0};
    char status_text[2] = {0};
    bool showdown_winner = showdown_waiting && ((showdown_winner_mask >> player_index) & 0x1u);
    bool active_player = (player_index == active_idx);

    snprintf(stack_text, sizeof(stack_text), "%d", (int)player->stack);
    role_text[0] = holdem_role_char(game, player_index);
    status_text[0] = holdem_status_char(game, player);

    if(showdown_winner) {
        canvas_draw_str(canvas, HOLDEM_TURN_MARK_X, (uint8_t)(baseline_y + 1u), "* ");
    } else if(active_player) {
        canvas_draw_str(canvas, HOLDEM_TURN_MARK_X, baseline_y, "> ");
    } else {
        canvas_draw_str(canvas, HOLDEM_TURN_MARK_X, baseline_y, "  ");
    }

    canvas_draw_str(canvas, HOLDEM_NAME_X, baseline_y, holdem_display_name(player));
    canvas_draw_str(canvas, HOLDEM_ROLE_X, baseline_y, role_text);
    canvas_draw_str(canvas, HOLDEM_STATUS_X, baseline_y, status_text);
    holdem_draw_chip_icon(canvas, HOLDEM_STACK_DOLLAR_X, baseline_y);
    canvas_draw_str(canvas, HOLDEM_STACK_VALUE_X, baseline_y, stack_text);

    if(!player->is_bot) {
        draw_x = holdem_draw_card(canvas, draw_x, baseline_y, player->hole[0]);
        holdem_draw_card(canvas, draw_x, baseline_y, player->hole[1]);
    } else if(game->stage == StageShowdown) {
        if(player->in_hand) {
            draw_x = holdem_draw_card(canvas, draw_x, baseline_y, player->hole[0]);
            holdem_draw_card(canvas, draw_x, baseline_y, player->hole[1]);
        } else {
            draw_x = holdem_draw_card_token(canvas, draw_x, baseline_y, "XX");
            holdem_draw_card_token(canvas, draw_x, baseline_y, "XX");
        }
    } else if(!player->in_hand) {
        uint8_t hidden_pair_x = draw_x;
        draw_x = holdem_draw_card_token(canvas, draw_x, baseline_y, "XX");
        holdem_draw_card_token(canvas, draw_x, baseline_y, "XX");
        holdem_draw_hidden_pair_strikethrough(canvas, hidden_pair_x, baseline_y);
    } else if(reveal_all) {
        draw_x = holdem_draw_card(canvas, draw_x, baseline_y, player->hole[0]);
        holdem_draw_card(canvas, draw_x, baseline_y, player->hole[1]);
    } else {
        draw_x = holdem_draw_card_token(canvas, draw_x, baseline_y, "??");
        holdem_draw_card_token(canvas, draw_x, baseline_y, "??");
    }

    if(player->is_bot && holdem_player_is_eliminated(game, player)) {
        holdem_draw_busted_row_strikethrough(canvas, baseline_y);
    }
}

void holdem_draw_callback(Canvas* canvas, void* ctx) {
    HoldemApp* app = ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    if(app->mode == UiModeSplash) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 10, "Hold 'em");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_frame(canvas, 96, 16, 14, 18);
        canvas_draw_frame(canvas, 108, 20, 14, 18);
        canvas_draw_str(canvas, 100, 28, "A");
        canvas_draw_str(canvas, 112, 32, "K");
        canvas_draw_line(canvas, 14, 34, 30, 28);
        canvas_draw_line(canvas, 30, 28, 50, 30);
        canvas_draw_line(canvas, 50, 30, 62, 36);
        canvas_draw_line(canvas, 62, 36, 48, 40);
        canvas_draw_line(canvas, 48, 40, 26, 40);
        canvas_draw_line(canvas, 26, 40, 14, 34);
        canvas_draw_line(canvas, 40, 29, 44, 22);
        canvas_draw_line(canvas, 44, 22, 48, 30);
        canvas_draw_line(canvas, 14, 34, 6, 30);
        canvas_draw_line(canvas, 14, 34, 5, 38);
        canvas_draw_dot(canvas, 53, 34);
        canvas_draw_line(canvas, 8, 44, 88, 44);
        canvas_draw_line(canvas, 10, 45, 86, 45);
        holdem_draw_line_right(canvas, 64, "v1.1");
        return;
    }

    if(app->mode == UiModeStartChoice) {
        holdem_draw_text_centered(canvas, 36, "Saved game found");
        holdem_draw_ok_sequence(canvas, 0, 64, "", " Load");
        holdem_draw_back_sequence_right(canvas, 64, "New ", "");
        return;
    }

    if(app->mode == UiModeStartReady) {
        holdem_draw_ok_sequence_center_with_gap(canvas, 64, 36, "", " Start Game", 2u);
        return;
    }

    if(app->mode == UiModeHelp) {
        char help_line[40];
        char page_text[8];
        int32_t menu_small_blind = app->pending_blinds_dirty ? app->pending_small_blind :
                                                               app->game.small_blind;
        int32_t menu_big_blind = menu_small_blind * 2;
        uint8_t page_width;
        uint8_t page_draw_x;
        snprintf(
            help_line,
            sizeof(help_line),
            "SB/BB: %d/%d",
            (int)menu_small_blind,
            (int)menu_big_blind);
        snprintf(page_text, sizeof(page_text), "%u/3", (unsigned int)(app->help_page + 1u));
        page_width = canvas_string_width(canvas, page_text);
        page_draw_x = (uint8_t)(127u - HOLDEM_UPDOWN_ICON_WIDTH - 1u - page_width);

        if(app->help_page == 0) {
            holdem_draw_line(canvas, 8, "Controls Help");
            canvas_draw_str(canvas, page_draw_x, 8, page_text);
            holdem_draw_nav_icon(
                canvas, (uint8_t)(127u - HOLDEM_UPDOWN_ICON_WIDTH), 8, app->help_page, 3u);
            holdem_draw_line(canvas, 16, "");
            holdem_draw_ok_icon(canvas, 0, 24);
            canvas_draw_str(canvas, 8, 24, " Check/Call/Raise");
            holdem_draw_nav_glyph(canvas, 0, 32, HoldemNavGlyphUpDown);
            canvas_draw_str(canvas, 8, 32, " Bet +/-");
            holdem_draw_right_icon(canvas, 0, 40);
            canvas_draw_str(canvas, 8, 40, " Reset bet (hold for All In)");
            holdem_draw_left_icon(canvas, 0, 48);
            canvas_draw_str(canvas, 8, 48, " Fold");
            holdem_draw_back_cancel_hint(canvas, 64);
        } else if(app->help_page == 1) {
            char blind_line[40];
            char bot_line[40];
            holdem_draw_line(canvas, 8, "Game Menu");
            canvas_draw_str(canvas, page_draw_x, 8, page_text);
            holdem_draw_nav_icon(
                canvas, (uint8_t)(127u - HOLDEM_UPDOWN_ICON_WIDTH), 8, app->help_page, 3u);
            holdem_draw_line(canvas, 16, "");
            snprintf(
                blind_line,
                sizeof(blind_line),
                "%d/%d)",
                (int)menu_small_blind,
                (int)menu_big_blind);
            snprintf(
                bot_line,
                sizeof(bot_line),
                app->progressive_blinds_enabled ? " Edit blinds (P - $%d/$%d)" :
                                                  " Edit blinds ($%d/$%d)",
                (int)menu_small_blind,
                (int)menu_big_blind);
            holdem_draw_ok_sequence_with_gap(canvas, 0, 24, "", bot_line, 2u);
            holdem_draw_right_icon(canvas, 0, 32);
            snprintf(
                bot_line,
                sizeof(bot_line),
                " Edit bots (%d - %s)",
                (int)(app->configured_player_count - 1),
                holdem_ai_label(app->configured_ai_level_pct));
            canvas_draw_str(canvas, 8, 32, bot_line);
            holdem_draw_left_icon(canvas, 0, 40);
            canvas_draw_str(canvas, 8, 40, " Start new game");
            holdem_draw_back_cancel_hint(canvas, 64);
        } else {
            holdem_draw_line(canvas, 8, "Hand Ranks");
            canvas_draw_str(canvas, page_draw_x, 8, page_text);
            holdem_draw_nav_icon(
                canvas, (uint8_t)(127u - HOLDEM_UPDOWN_ICON_WIDTH), 8, app->help_page, 3u);
            holdem_draw_line(canvas, 16, "");
            holdem_draw_line(canvas, 24, "1 Straight Flush");
            holdem_draw_line(canvas, 32, "2 Quads  3 Full House");
            holdem_draw_line(canvas, 40, "4 Flush  5 Straight");
            holdem_draw_line(canvas, 48, "6 Trips  7 Two Pair");
            holdem_draw_line(canvas, 56, "8 Pair   9 High Card");
            holdem_draw_back_cancel_hint(canvas, 64);
        }
        return;
    }

    if(app->mode == UiModeBlindEdit) {
        bool blind_edit_dirty = holdem_blind_edit_is_dirty(app);
        holdem_draw_line(canvas, 8, "Edit Blinds");
        holdem_draw_line(canvas, 16, "");
        if(!app->blind_edit_progressive_enabled) {
            char blind_line[32];
            const char* progress_text = "Progressive blinds: Off";
            uint8_t progress_text_width = canvas_string_width(canvas, progress_text);
            bool can_increase_sb = (app->blind_edit_sb < 500);
            bool can_decrease_sb = (app->blind_edit_sb > HOLDEM_BLIND_STEP_SB);
            HoldemNavGlyph blind_glyph = HoldemNavGlyphUpDown;
            snprintf(
                blind_line,
                sizeof(blind_line),
                "SB/BB: $%d/$%d",
                (int)app->blind_edit_sb,
                (int)(app->blind_edit_sb * 2));
            if(can_increase_sb && !can_decrease_sb)
                blind_glyph = HoldemNavGlyphUp;
            else if(!can_increase_sb && can_decrease_sb)
                blind_glyph = HoldemNavGlyphDown;

            canvas_draw_str(canvas, 0, 24, progress_text);
            holdem_draw_right_icon(canvas, (uint8_t)(progress_text_width + 1u), 24);
            holdem_draw_display_text(canvas, 0, 32, blind_line);
            holdem_draw_nav_glyph(
                canvas,
                (uint8_t)(holdem_display_text_width(canvas, blind_line) + 3u),
                32,
                blind_glyph);
            if(app->blind_edit_sb != app->blind_edit_initial_sb) {
                holdem_draw_left_icon(canvas, 0, 40);
                canvas_draw_str(canvas, 8, 40, " Reset blinds");
            } else {
                holdem_draw_line(canvas, 40, "");
            }
        } else {
            const char* progress_prefix = "Progressive blinds: ";
            uint8_t progress_prefix_width = canvas_string_width(canvas, progress_prefix);
            char increase_middle[20];
            char period_middle[8];
            bool can_decrease_step =
                (app->blind_edit_progressive_step_sb > HOLDEM_PROGRESSIVE_MIN_STEP_SB);
            bool can_increase_step =
                (app->blind_edit_progressive_step_sb < HOLDEM_PROGRESSIVE_MAX_STEP_SB);
            bool can_decrease_period =
                (app->blind_edit_progressive_period_hands > HOLDEM_PROGRESSIVE_MIN_PERIOD_HANDS);
            bool can_increase_period =
                (app->blind_edit_progressive_period_hands < HOLDEM_PROGRESSIVE_MAX_PERIOD_HANDS);
            HoldemNavGlyph increase_glyph = HoldemNavGlyphUpDown;

            canvas_draw_str(canvas, 0, 24, progress_prefix);
            if(app->blind_edit_progressive_period_hands == HOLDEM_PROGRESSIVE_MIN_PERIOD_HANDS) {
                holdem_draw_left_icon(canvas, progress_prefix_width, 24);
                canvas_draw_str(
                    canvas, (uint8_t)(progress_prefix_width + HOLDEM_LR_ICON_SIZE), 24, "On");
            } else {
                canvas_draw_str(canvas, progress_prefix_width, 24, "On");
            }

            if(can_increase_step && !can_decrease_step)
                increase_glyph = HoldemNavGlyphUp;
            else if(!can_increase_step && can_decrease_step)
                increase_glyph = HoldemNavGlyphDown;

            snprintf(
                increase_middle,
                sizeof(increase_middle),
                "Increase SB $%d",
                (int)app->blind_edit_progressive_step_sb);
            snprintf(
                period_middle,
                sizeof(period_middle),
                "%d",
                (int)app->blind_edit_progressive_period_hands);
            holdem_draw_line(canvas, 32, "");
            {
                uint8_t increase_text_width = holdem_display_text_width(canvas, increase_middle);
                uint8_t increase_total_width =
                    (uint8_t)(increase_text_width + 1u + HOLDEM_UPDOWN_ICON_WIDTH);
                uint8_t increase_x = holdem_centered_x_for_width(increase_total_width);
                holdem_draw_display_text(canvas, increase_x, 40, increase_middle);
                holdem_draw_nav_glyph(
                    canvas, (uint8_t)(increase_x + increase_text_width + 3u), 40, increase_glyph);
            }
            holdem_draw_centered_inline_lr_text(
                canvas,
                48,
                "every ",
                can_decrease_period,
                period_middle,
                can_increase_period,
                " hands (0 = Off)");
        }

        holdem_draw_back_sequence_right(canvas, 64, "Back ", "");
        if(blind_edit_dirty) {
            holdem_draw_ok_sequence(canvas, 0, 64, "", " Save");
        }
        return;
    }

    if(app->mode == UiModeBotCountEdit) {
        char count_prefix[24];
        char count_value[4];
        const char* difficulty_prefix = "Bot difficulty: ";
        const char* difficulty_value = holdem_ai_label(app->bot_difficulty_edit_value);
        bool bot_edit_dirty = holdem_bot_edit_is_dirty(app);
        bool can_increase_bots = (app->bot_count_edit_value < HOLDEM_MAX_PLAYERS);
        bool can_decrease_bots = (app->bot_count_edit_value > HOLDEM_MIN_PLAYERS);
        HoldemNavGlyph bot_count_glyph = HoldemNavGlyphUpDown;
        bool can_decrease_difficulty = (app->bot_difficulty_edit_value > HOLDEM_AI_EASY_LEVEL);
        bool can_increase_difficulty = (app->bot_difficulty_edit_value < HOLDEM_AI_EXTREME_LEVEL);
        uint8_t difficulty_prefix_width;
        uint8_t difficulty_x;

        snprintf(count_prefix, sizeof(count_prefix), "Number of bots: ");
        snprintf(count_value, sizeof(count_value), "%d", (int)(app->bot_count_edit_value - 1));
        if(can_increase_bots && !can_decrease_bots)
            bot_count_glyph = HoldemNavGlyphUp;
        else if(!can_increase_bots && can_decrease_bots)
            bot_count_glyph = HoldemNavGlyphDown;

        holdem_draw_line(canvas, 8, "Edit Bots");
        holdem_draw_line(canvas, 16, "");
        canvas_draw_str(canvas, 0, 24, count_prefix);
        canvas_draw_str(
            canvas, (uint8_t)canvas_string_width(canvas, count_prefix), 24, count_value);
        holdem_draw_nav_glyph(
            canvas,
            (uint8_t)(canvas_string_width(canvas, count_prefix) +
                      canvas_string_width(canvas, count_value) + 1u),
            24,
            bot_count_glyph);

        difficulty_prefix_width = canvas_string_width(canvas, difficulty_prefix);
        difficulty_x = difficulty_prefix_width;
        canvas_draw_str(canvas, 0, 32, difficulty_prefix);
        if(can_decrease_difficulty) {
            holdem_draw_left_icon(canvas, difficulty_x, 32);
            difficulty_x = (uint8_t)(difficulty_x + HOLDEM_LR_ICON_SIZE);
        }
        canvas_draw_str(canvas, difficulty_x, 32, difficulty_value);
        difficulty_x = (uint8_t)(difficulty_x + canvas_string_width(canvas, difficulty_value));
        if(can_increase_difficulty) {
            holdem_draw_right_icon(canvas, difficulty_x, 32);
        }

        holdem_draw_back_sequence_right(canvas, 64, "Back ", "");
        if(bot_edit_dirty) {
            holdem_draw_ok_sequence(canvas, 0, 64, "", " Save");
        }
        return;
    }

    if(app->mode == UiModeNewGameConfirm) {
        if(app->new_game_confirm_from_bot_edit) {
            holdem_draw_text_centered(canvas, 32, "Apply changes and");
            holdem_draw_text_centered(canvas, 40, "start new game?");
        } else {
            holdem_draw_text_centered(canvas, 36, "Start new game?");
        }
        holdem_draw_ok_sequence(canvas, 0, 64, "", " Yes");
        holdem_draw_back_sequence_right(canvas, 64, "No ", "");
        return;
    }

    if(app->mode == UiModeExitPrompt) {
        holdem_draw_line(canvas, 8, "Exit Hold 'em");
        holdem_draw_line(canvas, 16, "");
        holdem_draw_ok_sequence(canvas, 0, 24, "", " Save and exit");
        holdem_draw_back_sequence(canvas, 0, 32, "Hold ", " to exit without saving");
        holdem_draw_back_sequence_right(canvas, 64, "Back ", "");
        return;
    }

    if(app->mode == UiModePause) {
        size_t result_line_count = app->pause_body4[0] ? 4u : 3u;

        holdem_draw_line(canvas, 8, app->pause_title);
        holdem_draw_line(canvas, 20, "");
        if(app->pause_card_count > 0) {
            holdem_draw_pause_cards_centered(
                canvas,
                holdem_result_block_baseline(0u, result_line_count),
                app->pause_cards,
                app->pause_card_count);
        } else if(app->pause_body3[0]) {
            holdem_draw_text_centered(
                canvas, holdem_result_block_baseline(0u, result_line_count), app->pause_body3);
        }
        holdem_draw_text_centered(
            canvas, holdem_result_block_baseline(1u, result_line_count), app->pause_body2);
        holdem_draw_text_centered(
            canvas, holdem_result_block_baseline(2u, result_line_count), app->pause_body);
        if(app->pause_body4[0]) {
            holdem_draw_text_centered(
                canvas, holdem_result_block_baseline(3u, result_line_count), app->pause_body4);
        }
        if(app->pause_footer[0]) {
            holdem_draw_ok_sequence_right(canvas, 64, app->pause_footer, "");
        }
        return;
    }

    if(app->mode == UiModeBigWin) {
        holdem_draw_interstitial_screen(canvas, app);
        return;
    }

    holdem_draw_table_header(canvas, &app->game);
    holdem_draw_board_row(canvas, &app->game);

    int active_idx = -1;
    if(app->mode == UiModeActionPrompt) {
        active_idx = app->acting_idx;
    } else if(app->bot_turn_active) {
        active_idx = app->bot_turn_idx;
    }

    size_t display_order[HOLDEM_MAX_PLAYERS] = {0};
    holdem_build_display_order(&app->game, display_order);

    for(size_t display_row = 0; display_row < app->game.player_count; display_row++) {
        int player_index = (int)display_order[display_row];
        Player* player = &app->game.players[player_index];
        holdem_draw_player_row(
            canvas,
            &app->game,
            player,
            player_index,
            active_idx,
            app->showdown_waiting,
            app->showdown_winner_mask,
            app->reveal_all,
            24 + (uint8_t)(8 * display_row));
    }

    if(app->mode == UiModeActionPrompt) {
        const char* ok_action = "Call";
        HoldemNavGlyph bet_glyph = HoldemNavGlyphUp;
        bool show_bet_glyph = true;
        int32_t acting_stack = app->game.players[app->acting_idx].stack;
        bool can_raise =
            (app->prompt_bet_total < acting_stack) &&
            ((app->prompt_bet_total <= app->prompt_to_call) || (app->prompt_raise_by > 0));
        bool raise_selected = app->prompt_bet_total > app->prompt_to_call;
        if(app->prompt_bet_total <= app->prompt_to_call) {
            if(app->prompt_to_call == 0) {
                ok_action = "Check";
            } else if(app->prompt_to_call > acting_stack) {
                ok_action = "All In";
            } else {
                ok_action = "Call";
            }
            show_bet_glyph = can_raise;
        } else {
            ok_action = (app->prompt_bet_total >= acting_stack) ? "All In" : "Raise";
            bet_glyph = (app->prompt_bet_total >= acting_stack) ? HoldemNavGlyphDown :
                                                                  HoldemNavGlyphUpDown;
            show_bet_glyph = raise_selected;
        }

        holdem_draw_line(canvas, 56, "");
        holdem_draw_action_footer(
            canvas, app->prompt_bet_total, ok_action, bet_glyph, show_bet_glyph, 64);
    } else if(app->bot_turn_active) {
        const char* bot_status = app->bot_turn_text;
        if(!bot_status[0] && app->bot_turn_idx >= 0 &&
           app->bot_turn_idx < (int)app->game.player_count) {
            bot_status = app->game.players[app->bot_turn_idx].name;
        }
        holdem_draw_line(canvas, 56, "");
        holdem_draw_text_centered(canvas, 64, bot_status);
        if(holdem_can_skip_folded_autoplay(app)) {
            holdem_draw_ok_ff_hint(canvas, 64);
        }
    } else {
        holdem_draw_line(canvas, 56, "");
        if(app->showdown_waiting) {
            holdem_draw_ok_sequence_right(canvas, 64, "Results ", "");
        } else if(!app->game.players[0].in_hand) {
            if(app->game.hand_in_progress) {
                holdem_draw_ok_ff_hint(canvas, 64);
            } else {
                holdem_draw_line(canvas, 64, "");
            }
        } else {
            holdem_draw_line(canvas, 64, "");
        }
    }
}
