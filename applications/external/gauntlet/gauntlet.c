#include "gauntlet_i.h"
#include <string.h>

/* ---------------------------------------------------------- feedback ----- */
static const NotificationSequence seq_correct = {
    &message_green_255,
    &message_note_c5,
    &message_delay_50,
    &message_note_e5,
    &message_delay_50,
    &message_note_g5,
    &message_delay_50,
    &message_sound_off,
    &message_delay_100,
    &message_green_0,
    NULL,
};
static const NotificationSequence seq_wrong = {
    &message_red_255,
    &message_vibro_on,
    &message_note_a3,
    &message_delay_100,
    &message_sound_off,
    &message_vibro_off,
    &message_delay_50,
    &message_red_0,
    NULL,
};
static const NotificationSequence seq_blip = {
    &message_blue_255,
    &message_delay_10,
    &message_blue_0,
    NULL,
};

void gauntlet_notify_correct(GauntletApp* app) {
    furi_assert(app);
    if(app->sound || app->vibro || app->led)
        notification_message(app->notifications, &seq_correct);
}

void gauntlet_notify_wrong(GauntletApp* app) {
    furi_assert(app);
    if(app->sound || app->vibro || app->led)
        notification_message(app->notifications, &seq_wrong);
}

void gauntlet_notify_blip(GauntletApp* app) {
    furi_assert(app);
    if(app->led) notification_message(app->notifications, &seq_blip);
}

/* ------------------------------------------------ view dispatcher glue ---- */
static bool gauntlet_custom_event_callback(void* context, uint32_t event) {
    GauntletApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool gauntlet_back_event_callback(void* context) {
    GauntletApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void gauntlet_tick_event_callback(void* context) {
    GauntletApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

/* --------------------------------------------------------- lifecycle ----- */
static GauntletApp* gauntlet_app_alloc(void) {
    GauntletApp* app = malloc(sizeof(GauntletApp));
    memset(app, 0, sizeof(GauntletApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->storage = furi_record_open(RECORD_STORAGE);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&gauntlet_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, gauntlet_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, gauntlet_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, gauntlet_tick_event_callback, 100);

    /* defaults */
    app->sound = true;
    app->vibro = true;
    app->led = true;
    app->hints_free = true;

    /* heap-allocated library state */
    app->packs = malloc(sizeof(CtfPackInfo) * CTF_MAX_PACKS);
    app->pack = malloc(sizeof(CtfPack));
    app->progress = malloc(sizeof(CtfProgress));
    memset(app->pack, 0, sizeof(CtfPack));

    ctf_store_ensure_starter(app->storage);
    ctf_store_progress_load(app->storage, app->progress);

    /* shared modules */
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GauntletViewSubmenu, submenu_get_view(app->submenu));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GauntletViewTextInput, text_input_get_view(app->text_input));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        GauntletViewVarList,
        variable_item_list_get_view(app->var_item_list));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GauntletViewWidget, widget_get_view(app->widget));

    /* showpiece views */
    app->challenge_view = challenge_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        GauntletViewChallenge,
        challenge_view_get_view(app->challenge_view));

    app->result_view = result_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GauntletViewResult, result_view_get_view(app->result_view));

    app->toolkit_view = toolkit_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GauntletViewToolkit, toolkit_view_get_view(app->toolkit_view));

    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    return app;
}

static void gauntlet_app_free(GauntletApp* app) {
    furi_assert(app);

    view_dispatcher_remove_view(app->view_dispatcher, GauntletViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, GauntletViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, GauntletViewVarList);
    view_dispatcher_remove_view(app->view_dispatcher, GauntletViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, GauntletViewChallenge);
    view_dispatcher_remove_view(app->view_dispatcher, GauntletViewResult);
    view_dispatcher_remove_view(app->view_dispatcher, GauntletViewToolkit);

    submenu_free(app->submenu);
    text_input_free(app->text_input);
    variable_item_list_free(app->var_item_list);
    widget_free(app->widget);
    challenge_view_free(app->challenge_view);
    result_view_free(app->result_view);
    toolkit_view_free(app->toolkit_view);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    free(app->packs);
    free(app->pack);
    free(app->progress);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t gauntlet_app(void* p) {
    UNUSED(p);
    GauntletApp* app = gauntlet_app_alloc();
    scene_manager_next_scene(app->scene_manager, GauntletSceneStart);
    view_dispatcher_run(app->view_dispatcher);
    gauntlet_app_free(app);
    return 0;
}
