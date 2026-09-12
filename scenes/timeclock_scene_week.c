// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"
#include <furi_hal_rtc.h>
#include <datetime/datetime.h>

// Weekly summary: worked time per day of the current week (Monday..Sunday)
// plus the week total, across all collaborators.

void timeclock_scene_week_on_enter(void* context) {
    TimeClock* app = context;
    FuriString* s = app->text_store;
    furi_string_reset(s);

    DateTime now;
    furi_hal_rtc_get_datetime(&now);
    uint32_t now_ts = datetime_datetime_to_timestamp(&now);

    // Day of week from the timestamp: 1970-01-01 was a Thursday.
    uint32_t days = now_ts / 86400u;
    int dow = (int)((days + 4u) % 7u); // 0 = Sunday .. 6 = Saturday
    int since_monday = (dow + 6) % 7; // 0 if today is Monday
    uint32_t monday_ts = now_ts - (uint32_t)since_monday * 86400u;

    uint32_t total = 0;

    for(int i = 0; i < 7; i++) {
        DateTime d;
        datetime_timestamp_to_datetime(monday_ts + (uint32_t)i * 86400u, &d);
        char date[TC_DT_MAX];
        snprintf(date, sizeof(date), "%04u-%02u-%02u", d.year, d.month, d.day);

        uint32_t mins = tc_history_minutes_for_date(date, NULL, NULL, 0, NULL, 0);
        total += mins;

        furi_string_cat_printf(
            s,
            "%s %02u/%02u  %02lu:%02lu\n",
            tc_str((TcStr)(StrDowMon + i)),
            d.day,
            d.month,
            (unsigned long)(mins / 60),
            (unsigned long)(mins % 60));
    }

    furi_string_cat_printf(
        s,
        "\n%s: %02lu:%02lu",
        tc_str(StrWeekTotal),
        (unsigned long)(total / 60),
        (unsigned long)(total % 60));

    TextBox* text_box = app->text_box;
    text_box_reset(text_box);
    text_box_set_font(text_box, TextBoxFontText);
    text_box_set_text(text_box, furi_string_get_cstr(s));
    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewTextBox);
}

bool timeclock_scene_week_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void timeclock_scene_week_on_exit(void* context) {
    TimeClock* app = context;
    text_box_reset(app->text_box);
}
