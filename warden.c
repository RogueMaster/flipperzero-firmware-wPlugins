#include "warden_i.h"
#include <string.h>

/* ---------------------------------------------------------- feedback ----- */
static const NotificationSequence seq_secure = {
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
static const NotificationSequence seq_broken = {
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
static const NotificationSequence seq_blip = {
    &message_blue_255,
    &message_delay_10,
    &message_blue_0,
    NULL,
};

void warden_notify_graded(WardenApp* app, RiskBand band) {
    furi_assert(app);
    const NotificationSequence* seq;
    if(band == RiskSecure)
        seq = &seq_secure;
    else if(band == RiskCaution)
        seq = &seq_caution;
    else
        seq = &seq_broken; // Weak / Broken

    /* Split so the sound toggle can silence the tones but keep the LED/vibro. */
    if(app->led || app->sound || app->vibro) {
        notification_message(app->notifications, seq);
    }
}

void warden_notify_scan_blip(WardenApp* app) {
    furi_assert(app);
    if(app->led) notification_message(app->notifications, &seq_blip);
}

/* ------------------------------------------------ view dispatcher glue ---- */
static bool warden_custom_event_callback(void* context, uint32_t event) {
    WardenApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool warden_back_event_callback(void* context) {
    WardenApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void warden_tick_event_callback(void* context) {
    WardenApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

/* --------------------------------------------------------- lifecycle ----- */
static WardenApp* warden_app_alloc(void) {
    WardenApp* app = malloc(sizeof(WardenApp));
    memset(app, 0, sizeof(WardenApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&warden_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, warden_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, warden_back_event_callback);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, warden_tick_event_callback, 100);

    app->sound = true;
    app->vibro = true;
    app->led = true;

    app->reader = card_reader_alloc();

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, WardenViewSubmenu, submenu_get_view(app->submenu));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, WardenViewSettings, variable_item_list_get_view(app->var_item_list));

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, WardenViewWidget, widget_get_view(app->widget));

    app->scan_view = scan_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, WardenViewScan, scan_view_get_view(app->scan_view));

    app->result_view = result_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, WardenViewResult, result_view_get_view(app->result_view));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    return app;
}

static void warden_app_free(WardenApp* app) {
    furi_assert(app);

    card_reader_stop(app->reader);

    view_dispatcher_remove_view(app->view_dispatcher, WardenViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, WardenViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, WardenViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, WardenViewScan);
    view_dispatcher_remove_view(app->view_dispatcher, WardenViewResult);

    submenu_free(app->submenu);
    variable_item_list_free(app->var_item_list);
    widget_free(app->widget);
    scan_view_free(app->scan_view);
    result_view_free(app->result_view);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    card_reader_free(app->reader);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t warden_app(void* p) {
    UNUSED(p);
    WardenApp* app = warden_app_alloc();
    scene_manager_next_scene(app->scene_manager, WardenSceneStart);
    view_dispatcher_run(app->view_dispatcher);
    warden_app_free(app);
    return 0;
}
