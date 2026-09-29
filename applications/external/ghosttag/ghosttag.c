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

void ghosttag_backlight_hold(GhostTagApp* app, bool hold) {
    furi_assert(app);
    if(hold == app->backlight_forced) return;
    app->backlight_forced = hold;
    notification_message(
        app->notifications,
        hold ? &sequence_display_backlight_enforce_on : &sequence_display_backlight_enforce_auto);
}

/* ---- detection intake (runs on a worker thread, not the GUI thread) ---- */
static void ghosttag_on_detection(
    void* ctx,
    const uint8_t mac[6],
    TrackerType type,
    int8_t rssi,
    const char* name) {
    GhostTagApp* app = ctx;
    if(rssi < ghosttag_settings_rssi_cutoff(&app->settings)) return;

    /* Update the database and nothing else. The GUI picks the promotion up on
     * its next tick; see the note in ghosttag_i.h about why this thread must
     * not post into the view dispatcher. */
    tracker_db_update(app->db, mac, type, rssi, name, ghosttag_follow_threshold_ms(app));
}

static void ghosttag_uart_status(void* ctx, bool connected, const char* version) {
    GhostTagApp* app = ctx;
    app->esp_connected = connected;
    if(connected) app->esp_ever_seen = true;
    if(version) {
        strncpy(app->esp_version, version, sizeof(app->esp_version) - 1);
        app->esp_version[sizeof(app->esp_version) - 1] = '\0';
    }
}

/* ---- source control ---- */
bool ghosttag_is_hunting(GhostTagApp* app) {
    furi_assert(app);
    return uart_link_is_running(app->uart) || demo_source_is_running(app->demo);
}

uint32_t ghosttag_follow_threshold_ms(GhostTagApp* app) {
    furi_assert(app);
    /* The demo is scaled so somebody can actually watch it happen. */
    if(app->source == GhostTagSourceDemo) return DEMO_FOLLOW_MS;
    return ghosttag_settings_follow_ms(&app->settings);
}

void ghosttag_source_start(GhostTagApp* app, GhostTagSource source) {
    furi_assert(app);
    if(ghosttag_is_hunting(app)) {
        if(app->source == source) return;
        ghosttag_source_stop(app);
    }

    /* Only wipe when the KIND of session changes. Backing out to the menu and
     * starting the hunt again used to silently destroy everything found so
     * far, with no warning and no way to get it back. */
    if(app->db_source != source) {
        tracker_db_reset(app->db);
        app->db_source = source;
    }
    app->esp_connected = false;
    app->esp_ever_seen = false;
    app->source = source;

    if(source == GhostTagSourceDemo) {
        demo_source_start(app->demo);
    } else if(source == GhostTagSourceEsp32) {
        uart_link_start(app->uart);
        uart_link_send_command(app->uart, "START\n");
        /* A board that booted before the app opened has already sent its
         * GTHELLO into the void, so ask for one. */
        uart_link_send_command(app->uart, "PING\n");
        /* Only a real hunt is ever written down. */
        if(app->settings.log_session) session_log_begin(app->log);
    }

    ghosttag_backlight_hold(app, app->settings.keep_lit);
}

void ghosttag_source_stop(GhostTagApp* app) {
    furi_assert(app);
    if(uart_link_is_running(app->uart)) {
        uart_link_send_command(app->uart, "STOP\n");
        uart_link_stop(app->uart);
    }
    demo_source_stop(app->demo);
    session_log_end(app->log);
    app->source = GhostTagSourceNone;
    app->esp_connected = false;
    ghosttag_backlight_hold(app, false);
}

void ghosttag_clear_detections(GhostTagApp* app) {
    furi_assert(app);
    tracker_db_reset(app->db);
    app->db_source = app->source;
}

