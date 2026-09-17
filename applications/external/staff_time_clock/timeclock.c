// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "timeclock.h"

// -----------------------------------------------------------------------------
// Shared helpers
// -----------------------------------------------------------------------------

const char* tc_event_str(TcEventType type) {
    switch(type) {
    case TcEventIn:
        return "IN";
    case TcEventOut:
        return "OUT";
    default:
        return "-";
    }
}

void timeclock_reload_badges(TimeClock* app) {
    app->badge_count = tc_badges_load(app->badges, TC_MAX_BADGES);
}

int timeclock_find_badge(TimeClock* app, const char* uid) {
    for(size_t i = 0; i < app->badge_count; i++) {
        if(strcmp(app->badges[i].uid, uid) == 0) {
            return (int)i;
        }
    }
    return -1;
}

void timeclock_notify_success(TimeClock* app) {
    notification_message(app->notifications, &sequence_success);
}

void timeclock_notify_error(TimeClock* app) {
    notification_message(app->notifications, &sequence_error);
}

// IN: ascending tones. OUT: descending tones. (sound only)
static const NotificationSequence timeclock_seq_in_sound = {
    &message_note_c5,
    &message_delay_100,
    &message_note_e5,
    &message_delay_100,
    &message_note_g5,
    &message_delay_100,
    &message_sound_off,
    NULL,
};
static const NotificationSequence timeclock_seq_out_sound = {
    &message_note_g5,
    &message_delay_100,
    &message_note_e5,
    &message_delay_100,
    &message_note_c5,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

// IN: one vibro pulse. OUT: two vibro pulses. (vibro only)
static const NotificationSequence timeclock_seq_in_vibro = {
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    NULL,
};
static const NotificationSequence timeclock_seq_out_vibro = {
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    &message_delay_100,
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    NULL,
};

// Single short cue for "a chip is here" (registration), distinct from the
// IN/OUT punch cues. Sound only.
static const NotificationSequence timeclock_seq_detected_sound = {
    &message_note_a5,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

void timeclock_notify_detected(TimeClock* app) {
    if(app->config.sound_enabled) {
        notification_message(app->notifications, &timeclock_seq_detected_sound);
    }
    if(app->config.vibro_enabled) {
        notification_message(app->notifications, &sequence_single_vibro);
    }
    if(app->config.led_enabled) {
        notification_message(app->notifications, &sequence_blink_white_100);
    }
}

void timeclock_notify_punch(TimeClock* app, TcEventType type) {
    bool in = (type == TcEventIn);
    // if/else (not a ternary) because the IN/OUT sequences differ in length,
    // so &seq_in and &seq_out are different pointer-to-array types.
    if(app->config.sound_enabled) {
        if(in) {
            notification_message(app->notifications, &timeclock_seq_in_sound);
        } else {
            notification_message(app->notifications, &timeclock_seq_out_sound);
        }
    }
    if(app->config.vibro_enabled) {
        if(in) {
            notification_message(app->notifications, &timeclock_seq_in_vibro);
        } else {
            notification_message(app->notifications, &timeclock_seq_out_vibro);
        }
    }
    if(app->config.led_enabled) {
        if(in) {
            notification_message(app->notifications, &sequence_blink_green_100);
        } else {
            notification_message(app->notifications, &sequence_blink_blue_100);
        }
    }
}

void timeclock_record_punch_type(TimeClock* app, int badge_index, TcEventType type) {
    Badge* b = &app->badges[badge_index];

    char date[TC_DT_MAX];
    char time[8];
    char dt[TC_DT_MAX];
    tc_now_date(date, sizeof(date));
    tc_now_time(time, sizeof(time));
    tc_now_datetime(dt, sizeof(dt));

    tc_history_append(date, time, b->name, b->uid, type);
    b->last_event = type;
    strncpy(b->last_used, dt, sizeof(b->last_used) - 1);
    b->last_used[sizeof(b->last_used) - 1] = '\0';
    tc_badges_save(app->badges, app->badge_count);

    timeclock_notify_punch(app, type);
}

TcEventType timeclock_record_punch(TimeClock* app, int badge_index) {
    Badge* b = &app->badges[badge_index];
    TcEventType type = (b->last_event == TcEventIn) ? TcEventOut : TcEventIn;
    timeclock_record_punch_type(app, badge_index, type);
    return type;
}

// -----------------------------------------------------------------------------
// ViewDispatcher navigation callbacks
// -----------------------------------------------------------------------------

static bool timeclock_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    TimeClock* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool timeclock_back_event_callback(void* context) {
    furi_assert(context);
    TimeClock* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void timeclock_tick_event_callback(void* context) {
    furi_assert(context);
    TimeClock* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

// -----------------------------------------------------------------------------
// Alloc / free
// -----------------------------------------------------------------------------

static TimeClock* timeclock_app_alloc(void) {
    TimeClock* app = malloc(sizeof(TimeClock));
    memset(app, 0, sizeof(TimeClock));

    app->found_index = -1;
    app->selected_index = -1;
    app->replace_index = -1;
    app->scan_purpose = TcScanPunch;

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&timeclock_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, timeclock_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, timeclock_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, timeclock_tick_event_callback, 500);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->text_store = furi_string_alloc();

    // GUI modules
    app->submenu = submenu_alloc();
    app->text_input = text_input_alloc();
    app->text_box = text_box_alloc();
    app->widget = widget_alloc();
    app->popup = popup_alloc();
    app->pin_view = pin_view_alloc();
    app->work_view = work_view_alloc();
    app->overview_view = overview_view_alloc();
    app->scan_view = scan_view_alloc();

    view_dispatcher_add_view(
        app->view_dispatcher, TimeClockViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(
        app->view_dispatcher, TimeClockViewTextInput, text_input_get_view(app->text_input));
    view_dispatcher_add_view(
        app->view_dispatcher, TimeClockViewTextBox, text_box_get_view(app->text_box));
    view_dispatcher_add_view(
        app->view_dispatcher, TimeClockViewWidget, widget_get_view(app->widget));
    view_dispatcher_add_view(app->view_dispatcher, TimeClockViewPopup, popup_get_view(app->popup));
    view_dispatcher_add_view(
        app->view_dispatcher, TimeClockViewPin, pin_view_get_view(app->pin_view));
    view_dispatcher_add_view(
        app->view_dispatcher, TimeClockViewWork, work_view_get_view(app->work_view));
    view_dispatcher_add_view(
        app->view_dispatcher, TimeClockViewOverview, overview_view_get_view(app->overview_view));
    view_dispatcher_add_view(
        app->view_dispatcher, TimeClockViewScan, scan_view_get_view(app->scan_view));

    // Persistence
    tc_storage_init();
    tc_config_load(&app->config);
    tc_lang_set((TcLang)app->config.language);
    timeclock_reload_badges(app);

    return app;
}

static void timeclock_app_free(TimeClock* app) {
    furi_assert(app);

    view_dispatcher_remove_view(app->view_dispatcher, TimeClockViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, TimeClockViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, TimeClockViewTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, TimeClockViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, TimeClockViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, TimeClockViewPin);
    view_dispatcher_remove_view(app->view_dispatcher, TimeClockViewWork);
    view_dispatcher_remove_view(app->view_dispatcher, TimeClockViewOverview);
    view_dispatcher_remove_view(app->view_dispatcher, TimeClockViewScan);

    submenu_free(app->submenu);
    text_input_free(app->text_input);
    text_box_free(app->text_box);
    widget_free(app->widget);
    popup_free(app->popup);
    pin_view_free(app->pin_view);
    work_view_free(app->work_view);
    overview_view_free(app->overview_view);
    scan_view_free(app->scan_view);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_string_free(app->text_store);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

// -----------------------------------------------------------------------------
// Entry point
// -----------------------------------------------------------------------------

int32_t timeclock_app(void* p) {
    UNUSED(p);
    TimeClock* app = timeclock_app_alloc();

    // If a PIN is configured, start locked; otherwise go straight to the menu.
    if(app->config.pin_enabled) {
        app->pin_mode = TcPinModeUnlock;
        scene_manager_next_scene(app->scene_manager, TimeClockScenePinUnlock);
    } else {
        scene_manager_next_scene(app->scene_manager, TimeClockSceneMenu);
        if(!app->config.onboarded) {
            // First launch: offer to set the protection PIN (on top of the menu).
            scene_manager_next_scene(app->scene_manager, TimeClockSceneOnboarding);
        }
    }
    if(!tc_date_is_valid()) {
        // On top of everything else, so a wrong clock can never be missed.
        scene_manager_next_scene(app->scene_manager, TimeClockSceneDateWarning);
    }

    view_dispatcher_run(app->view_dispatcher);

    timeclock_app_free(app);
    return 0;
}
