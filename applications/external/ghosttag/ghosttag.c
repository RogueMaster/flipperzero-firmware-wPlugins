#include "ghosttag_i.h"

/* ---- alert feedback sequences (gated by settings) ---- */
static const NotificationSequence seq_alert_led = {
    &message_red_255,
    &message_delay_500,
    &message_red_0,
    NULL,
};
static const NotificationSequence seq_alert_vibro = {
    &message_vibro_on,
    &message_delay_250,
    &message_vibro_off,
    NULL,
};
static const NotificationSequence seq_alert_sound = {
    &message_note_c5,
    &message_delay_100,
    &message_note_e5,
    &message_delay_100,
    &message_note_g5,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

void ghosttag_notify_alert(GhostTagApp* app) {
    furi_assert(app);
    if(app->settings.led) notification_message(app->notifications, &seq_alert_led);
    if(app->settings.vibro) notification_message(app->notifications, &seq_alert_vibro);
    if(app->settings.sound) notification_message(app->notifications, &seq_alert_sound);
}

/* ---- UART worker callbacks (run on the link's worker thread) ---- */
static void ghosttag_uart_rx(
    void* ctx,
    const uint8_t mac[6],
    TrackerType type,
    int8_t rssi,
    const char* name) {
    GhostTagApp* app = ctx;
    app->last_rx_tick = furi_get_tick();
    if(rssi < ghosttag_settings_rssi_cutoff(&app->settings)) return;

    bool new_follower = tracker_db_update(
        app->db, mac, type, rssi, name, ghosttag_settings_follow_ms(&app->settings));
    if(new_follower) {
        view_dispatcher_send_custom_event(app->view_dispatcher, GhostTagCustomEventNewFollower);
    }
}

static void ghosttag_uart_status(void* ctx, bool connected, const char* version) {
    GhostTagApp* app = ctx;
    app->last_rx_tick = furi_get_tick();
    app->esp_connected = connected;
    if(version) {
        strncpy(app->esp_version, version, sizeof(app->esp_version) - 1);
        app->esp_version[sizeof(app->esp_version) - 1] = '\0';
    }
}

/* ---- view dispatcher callbacks ---- */
static bool ghosttag_custom_event_callback(void* context, uint32_t event) {
    GhostTagApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool ghosttag_back_event_callback(void* context) {
    GhostTagApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void ghosttag_tick_event_callback(void* context) {
    GhostTagApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

static GhostTagApp* ghosttag_app_alloc(void) {
    GhostTagApp* app = malloc(sizeof(GhostTagApp));
    memset(app, 0, sizeof(GhostTagApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&ghosttag_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, ghosttag_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, ghosttag_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, ghosttag_tick_event_callback, 100);

    // default settings
    app->settings.sensitivity_index = 1; // Medium
    app->settings.follow_index = 1; // 3 min
    app->settings.sound = true;
    app->settings.vibro = true;
    app->settings.led = true;

    app->db = tracker_db_alloc();
    app->uart = uart_link_alloc();
    uart_link_set_callbacks(app->uart, ghosttag_uart_rx, ghosttag_uart_status, app);

    // GUI modules
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GhostTagViewSubmenu, submenu_get_view(app->submenu));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        GhostTagViewSettings,
        variable_item_list_get_view(app->var_item_list));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GhostTagViewAbout, widget_get_view(app->widget));

    // custom views
    app->radar_view = radar_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GhostTagViewRadar, radar_view_get_view(app->radar_view));

    app->device_list_view = device_list_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        GhostTagViewDeviceList,
        device_list_view_get_view(app->device_list_view));

    app->device_detail_view = device_detail_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        GhostTagViewDeviceDetail,
        device_detail_view_get_view(app->device_detail_view));

    app->alert_view = alert_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GhostTagViewAlert, alert_view_get_view(app->alert_view));

    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void ghosttag_app_free(GhostTagApp* app) {
    furi_assert(app);

    // tear down the radio link first
    uart_link_stop(app->uart);
    uart_link_free(app->uart);

    view_dispatcher_remove_view(app->view_dispatcher, GhostTagViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, GhostTagViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, GhostTagViewAbout);
    view_dispatcher_remove_view(app->view_dispatcher, GhostTagViewRadar);
    view_dispatcher_remove_view(app->view_dispatcher, GhostTagViewDeviceList);
    view_dispatcher_remove_view(app->view_dispatcher, GhostTagViewDeviceDetail);
    view_dispatcher_remove_view(app->view_dispatcher, GhostTagViewAlert);

    submenu_free(app->submenu);
    variable_item_list_free(app->var_item_list);
    widget_free(app->widget);
    radar_view_free(app->radar_view);
    device_list_view_free(app->device_list_view);
    device_detail_view_free(app->device_detail_view);
    alert_view_free(app->alert_view);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    tracker_db_free(app->db);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t ghosttag_app(void* p) {
    UNUSED(p);
    GhostTagApp* app = ghosttag_app_alloc();
    scene_manager_next_scene(app->scene_manager, GhostTagSceneStart);
    view_dispatcher_run(app->view_dispatcher);
    ghosttag_app_free(app);
    return 0;
}
