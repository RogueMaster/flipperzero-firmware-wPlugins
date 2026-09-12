// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// History view (TextBox). Scene state: 0 = all badges, 1 = filter by the
// selected badge's UID.

void timeclock_scene_history_on_enter(void* context) {
    TimeClock* app = context;
    TextBox* text_box = app->text_box;

    uint32_t state = scene_manager_get_scene_state(app->scene_manager, TimeClockSceneHistory);
    const char* filter = NULL;
    if(state == 1 && app->selected_index >= 0) {
        filter = app->badges[app->selected_index].uid;
    }

    tc_history_read(app->text_store, filter, false);

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
    // Reset the filter so opening History from the menu shows everything.
    scene_manager_set_scene_state(app->scene_manager, TimeClockSceneHistory, 0);
    text_box_reset(app->text_box);
}
