#include "argus_i.h"
#include <stdio.h>
#include <string.h>

/* ---------------- alert feedback (gated by settings) ---------------- */
static const NotificationSequence seq_led_red = {
    &message_red_255,
    &message_delay_250,
    &message_red_0,
    NULL,
};
static const NotificationSequence seq_led_blue = {
    &message_blue_255,
    &message_delay_250,
    &message_blue_0,
    NULL,
};
static const NotificationSequence seq_vibro_long = {
    &message_vibro_on,
    &message_delay_250,
    &message_vibro_off,
    NULL,
};
static const NotificationSequence seq_vibro_short = {
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    NULL,
};
static const NotificationSequence seq_snd_attack = {
    &message_note_g5,
    &message_delay_100,
    &message_note_e5,
    &message_delay_100,
    &message_note_c5,
    &message_delay_100,
    &message_sound_off,
    NULL,
};
static const NotificationSequence seq_snd_twin = {
    &message_note_c5,
    &message_delay_100,
    &message_note_e5,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

void argus_notify_attack(ArgusApp* app) {
    furi_assert(app);
    if(app->settings.led) notification_message(app->notifications, &seq_led_red);
    if(app->settings.vibro) notification_message(app->notifications, &seq_vibro_long);
    if(app->settings.sound) notification_message(app->notifications, &seq_snd_attack);
}

void argus_notify_twin(ArgusApp* app) {
    furi_assert(app);
    if(app->settings.led) notification_message(app->notifications, &seq_led_blue);
    if(app->settings.vibro) notification_message(app->notifications, &seq_vibro_short);
    if(app->settings.sound) notification_message(app->notifications, &seq_snd_twin);
}

bool argus_is_under_attack(ArgusApp* app, const ArgusStats* stats) {
    if(stats->last_deauth_tick == 0) return false;
    uint32_t now = furi_get_tick();
    if((uint32_t)(now - stats->last_deauth_tick) > ARGUS_ATTACK_HOLD_MS) return false;
    return stats->deauth_rate >= argus_settings_storm_threshold(&app->settings);
}

/* ---------------- link arming ---------------- */
void argus_link_arm(ArgusApp* app) {
    furi_assert(app);
    if(!uart_link_is_running(app->uart)) {
        uart_link_start(app->uart);
    }

    char cmd[16 + ARGUS_SSID_MAX];
    uart_link_send_command(app->uart, "START\n");

    snprintf(cmd, sizeof(cmd), "CHAN:%u\n", argus_settings_channel(&app->settings));
    uart_link_send_command(app->uart, cmd);

    char guard[ARGUS_SSID_MAX];
    argus_db_get_guard(app->db, guard, sizeof(guard));
    if(guard[0]) {
        snprintf(cmd, sizeof(cmd), "GUARD:%s\n", guard);
        uart_link_send_command(app->uart, cmd);
    }

    uart_link_send_command(app->uart, "PING\n"); // ask for an AXHELLO
}

void argus_link_disarm(ArgusApp* app) {
    furi_assert(app);
    if(uart_link_is_running(app->uart)) {
        uart_link_send_command(app->uart, "STOP\n");
        uart_link_stop(app->uart);
    }
    app->esp_connected = false;
}

/* ---------------- UART worker callbacks ---------------- */
static void argus_uart_deauth(
    void* ctx,
    ArgusThreatKind kind,
    const uint8_t src[6],
    const uint8_t bssid[6],
    uint8_t channel,
    int8_t rssi,
    uint8_t reason) {
    ArgusApp* app = ctx;
    app->last_rx_tick = furi_get_tick();
    argus_db_on_deauth(
        app->db,
        kind,
        src,
        bssid,
        channel,
        rssi,
        reason,
        argus_settings_storm_threshold(&app->settings));
}

static void argus_uart_ap(
    void* ctx,
    const uint8_t bssid[6],
    uint8_t channel,
    int8_t rssi,
    ArgusEnc enc,
    const char* ssid) {
    ArgusApp* app = ctx;
    app->last_rx_tick = furi_get_tick();
    bool new_twin = argus_db_on_ap(app->db, bssid, channel, rssi, enc, ssid);
    if(new_twin) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ArgusCustomEventTwinFound);
    }
}

static void argus_uart_status(void* ctx, bool connected, const char* version) {
    ArgusApp* app = ctx;
    app->last_rx_tick = furi_get_tick();
    app->esp_connected = connected;
    if(version) {
        strncpy(app->esp_version, version, sizeof(app->esp_version) - 1);
        app->esp_version[sizeof(app->esp_version) - 1] = '\0';
    }
}

/* ---------------- view dispatcher plumbing ---------------- */
static bool argus_custom_event_callback(void* context, uint32_t event) {
    ArgusApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool argus_back_event_callback(void* context) {
    ArgusApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void argus_tick_event_callback(void* context) {
    ArgusApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

/* ---------------- lifecycle ---------------- */
static ArgusApp* argus_app_alloc(void) {
    ArgusApp* app = malloc(sizeof(ArgusApp));
    memset(app, 0, sizeof(ArgusApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&argus_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, argus_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, argus_back_event_callback);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, argus_tick_event_callback, 100);

    // default settings
    app->settings.channel_index = 0; // Hop all channels
    app->settings.sensitivity_index = 1; // Medium
    app->settings.sound = true;
    app->settings.vibro = true;
    app->settings.led = true;

    app->db = argus_db_alloc();
    app->uart = uart_link_alloc();
    uart_link_set_callbacks(app->uart, argus_uart_deauth, argus_uart_ap, argus_uart_status, app);

    // shared GUI modules
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, ArgusViewSubmenu, submenu_get_view(app->submenu));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, ArgusViewSettings, variable_item_list_get_view(app->var_item_list));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, ArgusViewTextInput, text_input_get_view(app->text_input));

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, ArgusViewAbout, widget_get_view(app->widget));

    // custom views
    app->monitor_view = monitor_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, ArgusViewMonitor, monitor_view_get_view(app->monitor_view));

    app->ap_list_view = ap_list_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, ArgusViewApList, ap_list_view_get_view(app->ap_list_view));

    app->threat_log_view = threat_log_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, ArgusViewThreatLog, threat_log_view_get_view(app->threat_log_view));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void argus_app_free(ArgusApp* app) {
    furi_assert(app);

    // tear down the radio link first
    uart_link_stop(app->uart);
    uart_link_free(app->uart);

    view_dispatcher_remove_view(app->view_dispatcher, ArgusViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, ArgusViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, ArgusViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, ArgusViewAbout);
    view_dispatcher_remove_view(app->view_dispatcher, ArgusViewMonitor);
    view_dispatcher_remove_view(app->view_dispatcher, ArgusViewApList);
    view_dispatcher_remove_view(app->view_dispatcher, ArgusViewThreatLog);

    submenu_free(app->submenu);
    variable_item_list_free(app->var_item_list);
    text_input_free(app->text_input);
    widget_free(app->widget);
    monitor_view_free(app->monitor_view);
    ap_list_view_free(app->ap_list_view);
    threat_log_view_free(app->threat_log_view);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    argus_db_free(app->db);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t argus_app(void* p) {
    UNUSED(p);
    ArgusApp* app = argus_app_alloc();
    scene_manager_next_scene(app->scene_manager, ArgusSceneStart);
    view_dispatcher_run(app->view_dispatcher);
    argus_app_free(app);
    return 0;
}
