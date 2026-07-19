#include "nyx_i.h"
#include <stdio.h>
#include <string.h>

/* ---------------- alert feedback (gated by settings) ----------------
 * The RGB LED only exposes full-on / full-off per channel, so "violet" is
 * red+blue (magenta) — the closest the hardware can get to Nyx's night theme. */
static const NotificationSequence seq_led_violet = {
    &message_red_255,
    &message_blue_255,
    &message_delay_100,
    &message_red_0,
    &message_blue_0,
    NULL,
};
static const NotificationSequence seq_led_violet_blink = {
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

void nyx_notify_found(NyxApp* app) {
    furi_assert(app);
    if(app->settings.led) notification_message(app->notifications, &seq_led_violet);
    if(app->settings.vibro) notification_message(app->notifications, &seq_vibro_short);
    if(app->settings.sound) notification_message(app->notifications, &seq_snd_found);
}

void nyx_notify_gone(NyxApp* app) {
    furi_assert(app);
    if(app->settings.led) notification_message(app->notifications, &seq_led_green_blip);
    if(app->settings.sound) notification_message(app->notifications, &seq_snd_gone);
}

void nyx_notify_click(NyxApp* app) {
    furi_assert(app);
    if(app->settings.sound) notification_message(app->notifications, &seq_snd_click);
}

void nyx_notify_present_led(NyxApp* app) {
    furi_assert(app);
    if(app->settings.led) notification_message(app->notifications, &seq_led_violet_blink);
}

/* ---------------- view dispatcher plumbing ---------------- */
static bool nyx_custom_event_callback(void* context, uint32_t event) {
    NyxApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool nyx_back_event_callback(void* context) {
    NyxApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void nyx_tick_event_callback(void* context) {
    NyxApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

/* ---------------- lifecycle ---------------- */
static NyxApp* nyx_app_alloc(void) {
    NyxApp* app = malloc(sizeof(NyxApp));
    memset(app, 0, sizeof(NyxApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&nyx_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, nyx_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, nyx_back_event_callback);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, nyx_tick_event_callback, 100);

    // default settings
    app->settings.mode_index = IrSenseModeAuto;
    app->settings.sensitivity_index = 1; // Medium
    app->settings.probe_pin_index = 0;
    app->settings.sound = true;
    app->settings.vibro = true;
    app->settings.led = true;

    app->sense = ir_sense_alloc();

    // shared GUI modules
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, NyxViewSubmenu, submenu_get_view(app->submenu));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NyxViewSettings, variable_item_list_get_view(app->var_item_list));

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, NyxViewAbout, widget_get_view(app->widget));

    // custom views
    app->sweep_view = sweep_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NyxViewSweep, sweep_view_get_view(app->sweep_view));

    app->probe_view = probe_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NyxViewProbe, probe_view_get_view(app->probe_view));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void nyx_app_free(NyxApp* app) {
    furi_assert(app);

    ir_sense_stop(app->sense);

    view_dispatcher_remove_view(app->view_dispatcher, NyxViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, NyxViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, NyxViewAbout);
    view_dispatcher_remove_view(app->view_dispatcher, NyxViewSweep);
    view_dispatcher_remove_view(app->view_dispatcher, NyxViewProbe);

    submenu_free(app->submenu);
    variable_item_list_free(app->var_item_list);
    widget_free(app->widget);
    sweep_view_free(app->sweep_view);
    probe_view_free(app->probe_view);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    ir_sense_free(app->sense);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t nyx_app(void* p) {
    UNUSED(p);
    NyxApp* app = nyx_app_alloc();
    scene_manager_next_scene(app->scene_manager, NyxSceneStart);
    view_dispatcher_run(app->view_dispatcher);
    nyx_app_free(app);
    return 0;
}
