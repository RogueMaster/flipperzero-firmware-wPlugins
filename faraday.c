#include "faraday_i.h"
#include <string.h>

/* ---------------- feedback ---------------- */
static const NotificationSequence seq_snd_lock = {
    &message_note_e5,
    &message_delay_50,
    &message_note_a5,
    &message_delay_50,
    &message_sound_off,
    NULL,
};
static const NotificationSequence seq_snd_reject = {
    &message_note_c4,
    &message_delay_100,
    &message_sound_off,
    NULL,
};
static const NotificationSequence seq_snd_pass = {
    &message_note_c5,
    &message_delay_50,
    &message_note_e5,
    &message_delay_50,
    &message_note_g5,
    &message_delay_100,
    &message_sound_off,
    NULL,
};
static const NotificationSequence seq_snd_fail = {
    &message_note_g4,
    &message_delay_100,
    &message_note_ds4,
    &message_delay_250,
    &message_sound_off,
    NULL,
};
static const NotificationSequence seq_led_lock = {
    &message_blue_255,
    &message_delay_50,
    &message_blue_0,
    NULL,
};
static const NotificationSequence seq_led_pass = {
    &message_green_255,
    &message_delay_250,
    &message_green_0,
    NULL,
};
static const NotificationSequence seq_led_fail = {
    &message_red_255,
    &message_delay_250,
    &message_red_0,
    NULL,
};
static const NotificationSequence seq_snd_click = {
    &message_note_c7,
    &message_delay_10,
    &message_sound_off,
    NULL,
};

void faraday_notify_lock(FaradayApp* app) {
    furi_assert(app);
    if(app->settings.led) notification_message(app->notifications, &seq_led_lock);
    if(app->settings.sound) notification_message(app->notifications, &seq_snd_lock);
}

void faraday_notify_reject(FaradayApp* app) {
    furi_assert(app);
    if(app->settings.sound) notification_message(app->notifications, &seq_snd_reject);
}

void faraday_notify_verdict(FaradayApp* app, uint8_t rating) {
    furi_assert(app);
    /* B or better is a pouch worth trusting; C and below is a warning.
     * Sequences of different lengths are different array types, so these
     * cannot share a ternary. */
    bool good = rating <= (uint8_t)FdyRatingB;
    if(app->settings.led) {
        if(good)
            notification_message(app->notifications, &seq_led_pass);
        else
            notification_message(app->notifications, &seq_led_fail);
    }
    if(app->settings.sound) {
        if(good)
            notification_message(app->notifications, &seq_snd_pass);
        else
            notification_message(app->notifications, &seq_snd_fail);
    }
}

void faraday_notify_click(FaradayApp* app) {
    furi_assert(app);
    if(app->settings.sound) notification_message(app->notifications, &seq_snd_click);
}

/* ---------------- result log ---------------- */
void faraday_log_result(FaradayApp* app, bool is_nfc, uint32_t frequency) {
    furi_assert(app);
    const FdyTest* t = &app->test;

    FdyResult r = {
        .is_nfc = is_nfc,
        .frequency = frequency,
        .base_value = t->base_value,
        .shield_value = t->shield_value,
        .atten = t->atten,
        .floored = t->atten_floored,
        .rating = t->rating,
    };
    fdy_store_result_append(&r);
}

/* ---------------- test state ---------------- */
void fdy_test_reset(FdyTest* t) {
    furi_assert(t);
    memset(t, 0, sizeof(FdyTest));
    t->phase = FdyPhaseBaseline;
}

/* ---------------- view dispatcher plumbing ---------------- */
static bool faraday_custom_event_callback(void* context, uint32_t event) {
    FaradayApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool faraday_back_event_callback(void* context) {
    FaradayApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void faraday_tick_event_callback(void* context) {
    FaradayApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

/* ---------------- lifecycle ---------------- */
static FaradayApp* faraday_app_alloc(void) {
    FaradayApp* app = malloc(sizeof(FaradayApp));
    memset(app, 0, sizeof(FaradayApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&faraday_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, faraday_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, faraday_back_event_callback);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, faraday_tick_event_callback, 100);

    // defaults, then whatever was saved last run (load leaves these alone if
    // there is no valid file)
    app->settings.band_index = 1; // 433.92 MHz
    app->settings.sound = true;
    app->settings.led = true;
    fdy_store_settings_load(&app->settings);
    fdy_test_reset(&app->test);

    app->subghz = fdy_subghz_alloc(app->view_dispatcher);
    app->nfc = fdy_nfc_alloc();

    // shared GUI modules
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FaradayViewSubmenu, submenu_get_view(app->submenu));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FaradayViewSettings, variable_item_list_get_view(app->var_item_list));

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, FaradayViewAbout, widget_get_view(app->widget));

    // the measurement screen
    app->meter_view = meter_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FaradayViewMeter, meter_view_get_view(app->meter_view));

    // the leak-hunt screen
    app->hunt_view = hunt_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FaradayViewHunt, hunt_view_get_view(app->hunt_view));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void faraday_app_free(FaradayApp* app) {
    furi_assert(app);

    fdy_subghz_stop(app->subghz);
    fdy_nfc_stop(app->nfc);

    fdy_store_settings_save(&app->settings);

    view_dispatcher_remove_view(app->view_dispatcher, FaradayViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, FaradayViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, FaradayViewAbout);
    view_dispatcher_remove_view(app->view_dispatcher, FaradayViewMeter);
    view_dispatcher_remove_view(app->view_dispatcher, FaradayViewHunt);

    submenu_free(app->submenu);
    variable_item_list_free(app->var_item_list);
    widget_free(app->widget);
    meter_view_free(app->meter_view);
    hunt_view_free(app->hunt_view);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    fdy_subghz_free(app->subghz);
    fdy_nfc_free(app->nfc);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t faraday_app(void* p) {
    UNUSED(p);
    FaradayApp* app = faraday_app_alloc();
    scene_manager_next_scene(app->scene_manager, FaradaySceneStart);
    view_dispatcher_run(app->view_dispatcher);
    faraday_app_free(app);
    return 0;
}
