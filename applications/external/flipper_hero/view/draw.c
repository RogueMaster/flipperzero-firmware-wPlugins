#include "draw.h"

void update_marquee_data(PluginState* plugin_state) {
    if(plugin_state->timer < plugin_state->marquee_data[0] - MARQUEE_DELAY &&
       plugin_state->timer % MARQUEE_SPEED == 0) {
        // If the marquee offset is >= the marquee spacing, loop the marquee. Otherwise, increment it.
        plugin_state->marquee_data[1] += MARQUEE_DISTANCE;
        if(plugin_state->marquee_data[1] >= plugin_state->marquee_data[2])
            plugin_state->marquee_data[1] = 0;
    }
}

void init_marquee_data(Canvas* const canvas, PluginState* plugin_state) {
    get_stratagem_name(
        plugin_state->name_buf, plugin_state->stratagem_queue[plugin_state->queue_spot]);
    char nameText[32];
    snprintf(nameText, sizeof(nameText), "%s", plugin_state->name_buf);
    canvas_set_font(canvas, FontKeyboard);
    int name_width = (int)canvas_string_width(canvas, nameText);
    plugin_state->do_marquee = (name_width > 128 - (NAME_PADDING_X * 2));
    int marquee_spacing = MARQUEE_BASE_PADDING + name_width;
    marquee_spacing +=
        MARQUEE_DISTANCE -
        (marquee_spacing %
         MARQUEE_DISTANCE); // Increase the marquee_spacing so that the it's divisble by marquee_distance
    plugin_state->marquee_data[0] = plugin_state->timer;
    plugin_state->marquee_data[1] = 0;
    plugin_state->marquee_data[2] = marquee_spacing;
}

