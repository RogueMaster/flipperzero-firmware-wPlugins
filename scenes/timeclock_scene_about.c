// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// About / help: what the app does, how to use it, and the copyright notice.

void timeclock_scene_about_on_enter(void* context) {
    TimeClock* app = context;

    furi_string_set(
        app->text_store,
        "Time Clock\n"
        "Staff time-clock for Flipper Zero.\n"
        "\n"
        "How it works:\n"
        "- Badges: register a person on a\n"
        "  blank chip (name only).\n"
        "- Punch: tap the chip. It logs IN,\n"
        "  then OUT, then IN... automatically.\n"
        "- Work mode: kiosk clock; greets\n"
        "  Welcome/Goodbye. Exit needs PIN.\n"
        "- PIN: a fast 4-arrow sequence\n"
        "  (set at first launch or Settings).\n"
        "- Today / This week: worked time\n"
        "  and breaks.\n"
        "- Export: CSV/JSON on the SD card.\n"
        "- A person is tied to the chip UID;\n"
        "  lost chip -> Badges > Replace chip.\n"
        "\n"
        "Only the chip UID is read - no\n"
        "writing, no emulation.\n"
        "\n"
        "Copyright (C) 2026 Vladyslav\n"
        "Pereverzyev\n"
        "License: GPL-3.0-or-later\n");

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
