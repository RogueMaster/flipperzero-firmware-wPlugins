#include "rosetta_i.h"
#include <string.h>

/* ---------------------------------------------------------- feedback ----- */
static const NotificationSequence seq_good = {
    &message_green_255,
    &message_delay_100,
    &message_note_c5,
    &message_delay_50,
    &message_note_g5,
    &message_delay_50,
    &message_sound_off,
    &message_green_0,
    NULL,
};
static const NotificationSequence seq_bad = {
    &message_red_255,
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    &message_note_gs4,
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

void rosetta_notify_capture(RosettaApp* app, bool good) {
    furi_assert(app);
    const NotificationSequence* seq = good ? &seq_good : &seq_bad;
    if(app->led || app->sound || app->vibro) {
        notification_message(app->notifications, seq);
    }
}

void rosetta_notify_blip(RosettaApp* app) {
    furi_assert(app);
    if(app->led) notification_message(app->notifications, &seq_blip);
}

/* ------------------------------------------------ view dispatcher glue ---- */
static bool rosetta_custom_event_callback(void* context, uint32_t event) {
    RosettaApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool rosetta_back_event_callback(void* context) {
    RosettaApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void rosetta_tick_event_callback(void* context) {
    RosettaApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

/* --------------------------------------------------------- lifecycle ----- */
static RosettaApp* rosetta_app_alloc(void) {
    RosettaApp* app = malloc(sizeof(RosettaApp));
    memset(app, 0, sizeof(RosettaApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&rosetta_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, rosetta_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, rosetta_back_event_callback);
    // ~60 fps ticks keep the animated walkthroughs and RF scope smooth.
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, rosetta_tick_event_callback, 16);

    app->sound = true;
    app->vibro = true;
    app->led = true;
    app->rf_freq_index = 0;

    app->nfc = nfc_reader_alloc();
    app->onewire = onewire_reader_alloc();
    app->rf = rf_scope_alloc();

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, RosettaViewSubmenu, submenu_get_view(app->submenu));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        RosettaViewSettings,
        variable_item_list_get_view(app->var_item_list));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, RosettaViewWidget, widget_get_view(app->widget));

    app->lesson_view = lesson_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, RosettaViewLesson, lesson_view_get_view(app->lesson_view));

    app->capture_view = capture_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, RosettaViewCapture, capture_view_get_view(app->capture_view));

    app->scope_view = scope_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, RosettaViewScope, scope_view_get_view(app->scope_view));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    return app;
}

static void rosetta_app_free(RosettaApp* app) {
    furi_assert(app);

    nfc_reader_stop(app->nfc);
    onewire_reader_stop(app->onewire);
    rf_scope_stop(app->rf);

    view_dispatcher_remove_view(app->view_dispatcher, RosettaViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, RosettaViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, RosettaViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, RosettaViewLesson);
    view_dispatcher_remove_view(app->view_dispatcher, RosettaViewCapture);
    view_dispatcher_remove_view(app->view_dispatcher, RosettaViewScope);

    submenu_free(app->submenu);
    variable_item_list_free(app->var_item_list);
    widget_free(app->widget);
    lesson_view_free(app->lesson_view);
    capture_view_free(app->capture_view);
    scope_view_free(app->scope_view);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    nfc_reader_free(app->nfc);
    onewire_reader_free(app->onewire);
    rf_scope_free(app->rf);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t rosetta_app(void* p) {
    UNUSED(p);
    RosettaApp* app = rosetta_app_alloc();
    scene_manager_next_scene(app->scene_manager, RosettaSceneStart);
    view_dispatcher_run(app->view_dispatcher);
    rosetta_app_free(app);
    return 0;
}
