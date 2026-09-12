// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// Text input for a badge name.
// Scene state: 0 = create a new badge (uses app->scanned_uid / scanned_tech),
//              1 = rename the badge at app->selected_index.

#define NAME_INPUT_DONE 210

static void timeclock_scene_name_input_callback(void* context) {
    TimeClock* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, NAME_INPUT_DONE);
}

void timeclock_scene_name_input_on_enter(void* context) {
    TimeClock* app = context;
    TextInput* text_input = app->text_input;

    uint32_t state = scene_manager_get_scene_state(app->scene_manager, TimeClockSceneNameInput);
    bool rename = (state == 1);

    if(rename && app->selected_index >= 0) {
        strncpy(app->name_buf, app->badges[app->selected_index].name, TC_NAME_MAX - 1);
        app->name_buf[TC_NAME_MAX - 1] = '\0';
    } else {
        app->name_buf[0] = '\0';
    }

    text_input_reset(text_input);
    text_input_set_header_text(text_input, "Badge name");
    text_input_set_result_callback(
        text_input,
        timeclock_scene_name_input_callback,
        app,
        app->name_buf,
        TC_NAME_MAX,
        !rename); // clear default text only when creating

    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewTextInput);
}

bool timeclock_scene_name_input_on_event(void* context, SceneManagerEvent event) {
    TimeClock* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == NAME_INPUT_DONE) {
        consumed = true;

        if(app->name_buf[0] == '\0') {
            strncpy(app->name_buf, "Badge", TC_NAME_MAX - 1);
            app->name_buf[TC_NAME_MAX - 1] = '\0';
        }

        uint32_t state =
            scene_manager_get_scene_state(app->scene_manager, TimeClockSceneNameInput);

        if(state == 1 && app->selected_index >= 0) {
            // Rename existing badge.
            strncpy(app->badges[app->selected_index].name, app->name_buf, TC_NAME_MAX - 1);
            app->badges[app->selected_index].name[TC_NAME_MAX - 1] = '\0';
            tc_badges_save(app->badges, app->badge_count);
            scene_manager_previous_scene(app->scene_manager);
        } else {
            // Create a new badge.
            if(app->badge_count < TC_MAX_BADGES) {
                char dt[TC_DT_MAX];
                tc_now_datetime(dt, sizeof(dt));

                Badge* b = &app->badges[app->badge_count];
                memset(b, 0, sizeof(Badge));
                strncpy(b->uid, app->scanned_uid, TC_UID_STR_MAX - 1);
                strncpy(b->name, app->name_buf, TC_NAME_MAX - 1);
                strncpy(b->tech, app->scanned_tech, TC_TECH_MAX - 1);
                strncpy(b->created, dt, TC_DT_MAX - 1);
                strncpy(b->last_used, dt, TC_DT_MAX - 1);
                b->last_event = TcEventNone;

                app->found_index = (int)app->badge_count;
                app->badge_count++;
                tc_badges_save(app->badges, app->badge_count);

                // Proceed to choose IN / OUT for the freshly registered badge.
                scene_manager_next_scene(app->scene_manager, TimeClockSceneBadgeAction);
            } else {
                timeclock_notify_error(app);
                scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, TimeClockSceneMenu);
            }
        }
    }

    return consumed;
}

void timeclock_scene_name_input_on_exit(void* context) {
    UNUSED(context);
}
