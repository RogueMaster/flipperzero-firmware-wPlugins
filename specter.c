#include "specter_i.h"
#include <stdio.h>
#include <string.h>

/* ---------------- alert feedback (gated by settings) ---------------- */
static const NotificationSequence seq_led_magenta = {
    &message_red_255,
    &message_blue_255,
    &message_delay_100,
    &message_red_0,
    &message_blue_0,
    NULL,
};
static const NotificationSequence seq_led_magenta_blink = {
    &message_red_255,
    &message_blue_255,
    &message_delay_10,
    &message_red_0,
    &message_blue_0,
    NULL,
};
static const NotificationSequence seq_led_green_blip = {
    &message_green_255,
    &message_delay_50,
    &message_green_0,
    NULL,
};
static const NotificationSequence seq_vibro_short = {
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    NULL,
};
static const NotificationSequence seq_snd_found = {
    &message_note_c5,
    &message_delay_50,
    &message_note_e5,
    &message_delay_50,
    &message_note_g5,
    &message_delay_50,
    &message_sound_off,
    NULL,
};
static const NotificationSequence seq_snd_gone = {
    &message_note_g4,
    &message_delay_50,
    &message_sound_off,
    NULL,
};
static const NotificationSequence seq_snd_click = {
    &message_note_c7,
    &message_delay_10,
    &message_sound_off,
    NULL,
};
static const NotificationSequence seq_snd_saved = {
    &message_note_e6,
    &message_delay_50,
    &message_note_a6,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

/* Stealth darkens the screen and the LED but leaves sound and vibration to
 * their own settings: the point is not to glow while you are the one doing the
 * looking, not to disable the feedback you are sweeping by. */
static bool specter_lights_allowed(const SpecterApp* app) {
    return app->settings.led && !app->settings.stealth;
}

void specter_notify_found(SpecterApp* app) {
    furi_assert(app);
    if(specter_lights_allowed(app)) notification_message(app->notifications, &seq_led_magenta);
    if(app->settings.vibro) notification_message(app->notifications, &seq_vibro_short);
    if(app->settings.sound) notification_message(app->notifications, &seq_snd_found);
}

void specter_notify_gone(SpecterApp* app) {
    furi_assert(app);
    if(specter_lights_allowed(app)) notification_message(app->notifications, &seq_led_green_blip);
    if(app->settings.sound) notification_message(app->notifications, &seq_snd_gone);
}

void specter_notify_click(SpecterApp* app) {
    furi_assert(app);
    if(app->settings.sound) notification_message(app->notifications, &seq_snd_click);
}

void specter_notify_present_led(SpecterApp* app) {
    furi_assert(app);
    if(specter_lights_allowed(app))
        notification_message(app->notifications, &seq_led_magenta_blink);
}

void specter_notify_saved(SpecterApp* app) {
    furi_assert(app);
    if(app->settings.sound) notification_message(app->notifications, &seq_snd_saved);
    if(app->settings.vibro) notification_message(app->notifications, &seq_vibro_short);
}

/* ---------------- stealth ---------------- */
void specter_stealth_enter(SpecterApp* app) {
    furi_assert(app);
    if(!app->settings.stealth || app->stealth_engaged) return;
    notification_message(app->notifications, &sequence_display_backlight_off);
    app->stealth_engaged = true;
}

void specter_stealth_exit(SpecterApp* app) {
    furi_assert(app);
    if(!app->stealth_engaged) return;
    /* Hand the backlight back to the system's own timeout rather than forcing
     * it on - leaving it pinned would outlive the app. */
    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
    app->stealth_engaged = false;
}

void specter_apply_threshold(SpecterApp* app) {
    furi_assert(app);
    field_detector_set_threshold(app->detector, specter_settings_threshold(&app->settings));
}

/* ---------------- view dispatcher plumbing ---------------- */
static bool specter_custom_event_callback(void* context, uint32_t event) {
    SpecterApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool specter_back_event_callback(void* context) {
    SpecterApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void specter_tick_event_callback(void* context) {
    SpecterApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

/* ---------------- lifecycle ---------------- */
static SpecterApp* specter_app_alloc(void) {
    SpecterApp* app = malloc(sizeof(SpecterApp));
    memset(app, 0, sizeof(SpecterApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&specter_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, specter_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, specter_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, specter_tick_event_callback, 100);

    specter_settings_load(&app->settings);

    app->detector = field_detector_alloc();
    specter_apply_threshold(app);

    // shared GUI modules
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SpecterViewSubmenu, submenu_get_view(app->submenu));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        SpecterViewSettings,
        variable_item_list_get_view(app->var_item_list));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SpecterViewWidget, widget_get_view(app->widget));

    app->text_box = text_box_alloc();
    app->text_box_store = furi_string_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SpecterViewTextBox, text_box_get_view(app->text_box));

    // custom views
    app->sweep_view = sweep_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SpecterViewSweep, sweep_view_get_view(app->sweep_view));

    app->fingerprint_view = fingerprint_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        SpecterViewFingerprint,
        fingerprint_view_get_view(app->fingerprint_view));

    app->survey_view = survey_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SpecterViewSurvey, survey_view_get_view(app->survey_view));

    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void specter_app_free(SpecterApp* app) {
    furi_assert(app);

    field_detector_stop(app->detector);
    specter_stealth_exit(app);
    specter_settings_save(&app->settings);

    view_dispatcher_remove_view(app->view_dispatcher, SpecterViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, SpecterViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, SpecterViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, SpecterViewTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, SpecterViewSweep);
    view_dispatcher_remove_view(app->view_dispatcher, SpecterViewFingerprint);
    view_dispatcher_remove_view(app->view_dispatcher, SpecterViewSurvey);

    submenu_free(app->submenu);
    variable_item_list_free(app->var_item_list);
    widget_free(app->widget);
    text_box_free(app->text_box);
    furi_string_free(app->text_box_store);
    sweep_view_free(app->sweep_view);
    fingerprint_view_free(app->fingerprint_view);
    survey_view_free(app->survey_view);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    field_detector_free(app->detector);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t specter_app(void* p) {
    UNUSED(p);
    SpecterApp* app = specter_app_alloc();
    scene_manager_next_scene(app->scene_manager, SpecterSceneStart);
    view_dispatcher_run(app->view_dispatcher);
    specter_app_free(app);
    return 0;
}
