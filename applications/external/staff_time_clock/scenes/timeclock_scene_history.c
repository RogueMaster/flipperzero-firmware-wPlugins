// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"
#include <furi_hal_rtc.h>
#include <datetime/datetime.h>

// History view (TextBox). Scene state selects the filter:
//   0 = all, 1 = the selected badge (from Badges), 2 = today, 3 = this week.

void timeclock_scene_history_on_enter(void* context) {
    TimeClock* app = context;
    TextBox* text_box = app->text_box;

    uint32_t state = scene_manager_get_scene_state(app->scene_manager, TimeClockSceneHistory);

    if(state == 1 && app->selected_index >= 0) {
        tc_history_read_range(app->text_store, app->badges[app->selected_index].uid, NULL, NULL);
    } else if(state == 2) {
        char today[TC_DT_MAX];
        tc_now_date(today, sizeof(today));
        tc_history_read_range(app->text_store, NULL, today, today);
    } else if(state == 3) {
        DateTime now;
        furi_hal_rtc_get_datetime(&now);
        uint32_t ts = datetime_datetime_to_timestamp(&now);
        uint32_t days = ts / 86400u;
        int dow = (int)((days + 4u) % 7u); // 0 = Sunday
        int since_monday = (dow + 6) % 7;
        DateTime mon;
        datetime_timestamp_to_datetime(ts - (uint32_t)since_monday * 86400u, &mon);
        char from[TC_DT_MAX];
        char today[TC_DT_MAX];
        snprintf(from, sizeof(from), "%04u-%02u-%02u", mon.year, mon.month, mon.day);
        tc_now_date(today, sizeof(today));
        tc_history_read_range(app->text_store, NULL, from, today);
    } else {
        tc_history_read_range(app->text_store, NULL, NULL, NULL);
    }

    text_box_reset(text_box);
    text_box_set_font(text_box, TextBoxFontText);
    text_box_set_text(text_box, furi_string_get_cstr(app->text_store));

    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewTextBox);
}

bool timeclock_scene_history_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false; // Back pops the scene
}

void timeclock_scene_history_on_exit(void* context) {
    TimeClock* app = context;
    // Reset the filter so the next entry defaults to "all".
    scene_manager_set_scene_state(app->scene_manager, TimeClockSceneHistory, 0);
    text_box_reset(app->text_box);
}
