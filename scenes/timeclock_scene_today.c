// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// Daily summary: today's punches plus first-in, last-out, total worked time and
// total break time (idle time between the first IN and the last OUT).

static int today_hhmm_to_minutes(const char* s) {
    if(strlen(s) < 5) return -1;
    int h = (s[0] - '0') * 10 + (s[1] - '0');
    int m = (s[3] - '0') * 10 + (s[4] - '0');
    if(h < 0 || h > 23 || m < 0 || m > 59) return -1;
    return h * 60 + m;
}

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

    // Break time = span (first in -> last out) minus worked minutes.
    uint32_t brk = 0;
    if(first_in[0] && last_out[0]) {
        int fi = today_hhmm_to_minutes(first_in);
        int lo = today_hhmm_to_minutes(last_out);
        if(fi >= 0 && lo >= fi) {
            int b = (lo - fi) - (int)minutes;
            if(b > 0) brk = (uint32_t)b;
        }
    }

    furi_string_cat_printf(
        app->text_store,
        "\n%s: %s\n%s: %s\n%s: %02lu:%02lu\n%s: %02lu:%02lu\n",
        tc_str(StrFirstIn),
        first_in[0] ? first_in : "-",
        tc_str(StrLastOut),
        last_out[0] ? last_out : "-",
        tc_str(StrTotal),
        (unsigned long)h,
        (unsigned long)m,
        tc_str(StrBreak),
        (unsigned long)(brk / 60),
        (unsigned long)(brk % 60));

    // Warn if the Flipper clock is not set (dates/times would be wrong).
    char d[TC_DT_MAX];
    tc_now_date(d, sizeof(d));
    int year = (d[0] - '0') * 1000 + (d[1] - '0') * 100 + (d[2] - '0') * 10 + (d[3] - '0');
    if(year < 2020) {
        furi_string_cat_printf(app->text_store, "\n! %s", tc_str(StrClockNotSet));
    }

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