void draw_round_score(PluginState* plugin_state, Canvas* const canvas) {
    char bonusTextBuf[32];
    canvas_set_font(canvas, FontPrimary);
    //Round Bonus ~= 25 * round number
    canvas_draw_str_aligned(canvas, 4, 16, AlignLeft, AlignCenter, "Round Bonus");
    snprintf(bonusTextBuf, sizeof(bonusTextBuf), "%d", plugin_state->bonuses[0]);

    canvas_draw_str_aligned(canvas, 124, 16, AlignRight, AlignCenter, bonusTextBuf);

    // Time Bonus
    if(plugin_state->timer < 200) {
        canvas_draw_str_aligned(canvas, 4, 28, AlignLeft, AlignCenter, "Time Bonus");
        snprintf(bonusTextBuf, sizeof(bonusTextBuf), "%d", plugin_state->bonuses[1]);

        canvas_draw_str_aligned(canvas, 124, 28, AlignRight, AlignCenter, bonusTextBuf);
    }
    // Perfect Bonus
    if(plugin_state->timer < 150) {
        canvas_draw_str_aligned(canvas, 4, 40, AlignLeft, AlignCenter, "Perfect Bonus");
        snprintf(bonusTextBuf, sizeof(bonusTextBuf), "%d", plugin_state->bonuses[2]);

        canvas_draw_str_aligned(canvas, 124, 40, AlignRight, AlignCenter, bonusTextBuf);
    }
    // Total Score
    if(plugin_state->timer < 100) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 4, 52, AlignLeft, AlignCenter, "Total Score");
        snprintf(bonusTextBuf, sizeof(bonusTextBuf), "%d", plugin_state->score);

        canvas_draw_str_aligned(canvas, 124, 52, AlignRight, AlignCenter, bonusTextBuf);
    }
}
void draw_pre_round(PluginState* plugin_state, Canvas* const canvas) {
    char roundText[32];
    char specialRoundText[32];
    switch(plugin_state->special_round) {
    case SpecialRoundBackpack:
        snprintf(specialRoundText, sizeof(specialRoundText), "BACK-PACKING A PUNCH");
        break;
    case SpecialRoundOrbital:
        snprintf(specialRoundText, sizeof(specialRoundText), "RAIN DOWN FROM ABOVE");
        break;
    case SpecialRoundEagle:
        snprintf(specialRoundText, sizeof(specialRoundText), "FLY LIKE AN EAGLE");
        break;
    case SpecialRoundWeapon:
        snprintf(specialRoundText, sizeof(specialRoundText), "OVERWHELMING FIREPOWER");
        break;
    default:
        break;
    }
    snprintf(roundText, sizeof(roundText), "%d", plugin_state->round);

    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str_aligned(canvas, 64, 64, AlignCenter, AlignBottom, specialRoundText);
    canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignBottom, "Round");

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "GET READY");
    canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignTop, roundText);
}
void draw_start_screen(Canvas* const canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_icon(canvas, 0, 0, &I_flipper_hero_128x50);
    elements_button_center(canvas, "Start");
}
void draw_game_over_screen(Canvas* const canvas, PluginState* plugin_state) {
    char scoreText[32];
    char roundText[32];
    char highScoreText[32];
    char highRoundText[32];

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignCenter, "Game Over");
    canvas_draw_str_aligned(canvas, 4, 15, AlignLeft, AlignCenter, "Score:");
    canvas_draw_str_aligned(canvas, 124, 15, AlignRight, AlignCenter, "Round:");
    canvas_draw_str_aligned(canvas, 4, 35, AlignLeft, AlignCenter, "High Score:");
    canvas_draw_str_aligned(canvas, 124, 35, AlignRight, AlignCenter, "High Round:");
    snprintf(scoreText, sizeof(scoreText), "%d", plugin_state->score);
    snprintf(roundText, sizeof(roundText), "%d", plugin_state->round);
    snprintf(highScoreText, sizeof(highScoreText), "%ld", plugin_state->record->score);
    snprintf(highRoundText, sizeof(highRoundText), "%ld", plugin_state->record->round);
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str_aligned(canvas, 4, 25, AlignLeft, AlignCenter, scoreText);
    canvas_draw_str_aligned(canvas, 124, 25, AlignRight, AlignCenter, roundText);
    canvas_draw_str_aligned(canvas, 4, 45, AlignLeft, AlignCenter, highScoreText);
    canvas_draw_str_aligned(canvas, 124, 45, AlignRight, AlignCenter, highRoundText);

    elements_button_center(canvas, "Restart");
    canvas_draw_frame(canvas, 0, 0, 128, 64);
}
void draw_name(Canvas* const canvas, PluginState* plugin_state) {
    char nameText[32];
    get_stratagem_name(
        plugin_state->name_buf, plugin_state->stratagem_queue[plugin_state->queue_spot]);

    // I made these const variables so it would be easier to adjust if needed

    snprintf(nameText, sizeof(nameText), "%s", plugin_state->name_buf);

    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_box(canvas, 0, NAME_POS_Y - NAME_PADDING_Y, 128, 9 + (2 * NAME_PADDING_Y));
    canvas_invert_color(canvas);

    // If the text wouldn't fit on the screen, marquee it. Otherwise, just display it normally.
    if(plugin_state->do_marquee) {
        canvas_draw_str_aligned(
            canvas,
            NAME_PADDING_X - plugin_state->marquee_data[1],
            NAME_POS_Y,
            AlignLeft,
            AlignTop,
            nameText);
        canvas_draw_str_aligned(
            canvas,
            NAME_PADDING_X - plugin_state->marquee_data[1] + plugin_state->marquee_data[2],
            NAME_POS_Y,
            AlignLeft,
            AlignTop,
            nameText);
    } else {
        canvas_draw_str_aligned(canvas, NAME_PADDING_X, NAME_POS_Y, AlignLeft, AlignTop, nameText);
    }
    canvas_invert_color(canvas);

    // Draw borders to the left and right of the name text
    canvas_draw_box(
        canvas, 0, NAME_POS_Y - NAME_PADDING_Y, NAME_PADDING_X, 9 + (2 * NAME_PADDING_Y));
    canvas_draw_box(
        canvas,
        128 - NAME_PADDING_X,
        NAME_POS_Y - NAME_PADDING_Y,
        NAME_PADDING_X,
        9 + (2 * NAME_PADDING_Y));
}
void draw_game_ui(Canvas* const canvas, int round, int score) {
    char roundText[32], scoreText[32];

    snprintf(roundText, sizeof(roundText), "%d", round);
    snprintf(scoreText, sizeof(scoreText), "%d", score);

    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str_aligned(canvas, 4, 9, AlignLeft, AlignBottom, "Round");
    canvas_draw_str_aligned(canvas, 124, 10, AlignRight, AlignTop, "Score");

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 4, 10, AlignLeft, AlignTop, roundText);
    canvas_draw_str_aligned(canvas, 124, 9, AlignRight, AlignBottom, scoreText);
    // canvas_draw_frame(canvas, 0, 0, 128, 64);
}
void draw_icon(
    Canvas* const canvas,
    int8_t pos_x,
    int8_t pos_y,
    int8_t icon_index,
    bool large_icon) {
    int width = 15;
    int height = 15;
    if(large_icon) {
        width *= 2;
        height *= 2;
    }

    pos_x -= width / 2;
    pos_y -= height / 2;

    canvas_draw_icon(canvas, pos_x, pos_y, get_stratagem_icon(icon_index, large_icon));
}
void draw_queue(Canvas* const canvas, PluginState* plugin_state) {
    int spaceX = 15;
    // int totalIconsWidth = 5 * spaceX;
    int queue_stop = plugin_state->queue_spot + 5;
    if(queue_stop > 20) queue_stop = 20;
    int startX = (plugin_state->config->center_queue) ? 64 : 50;
    for(int i = plugin_state->queue_spot; i < queue_stop; i++) {
        if(plugin_state->stratagem_queue[i] == -1) break;
        if(i == plugin_state->queue_spot) {
            draw_icon(canvas, startX, 19, plugin_state->stratagem_queue[i], true);
            canvas_draw_frame(canvas, startX - 16, 3, 32, 32);
        } else {
            draw_icon(
                canvas,
                startX + ((i - plugin_state->queue_spot) * (spaceX + 1)) + 8,
                25,
                plugin_state->stratagem_queue[i],
                false);
        }
    }
}
void draw_arrows(Canvas* const canvas, PluginState* plugin_state) {
    int spaceX = 15;
    int totalArrowsWidth = (plugin_state->numArrows - 1) * spaceX;
    int startX = (128 - totalArrowsWidth) / 2 + 2; // Assuming canvas width of 128 pixels
    for(int i = 0; i < plugin_state->numArrows; i++) {
        draw_empty_or_filled_arrow(
            canvas,
            startX + i * spaceX,
            53,
            plugin_state->arrowDirections[i],
            plugin_state->arrowFilled[i]);
    }
}
void draw_progress_box(Canvas* const canvas, int timer) {
    canvas_draw_frame(canvas, 0, 58, 128, 6);
    canvas_draw_box(
        canvas, 0, 58, timer * 0.128, 6); // Assuming max timer value scales to full width
}

