// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// About / help: what the app does, how to use it, and the copyright notice.

void timeclock_scene_about_on_enter(void* context) {
    TimeClock* app = context;

    furi_string_set(app->text_store, tc_str(StrAboutText));
    furi_string_cat_str(
        app->text_store, "\nCopyright (C) 2026 Vladyslav Pereverzyev\nGPL-3.0-or-later\n");

    TextBox* text_box = app->text_box;
    text_box_reset(text_box);
    text_box_set_font(text_box, TextBoxFontText);
    text_box_set_text(text_box, furi_string_get_cstr(app->text_store));
    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewTextBox);
}

bool timeclock_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void timeclock_scene_about_on_exit(void* context) {
    TimeClock* app = context;
    text_box_reset(app->text_box);
}
