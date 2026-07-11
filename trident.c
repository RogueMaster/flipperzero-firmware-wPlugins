#include "trident_i.h"
#include <string.h>

/* ---------------- feedback (gated by settings) ---------------- */
static const NotificationSequence seq_led_red = {
    &message_red_255,
    &message_delay_250,
    &message_red_0,
    NULL,
};
static const NotificationSequence seq_vibro = {
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    NULL,
};
static const NotificationSequence seq_snd_start = {
    &message_note_c5,
    &message_delay_100,
    &message_note_g5,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

void trident_notify_start(TridentApp* app) {
    furi_assert(app);
    if(app->settings.led) notification_message(app->notifications, &seq_led_red);
    if(app->settings.vibro) notification_message(app->notifications, &seq_vibro);
    if(app->settings.sound) notification_message(app->notifications, &seq_snd_start);
}

/* ---------------- ESP32 serial link ---------------- */
void trident_link_ensure(TridentApp* app) {
    furi_assert(app);
    if(marauder_uart_is_running(app->uart)) return;
    marauder_uart_set_channel(
        app->uart,
        app->settings.uart_channel == TridentUartLpuart ? MarauderUartChannelLpuart :
                                                           MarauderUartChannelUsart);
    app->last_rx_tick = 0;
    marauder_uart_start(app->uart);
}

void trident_link_disarm(TridentApp* app) {
    furi_assert(app);
    if(marauder_uart_is_running(app->uart)) {
        marauder_uart_send(app->uart, MARAUDER_CMD_STOP "\n");
        marauder_uart_stop(app->uart);
    }
    app->last_rx_tick = 0;
}

void trident_link_send(TridentApp* app, const char* cmd) {
    furi_assert(app);
    marauder_uart_send(app->uart, cmd);
}

bool trident_link_is_live(TridentApp* app) {
    furi_assert(app);
    if(!marauder_uart_is_running(app->uart)) return false;
    uint32_t last = app->last_rx_tick;
    if(last == 0) return false;
    return (uint32_t)(furi_get_tick() - last) < TRIDENT_LINK_TIMEOUT_MS;
}

/* ---------------- command launcher ---------------- */
void trident_launch(TridentApp* app, const char* title, const char* cmd, bool is_attack) {
    furi_assert(app);
    strncpy(app->pending_title, title, sizeof(app->pending_title) - 1);
    app->pending_title[sizeof(app->pending_title) - 1] = '\0';
    strncpy(app->pending_cmd, cmd, sizeof(app->pending_cmd) - 1);
    app->pending_cmd[sizeof(app->pending_cmd) - 1] = '\0';
    app->pending_is_attack = is_attack;

    if(is_attack && app->settings.confirm_attacks) {
        scene_manager_next_scene(app->scene_manager, TridentSceneConfirm);
    } else {
        if(is_attack) trident_notify_start(app);
        scene_manager_next_scene(app->scene_manager, TridentSceneConsole);
    }
}

void trident_prompt(
    TridentApp* app,
    const char* header,
    const char* prefix,
    const char* after_title,
    const char* after_cmd) {
    furi_assert(app);
    strncpy(app->input_header, header, sizeof(app->input_header) - 1);
    app->input_header[sizeof(app->input_header) - 1] = '\0';
    strncpy(app->input_prefix, prefix, sizeof(app->input_prefix) - 1);
    app->input_prefix[sizeof(app->input_prefix) - 1] = '\0';
    strncpy(
        app->input_after_title,
        after_title ? after_title : "",
        sizeof(app->input_after_title) - 1);
    app->input_after_title[sizeof(app->input_after_title) - 1] = '\0';
    strncpy(
        app->input_after_cmd, after_cmd ? after_cmd : "", sizeof(app->input_after_cmd) - 1);
    app->input_after_cmd[sizeof(app->input_after_cmd) - 1] = '\0';
    scene_manager_next_scene(app->scene_manager, TridentSceneInput);
}

/* ---------------- UART worker callback ---------------- */
static void trident_uart_line_cb(void* context, const char* line) {
    TridentApp* app = context;
    app->last_rx_tick = furi_get_tick();
    console_view_push_line(app->console_view, line);
}

/* ---------------- view dispatcher plumbing ---------------- */
static bool trident_custom_event_callback(void* context, uint32_t event) {
    TridentApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool trident_back_event_callback(void* context) {
    TridentApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void trident_tick_event_callback(void* context) {
    TridentApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

/* ---------------- lifecycle ---------------- */
static TridentApp* trident_app_alloc(void) {
    TridentApp* app = malloc(sizeof(TridentApp));
    memset(app, 0, sizeof(TridentApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&trident_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, trident_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, trident_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, trident_tick_event_callback, 100);

    // default settings
    app->settings.uart_channel = TridentUartUsart; // GPIO 13/14
    app->settings.cc1101_device = TridentCc1101Internal;
    app->settings.subghz_band = 0; // 433 band
    app->settings.autoscroll = true;
    app->settings.confirm_attacks = true;
    app->settings.sound = true;
    app->settings.vibro = true;
    app->settings.led = true;

    // radios
    app->uart = marauder_uart_alloc();
    marauder_uart_set_line_callback(app->uart, trident_uart_line_cb, app);
    app->nrf24 = nrf24_radio_alloc();
    app->subghz = subghz_radio_alloc();

    // shared GUI modules
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TridentViewSubmenu, submenu_get_view(app->submenu));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        TridentViewVarList,
        variable_item_list_get_view(app->var_item_list));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TridentViewTextInput, text_input_get_view(app->text_input));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TridentViewWidget, widget_get_view(app->widget));

    // custom views
    app->console_view = console_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TridentViewConsole, console_view_get_view(app->console_view));

    app->spectrum_view = spectrum_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TridentViewSpectrum, spectrum_view_get_view(app->spectrum_view));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void trident_app_free(TridentApp* app) {
    furi_assert(app);

    // tear the radios down first
    marauder_uart_stop(app->uart);
    marauder_uart_free(app->uart);
    nrf24_radio_stop(app->nrf24);
    nrf24_radio_free(app->nrf24);
    subghz_radio_stop(app->subghz);
    subghz_radio_free(app->subghz);

    view_dispatcher_remove_view(app->view_dispatcher, TridentViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, TridentViewVarList);
    view_dispatcher_remove_view(app->view_dispatcher, TridentViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, TridentViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, TridentViewConsole);
    view_dispatcher_remove_view(app->view_dispatcher, TridentViewSpectrum);

    submenu_free(app->submenu);
    variable_item_list_free(app->var_item_list);
    text_input_free(app->text_input);
    widget_free(app->widget);
    console_view_free(app->console_view);
    spectrum_view_free(app->spectrum_view);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t trident_app(void* p) {
    UNUSED(p);
    TridentApp* app = trident_app_alloc();
    scene_manager_next_scene(app->scene_manager, TridentSceneStart);
    view_dispatcher_run(app->view_dispatcher);
    trident_app_free(app);
    return 0;
}
