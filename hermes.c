#include "hermes_i.h"
#include <string.h>

/* ---------------------------------------------------------- feedback ------ */

static const NotificationSequence seq_found = {
    &message_green_255,
    &message_delay_50,
    &message_note_c5,
    &message_delay_50,
    &message_note_e5,
    &message_delay_50,
    &message_note_g5,
    &message_delay_50,
    &message_sound_off,
    &message_green_0,
    NULL,
};

static const NotificationSequence seq_lost = {
    &message_red_255,
    &message_note_gs4,
    &message_delay_100,
    &message_sound_off,
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

void hermes_notify_found(HermesApp* app, bool good) {
    furi_assert(app);
    if(!app->sound && !app->led) return;

    /* Not a ternary: the two sequences are arrays of different lengths, so the
     * branches have incompatible pointer types. */
    if(good) {
        notification_message(app->notifications, &seq_found);
    } else {
        notification_message(app->notifications, &seq_lost);
    }
}

void hermes_notify_blip(HermesApp* app) {
    furi_assert(app);
    if(app->led) notification_message(app->notifications, &seq_blip);
}

/* The point of a watch is that you are not looking at the screen, so this one
 * vibrates as well as flashing - it has to carry across a bench. */
static const NotificationSequence seq_trigger = {
    &message_blue_255,
    &message_vibro_on,
    &message_note_e5,
    &message_delay_100,
    &message_note_b5,
    &message_delay_100,
    &message_sound_off,
    &message_vibro_off,
    &message_blue_0,
    NULL,
};

void hermes_notify_trigger(HermesApp* app) {
    furi_assert(app);
    notification_message(app->notifications, &seq_trigger);
}

/* ------------------------------------------------ view dispatcher glue ---- */

static bool hermes_custom_event_callback(void* context, uint32_t event) {
    HermesApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool hermes_back_event_callback(void* context) {
    HermesApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void hermes_tick_event_callback(void* context) {
    HermesApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

/* --------------------------------------------------------- lifecycle ------ */

static HermesApp* hermes_app_alloc(void) {
    HermesApp* app = malloc(sizeof(HermesApp));
    memset(app, 0, sizeof(HermesApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&hermes_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, hermes_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, hermes_back_event_callback);
    /* ~60 fps: the detect scope and the console both want to feel live. */
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, hermes_tick_event_callback, 16);

    /* defaults: listen first, talk only when asked */
    app->port = HermesPortUsart;
    app->enter_mode = UartTapEnterCr;
    app->tx_enabled = true;
    app->local_echo = false;
    app->logging = false; // opt-in: writing to the SD card is the user's call
    app->sound = true;
    app->led = true;
    app->custom_baud = 115200;

    app->autobaud = autobaud_alloc();
    app->verifier = verifier_alloc();
    app->tap = uart_tap_alloc();
    app->term = term_alloc();
    app->log = session_log_alloc();
    app->selftest = selftest_alloc();
    app->trigger = trigger_alloc();
    app->script = script_alloc();

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HermesViewSubmenu, submenu_get_view(app->submenu));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HermesViewSettings, variable_item_list_get_view(app->var_item_list));

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, HermesViewWidget, widget_get_view(app->widget));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HermesViewTextInput, text_input_get_view(app->text_input));

    app->number_input = number_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HermesViewNumberInput, number_input_get_view(app->number_input));

    app->detect_view = detect_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HermesViewDetect, detect_view_get_view(app->detect_view));

    app->result_view = result_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HermesViewResult, result_view_get_view(app->result_view));

    app->console_view = console_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HermesViewConsole, console_view_get_view(app->console_view));

    app->wiring_view = wiring_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HermesViewWiring, wiring_view_get_view(app->wiring_view));

    app->selftest_view = selftest_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HermesViewSelfTest, selftest_view_get_view(app->selftest_view));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    return app;
}

static void hermes_app_free(HermesApp* app) {
    furi_assert(app);

    /* Hand the hardware back before anything that owns it goes away, and close
     * the capture so the file on the card is complete. */
    autobaud_stop(app->autobaud);
    verifier_stop(app->verifier);
    selftest_stop(app->selftest);
    uart_tap_close(app->tap);
    session_log_close(app->log);

    view_dispatcher_remove_view(app->view_dispatcher, HermesViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, HermesViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, HermesViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, HermesViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, HermesViewNumberInput);
    view_dispatcher_remove_view(app->view_dispatcher, HermesViewDetect);
    view_dispatcher_remove_view(app->view_dispatcher, HermesViewResult);
    view_dispatcher_remove_view(app->view_dispatcher, HermesViewConsole);
    view_dispatcher_remove_view(app->view_dispatcher, HermesViewWiring);
    view_dispatcher_remove_view(app->view_dispatcher, HermesViewSelfTest);

    submenu_free(app->submenu);
    variable_item_list_free(app->var_item_list);
    widget_free(app->widget);
    text_input_free(app->text_input);
    number_input_free(app->number_input);
    detect_view_free(app->detect_view);
    result_view_free(app->result_view);
    console_view_free(app->console_view);
    wiring_view_free(app->wiring_view);
    selftest_view_free(app->selftest_view);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    autobaud_free(app->autobaud);
    verifier_free(app->verifier);
    uart_tap_free(app->tap);
    term_free(app->term);
    session_log_free(app->log);
    selftest_free(app->selftest);
    trigger_free(app->trigger);
    script_free(app->script);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t hermes_app(void* p) {
    UNUSED(p);
    HermesApp* app = hermes_app_alloc();
    scene_manager_next_scene(app->scene_manager, HermesSceneStart);
    view_dispatcher_run(app->view_dispatcher);
    hermes_app_free(app);
    return 0;
}
