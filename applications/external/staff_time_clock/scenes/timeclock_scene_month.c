// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// Monthly summary: worked time this month per collaborator (computed per person,
// so different people's punches never mix) plus the grand total.

void timeclock_scene_month_on_enter(void* context) {
    TimeClock* app = context;
    FuriString* s = app->text_store;
    furi_string_reset(s);

    char date[TC_DT_MAX];
    tc_now_date(date, sizeof(date)); // YYYY-MM-DD
    if(!tc_date_is_valid()) {
        furi_string_cat_printf(s, "! %s\n\n", tc_str(StrClockNotSet));
    }

    char month[8];
    strncpy(month, date, 7); // YYYY-MM
    month[7] = '\0';

    uint32_t grand = 0;
    for(size_t i = 0; i < app->badge_count; i++) {
        uint32_t m = tc_history_month_minutes(month, app->badges[i].uid);
        grand += m;
        furi_string_cat_printf(
            s,
            "%s  %02lu:%02lu\n",
            app->badges[i].name,
            (unsigned long)(m / 60),
            (unsigned long)(m % 60));
    }
    if(app->badge_count == 0) {
        furi_string_cat_printf(s, "%s\n", tc_str(StrNoPunches));
    }
    furi_string_cat_printf(
        s,
        "\n%s: %02lu:%02lu",
        tc_str(StrTotal),
        (unsigned long)(grand / 60),
        (unsigned long)(grand % 60));

    TextBox* text_box = app->text_box;
    text_box_reset(text_box);
    text_box_set_font(text_box, TextBoxFontText);
    text_box_set_text(text_box, furi_string_get_cstr(s));
    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewTextBox);
}

bool timeclock_scene_month_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void timeclock_scene_month_on_exit(void* context) {
    TimeClock* app = context;
    text_box_reset(app->text_box);
}