void ghosttag_update_link(GhostTagApp* app) {
    furi_assert(app);
    if(app->source != GhostTagSourceEsp32) {
        app->esp_connected = false;
        return;
    }
    /* Liveness is "the board said ANYTHING recently", not "we saw a tracker
     * recently". A quiet room is not a broken board. */
    uint32_t last = uart_link_last_rx_tick(app->uart);
    app->esp_ever_seen = uart_link_has_greeted(app->uart) || last != 0;
    app->esp_connected = (last != 0) && ((furi_get_tick() - last) < GHOSTTAG_ESP_TIMEOUT_MS);
}

bool ghosttag_poll_alert(GhostTagApp* app) {
    furi_assert(app);
    if(!tracker_db_take_pending_alert(app->db, &app->alert_record)) return false;

    /* Never write an invented tracker into a file that reads like evidence. */
    if(app->source == GhostTagSourceEsp32 && session_log_is_open(app->log)) {
        session_log_follower(app->log, &app->alert_record);
    }

    alert_view_set_record(app->alert_view, &app->alert_record, app->source == GhostTagSourceDemo);
    ghosttag_notify_alert(app);
    scene_manager_next_scene(app->scene_manager, GhostTagSceneAlert);
    return true;
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

    ghosttag_settings_defaults(&app->settings);
    ghosttag_settings_load(&app->settings);

    app->db = tracker_db_alloc();
    app->log = session_log_alloc();
    app->scratch = malloc(sizeof(TrackerRecord) * TRACKER_DB_MAX);

    app->uart = uart_link_alloc();
    uart_link_set_callbacks(app->uart, ghosttag_on_detection, ghosttag_uart_status, app);

    app->demo = demo_source_alloc();
    demo_source_set_callback(app->demo, ghosttag_on_detection, app);

    app->air = air_check_alloc();

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
    app->splash_view = splash_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GhostTagViewSplash, splash_view_get_view(app->splash_view));

    app->radar_view = radar_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GhostTagViewRadar, radar_view_get_view(app->radar_view));

    app->air_view = air_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GhostTagViewAir, air_view_get_view(app->air_view));

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

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void ghosttag_app_free(GhostTagApp* app) {
    furi_assert(app);

    /* Tear the radio down first: its worker calls back into the app. */
    ghosttag_source_stop(app);
    ghosttag_backlight_hold(app, false);
    uart_link_free(app->uart);
    demo_source_free(app->demo);
    /* Hands the Bluetooth radio back if an Air Check is somehow still live. */
    air_check_free(app->air);
    session_log_free(app->log);

    view_dispatcher_remove_view(app->view_dispatcher, GhostTagViewSplash);
    view_dispatcher_remove_view(app->view_dispatcher, GhostTagViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, GhostTagViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, GhostTagViewAbout);
    view_dispatcher_remove_view(app->view_dispatcher, GhostTagViewRadar);
    view_dispatcher_remove_view(app->view_dispatcher, GhostTagViewAir);
    view_dispatcher_remove_view(app->view_dispatcher, GhostTagViewDeviceList);
    view_dispatcher_remove_view(app->view_dispatcher, GhostTagViewDeviceDetail);
    view_dispatcher_remove_view(app->view_dispatcher, GhostTagViewAlert);

    submenu_free(app->submenu);
    variable_item_list_free(app->var_item_list);
    widget_free(app->widget);
    splash_view_free(app->splash_view);
    radar_view_free(app->radar_view);
    air_view_free(app->air_view);
    device_list_view_free(app->device_list_view);
    device_detail_view_free(app->device_detail_view);
    alert_view_free(app->alert_view);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    tracker_db_free(app->db);
    free(app->scratch);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t ghosttag_app(void* p) {
    UNUSED(p);
    GhostTagApp* app = ghosttag_app_alloc();
    /* Start sits UNDER the intro, so finishing the intro pops back to a menu
     * that is already on the stack. Pushing the menu on top of the intro
     * instead would make Back from the menu replay the intro rather than
     * leaving the app. */
    scene_manager_next_scene(app->scene_manager, GhostTagSceneStart);
    scene_manager_next_scene(app->scene_manager, GhostTagSceneSplash);
    view_dispatcher_run(app->view_dispatcher);
    ghosttag_app_free(app);
    return 0;
}