void draw_pause_menu(Canvas* const canvas, PluginState* plugin_state) {
    char faithText[8];
    char soundText[8];
    char queueText[8];
    // Draw Box
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(
        canvas,
        PAUSE_MENU_MARGIN_X,
        PAUSE_MENU_MARGIN_Y,
        (128 - (2 * PAUSE_MENU_MARGIN_X)),
        (64 - (2 * PAUSE_MENU_MARGIN_Y)));
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(
        canvas,
        PAUSE_MENU_MARGIN_X,
        PAUSE_MENU_MARGIN_Y,
        (128 - (2 * PAUSE_MENU_MARGIN_X)),
        (64 - (2 * PAUSE_MENU_MARGIN_Y)));
    // Draw menu options
    canvas_set_font(canvas, FontKeyboard);
    snprintf(
        faithText, sizeof(faithText), "<%s>", (plugin_state->config->unfaithful) ? "ON " : "OFF");
    snprintf(
        soundText, sizeof(soundText), "<%s>", (plugin_state->config->enable_sound) ? "ON " : "OFF");
    snprintf(
        queueText, sizeof(queueText), "<%s>", (plugin_state->config->center_queue) ? "ON " : "OFF");
    canvas_draw_str_aligned(
        canvas,
        PAUSE_MENU_MARGIN_X + PAUSE_MENU_PADDING_X,
        PAUSE_MENU_MARGIN_Y + PAUSE_MENU_PADDING_Y,
        AlignLeft,
        AlignTop,
        "New Strgms:");
    canvas_draw_str_aligned(
        canvas,
        PAUSE_MENU_MARGIN_X + PAUSE_MENU_PADDING_X,
        PAUSE_MENU_MARGIN_Y + PAUSE_MENU_PADDING_Y + MENU_OPTION_SPACE_Y,
        AlignLeft,
        AlignTop,
        "Sound:");
    canvas_draw_str_aligned(
        canvas,
        PAUSE_MENU_MARGIN_X + PAUSE_MENU_PADDING_X,
        PAUSE_MENU_MARGIN_Y + PAUSE_MENU_PADDING_Y + (MENU_OPTION_SPACE_Y * 2),
        AlignLeft,
        AlignTop,
        "Center Queue:");

    canvas_draw_str_aligned(
        canvas,
        128 - (PAUSE_MENU_MARGIN_X + PAUSE_MENU_PADDING_X),
        PAUSE_MENU_MARGIN_Y + PAUSE_MENU_PADDING_Y,
        AlignRight,
        AlignTop,
        faithText);

    canvas_draw_str_aligned(
        canvas,
        128 - (PAUSE_MENU_MARGIN_X + PAUSE_MENU_PADDING_X),
        PAUSE_MENU_MARGIN_Y + PAUSE_MENU_PADDING_Y + MENU_OPTION_SPACE_Y,
        AlignRight,
        AlignTop,
        soundText);

    canvas_draw_str_aligned(
        canvas,
        128 - (PAUSE_MENU_MARGIN_X + PAUSE_MENU_PADDING_X),
        PAUSE_MENU_MARGIN_Y + PAUSE_MENU_PADDING_Y + (MENU_OPTION_SPACE_Y * 2),
        AlignRight,
        AlignTop,
        queueText);
    // Draw an invert color box over the selected menu option
    canvas_set_color(canvas, ColorXOR);
    canvas_draw_box(
        canvas,
        PAUSE_MENU_MARGIN_X + 1,
        PAUSE_MENU_PADDING_Y + PAUSE_MENU_MARGIN_Y +
            (MENU_OPTION_SPACE_Y * plugin_state->menu_spot) - 2,
        126 - (2 * PAUSE_MENU_MARGIN_X),
        MENU_OPTION_SPACE_Y);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str_aligned(
        canvas,
        64,
        58 - (PAUSE_MENU_MARGIN_Y + PAUSE_MENU_PADDING_Y),
        AlignCenter,
        AlignBottom,
        "Back: Exit");
    canvas_draw_str_aligned(
        canvas,
        64,
        66 - (PAUSE_MENU_MARGIN_Y + PAUSE_MENU_PADDING_Y),
        AlignCenter,
        AlignBottom,
        "OK: Exit & Save");
}
