#include "rollcall_i.h"
#include <string.h>

/* ---------------------------------------------------------- feedback ----- */
static const NotificationSequence seq_healthy = {
    &message_green_255,
    &message_delay_250,
    &message_green_0,
    &message_note_c5,
    &message_delay_50,
    &message_note_g5,
    &message_delay_50,
    &message_sound_off,
    NULL,
};
static const NotificationSequence seq_caution = {
    &message_red_255,
    &message_green_255, // red+green = amber
    &message_delay_250,
    &message_red_0,
    &message_green_0,
    &message_note_e5,
    &message_delay_100,
    &message_sound_off,
    NULL,
};
static const NotificationSequence seq_atrisk = {
    &message_red_255,
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    &message_delay_50,
    &message_note_gs4,
    &message_delay_100,
    &message_note_ds4,
    &message_delay_100,
    &message_sound_off,
    &message_red_0,
    NULL,
};
static const NotificationSequence seq_blip_led = {
    &message_blue_255,
    &message_delay_10,
    &message_blue_0,
    NULL,
};
static const NotificationSequence seq_blip_beep = {
    &message_note_c6,
    &message_delay_10,
    &message_sound_off,
    NULL,
};
static const NotificationSequence seq_blip_vibro = {
    &message_vibro_on,
    &message_delay_10,
    &message_vibro_off,
    NULL,
};

void rollcall_notify_verdict(RollCallApp* app, RcHealth health) {
    furi_assert(app);
    const NotificationSequence* seq;
    if(health == RcHealthHealthy || health == RcHealthLikely)
        seq = &seq_healthy;
    else if(health == RcHealthCaution || health == RcHealthUnknown)
        seq = &seq_caution;
    else
        seq = &seq_atrisk; // AtRisk

    if(app->settings.led || app->settings.sound || app->settings.vibro) {
        notification_message(app->notifications, seq);
    }
}

/* One short acknowledgement per registered press. Each channel is fired
 * separately so a user who turned the LED off but left sound on still gets
 * told that the press landed. */
void rollcall_notify_capture(RollCallApp* app) {
    furi_assert(app);
    if(app->settings.led) notification_message(app->notifications, &seq_blip_led);
    if(app->settings.sound) notification_message(app->notifications, &seq_blip_beep);
    if(app->settings.vibro) notification_message(app->notifications, &seq_blip_vibro);
}

/* ------------------------------------------------ view dispatcher glue ---- */
static bool rollcall_custom_event_callback(void* context, uint32_t event) {
    RollCallApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool rollcall_back_event_callback(void* context) {
    RollCallApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void rollcall_tick_event_callback(void* context) {
    RollCallApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

/* --------------------------------------------------------- lifecycle ----- */
static RollCallApp* rollcall_app_alloc(void) {
    RollCallApp* app = malloc(sizeof(RollCallApp));
    memset(app, 0, sizeof(RollCallApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&rollcall_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, rollcall_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, rollcall_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, rollcall_tick_event_callback, 100);

    /* Whatever the user picked last time, or shipped defaults on first run. */
    rc_settings_load(&app->settings);

    /* The radio posts progress using the app's own event id - keeping one
     * definition means the capture scene and the worker can never drift apart. */
    app->radio = rc_radio_alloc(app->view_dispatcher, RollCallCustomEventCapture);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, RollCallViewSubmenu, submenu_get_view(app->submenu));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        RollCallViewSettings,
        variable_item_list_get_view(app->var_item_list));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, RollCallViewWidget, widget_get_view(app->widget));

    app->capture_view = capture_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, RollCallViewCapture, capture_view_get_view(app->capture_view));

    app->hunt_view = hunt_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, RollCallViewHunt, hunt_view_get_view(app->hunt_view));

    app->verdict_view = verdict_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, RollCallViewVerdict, verdict_view_get_view(app->verdict_view));

    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    return app;
}

static void rollcall_app_free(RollCallApp* app) {
    furi_assert(app);

    rc_radio_hunt_stop(app->radio);
    rc_radio_stop(app->radio);

    view_dispatcher_remove_view(app->view_dispatcher, RollCallViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, RollCallViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, RollCallViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, RollCallViewCapture);
    view_dispatcher_remove_view(app->view_dispatcher, RollCallViewHunt);
    view_dispatcher_remove_view(app->view_dispatcher, RollCallViewVerdict);

    submenu_free(app->submenu);
    variable_item_list_free(app->var_item_list);
    widget_free(app->widget);
    capture_view_free(app->capture_view);
    hunt_view_free(app->hunt_view);
    verdict_view_free(app->verdict_view);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    rc_radio_free(app->radio);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t rollcall_app(void* p) {
    UNUSED(p);
    RollCallApp* app = rollcall_app_alloc();
    scene_manager_next_scene(app->scene_manager, RollCallSceneStart);
    view_dispatcher_run(app->view_dispatcher);
    rollcall_app_free(app);
    return 0;
}
