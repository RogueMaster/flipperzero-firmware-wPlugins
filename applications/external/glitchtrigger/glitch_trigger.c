#include "glitch_trigger_i.h"
#include <string.h>

/* ---------------------------------------------------------- feedback ----- */
static const NotificationSequence seq_fire = {
    &message_red_255,
    &message_vibro_on,
    &message_delay_10,
    &message_vibro_off,
    &message_note_c6,
    &message_delay_10,
    &message_sound_off,
    &message_red_0,
    NULL,
};
static const NotificationSequence seq_arm = {
    &message_green_255,
    &message_note_g5,
    &message_delay_50,
    &message_sound_off,
    NULL,
};
static const NotificationSequence seq_disarm = {
    &message_green_0,
    &message_note_c5,
    &message_delay_50,
    &message_sound_off,
    NULL,
};
static const NotificationSequence seq_hit = {
    &message_blue_255,
    &message_note_e6,
    &message_delay_50,
    &message_note_a6,
    &message_delay_50,
    &message_sound_off,
    &message_blue_0,
    NULL,
};

void glitch_notify_fire(GlitchApp* app) {
    furi_assert(app);
    if(app->led || app->sound || app->vibro) notification_message(app->notifications, &seq_fire);
}
void glitch_notify_arm(GlitchApp* app) {
    furi_assert(app);
    if(app->led || app->sound) notification_message(app->notifications, &seq_arm);
}
void glitch_notify_disarm(GlitchApp* app) {
    furi_assert(app);
    if(app->led || app->sound) notification_message(app->notifications, &seq_disarm);
}
void glitch_notify_hit(GlitchApp* app) {
    furi_assert(app);
    if(app->led || app->sound) notification_message(app->notifications, &seq_hit);
}

/* ------------------------------------------------ view dispatcher glue ---- */
static bool glitch_custom_event_callback(void* context, uint32_t event) {
    GlitchApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}
static bool glitch_back_event_callback(void* context) {
    GlitchApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}
static void glitch_tick_event_callback(void* context) {
    GlitchApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

/* --------------------------------------------------------- lifecycle ----- */
static GlitchApp* glitch_app_alloc(void) {
    GlitchApp* app = malloc(sizeof(GlitchApp));
    memset(app, 0, sizeof(GlitchApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&glitch_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, glitch_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, glitch_back_event_callback);
    // ~30 fps: smooth enough for the armed blink, repeat firing and sweep bar.
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, glitch_tick_event_callback, 32);

    app->sound = true;
    app->vibro = true;
    app->led = true;
    glitch_params_default(&app->params);
    app->engine = glitch_engine_alloc();

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GlitchViewSubmenu, submenu_get_view(app->submenu));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GlitchViewVarList, variable_item_list_get_view(app->var_item_list));

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, GlitchViewWidget, widget_get_view(app->widget));

    app->trigger_view = trigger_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GlitchViewTrigger, trigger_view_get_view(app->trigger_view));

    app->sweep_view = sweep_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GlitchViewSweep, sweep_view_get_view(app->sweep_view));

    app->wiring_view = wiring_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GlitchViewWiring, wiring_view_get_view(app->wiring_view));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    return app;
}

static void glitch_app_free(GlitchApp* app) {
    furi_assert(app);

    glitch_engine_release(app->engine);

    view_dispatcher_remove_view(app->view_dispatcher, GlitchViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, GlitchViewVarList);
    view_dispatcher_remove_view(app->view_dispatcher, GlitchViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, GlitchViewTrigger);
    view_dispatcher_remove_view(app->view_dispatcher, GlitchViewSweep);
    view_dispatcher_remove_view(app->view_dispatcher, GlitchViewWiring);

    submenu_free(app->submenu);
    variable_item_list_free(app->var_item_list);
    widget_free(app->widget);
    trigger_view_free(app->trigger_view);
    sweep_view_free(app->sweep_view);
    wiring_view_free(app->wiring_view);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    glitch_engine_free(app->engine);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t glitch_trigger_app(void* p) {
    UNUSED(p);
    GlitchApp* app = glitch_app_alloc();
    scene_manager_next_scene(app->scene_manager, GlitchSceneStart);
    view_dispatcher_run(app->view_dispatcher);
    glitch_app_free(app);
    return 0;
}
