// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// Daily summary: today's punches plus first-in, last-out and total worked time.

void timeclock_scene_today_on_enter(void* context) {
    TimeClock* app = context;
    TextBox* text_box = app->text_box;

    // Today's punch list (all badges).
    tc_history_read(app->text_store, NULL, true);

    char first_in[8] = {0};
    char last_out[8] = {0};
    uint32_t minutes =
        tc_history_today_minutes(NULL, first_in, sizeof(first_in), last_out, sizeof(last_out));
    uint32_t h = minutes / 60;
    uint32_t m = minutes % 60;

    furi_string_cat_printf(
        app->text_store,
        "\nFirst in: %s\nLast out: %s\nTotal: %02lu:%02lu\n",
        first_in[0] ? first_in : "-",
        last_out[0] ? last_out : "-",
        (unsigned long)h,
        (unsigned long)m);

    text_box_reset(text_box);
    text_box_set_font(text_box, TextBoxFontText);
    text_box_set_text(text_box, furi_string_get_cstr(app->text_store));

    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewTextBox);
}

bool timeclock_scene_today_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void timeclock_scene_today_on_exit(void* context) {
    TimeClock* app = context;
    text_box_reset(app->text_box);
}
