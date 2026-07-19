#include "cerberus_i.h"

#include <storage/storage.h>
#include <toolbox/saved_struct.h>

#ifndef APP_DATA_PATH
#define APP_DATA_PATH(p) EXT_PATH("apps_data/cerberus/" p)
#endif

#define CERBERUS_CONFIG_PATH    APP_DATA_PATH("cerberus.conf")
#define CERBERUS_CONFIG_MAGIC   0xC5
#define CERBERUS_CONFIG_VERSION 2
#define CERBERUS_CSV_PATH       APP_DATA_PATH("alerts.csv")

// --- Custom alert feedback sequences ---------------------------------------

static const NotificationSequence sequence_cerberus_alert_tone = {
    &message_note_g5,
    &message_delay_100,
    &message_note_c5,
    &message_delay_100,
    &message_note_g5,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

static const NotificationSequence sequence_cerberus_led = {
    &message_red_255,
    &message_delay_250,
    &message_red_0,
    NULL,
};

static const NotificationSequence sequence_cerberus_vibro = {
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    NULL,
};

// --- Config persistence -----------------------------------------------------

static void cerberus_config_default(CerberusConfig* c) {
    c->scan_mode = CerberusScanModeHop;
    c->sensitivity = 1; // medium
    c->detect_jam = true;
    c->detect_flood = true;
    c->detect_replay = true;
    c->notify_sound = true;
    c->notify_vibro = true;
    c->notify_led = true;
    c->log_csv = false; // opt-in: writes to the SD card
    c->keep_screen_on = true; // it's a desk monitor - keep it lit while watching
}

static void cerberus_config_load(CerberusConfig* c) {
    if(!saved_struct_load(
           CERBERUS_CONFIG_PATH,
           c,
           sizeof(CerberusConfig),
           CERBERUS_CONFIG_MAGIC,
           CERBERUS_CONFIG_VERSION)) {
        cerberus_config_default(c);
    }
}

static void cerberus_config_save(CerberusConfig* c) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, APP_DATA_PATH(""));
    furi_record_close(RECORD_STORAGE);
    saved_struct_save(
        CERBERUS_CONFIG_PATH,
        c,
        sizeof(CerberusConfig),
        CERBERUS_CONFIG_MAGIC,
        CERBERUS_CONFIG_VERSION);
}

// --- Config -> worker -------------------------------------------------------

void cerberus_apply_config(Cerberus* app) {
    CerberusWorkerConfig wc = {
        .scan_mode = (CerberusScanMode)app->config.scan_mode,
        .sensitivity = app->config.sensitivity,
        .detect_jam = app->config.detect_jam,
        .detect_flood = app->config.detect_flood,
        .detect_replay = app->config.detect_replay,
    };
    cerberus_subghz_set_config(app->worker, &wc);
}

// --- CSV logging to the SD card --------------------------------------------

static void cerberus_csv_append(const CerberusAlertRecord* r) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, CERBERUS_CSV_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        FuriString* line = furi_string_alloc();
        if(storage_file_size(file) == 0) {
            furi_string_set(line, "uptime_s,threat,mhz,rssi_dbm\n");
            storage_file_write(file, furi_string_get_cstr(line), furi_string_size(line));
        }
        furi_string_printf(
            line,
            "%lu,%s,%u,%d\n",
            (unsigned long)r->timestamp_s,
            cerberus_threat_name(r->threat),
            (unsigned)r->freq_mhz,
            (int)r->rssi);
        storage_file_write(file, furi_string_get_cstr(line), furi_string_size(line));
        furi_string_free(line);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void cerberus_reset_stats(Cerberus* app) {
    furi_mutex_acquire(app->log_mutex, FuriWaitForever);
    app->log_count = 0;
    app->log_head = 0;
    app->alert_total = 0;
    memset(app->count_threat, 0, sizeof(app->count_threat));
    memset(app->count_band, 0, sizeof(app->count_band));
    furi_mutex_release(app->log_mutex);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_remove(storage, CERBERUS_CSV_PATH);
    furi_record_close(RECORD_STORAGE);
}

// --- Alert handling (runs on the GUI thread, regardless of active scene) ----

static void cerberus_handle_alert(Cerberus* app, uint32_t event) {
    CerberusThreat threat = CERBERUS_ALERT_EVENT_THREAT(event);
    uint8_t band = CERBERUS_ALERT_EVENT_BAND(event);
    if(threat == CerberusThreatNone || band >= CERBERUS_BAND_COUNT) return;

    CerberusSnapshot snap;
    cerberus_subghz_get_snapshot(app->worker, &snap);

    CerberusAlertRecord record;
    furi_mutex_acquire(app->log_mutex, FuriWaitForever);
    CerberusAlertRecord* r = &app->log[app->log_head];
    r->timestamp_s = (furi_get_tick() - app->start_tick) / 1000;
    r->threat = threat;
    r->band_index = band;
    r->freq_mhz = cerberus_bands[band].mhz;
    r->rssi = snap.band[band].rssi;
    record = *r; // copy for CSV outside the lock
    app->log_head = (uint8_t)((app->log_head + 1) % CERBERUS_LOG_MAX);
    if(app->log_count < CERBERUS_LOG_MAX) app->log_count++;
    app->alert_total++;
    if(threat < 4) app->count_threat[threat]++;
    app->count_band[band]++;
    furi_mutex_release(app->log_mutex);

    // CSV + counters happen whether armed or silent; only the *alarms* are gated.
    if(app->config.log_csv) cerberus_csv_append(&record);

    if(app->armed) {
        notification_message(app->notifications, &sequence_display_backlight_on);
        if(app->config.notify_led)
            notification_message(app->notifications, &sequence_cerberus_led);
        if(app->config.notify_vibro)
            notification_message(app->notifications, &sequence_cerberus_vibro);
        if(app->config.notify_sound)
            notification_message(app->notifications, &sequence_cerberus_alert_tone);
    }
}

// --- ViewDispatcher callbacks ----------------------------------------------

static bool cerberus_custom_event_callback(void* context, uint32_t event) {
    Cerberus* app = context;
    if(event >= CERBERUS_ALERT_EVENT_BASE) {
        cerberus_handle_alert(app, event);
        return true;
    }
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool cerberus_back_event_callback(void* context) {
    Cerberus* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void cerberus_tick_event_callback(void* context) {
    Cerberus* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

// --- Lifecycle --------------------------------------------------------------

static Cerberus* cerberus_app_alloc(void) {
    Cerberus* app = malloc(sizeof(Cerberus));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&cerberus_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, cerberus_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, cerberus_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, cerberus_tick_event_callback, 100);
    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, CerberusViewSubmenu, submenu_get_view(app->submenu));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        CerberusViewVarItemList,
        variable_item_list_get_view(app->var_item_list));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, CerberusViewWidget, widget_get_view(app->widget));

    app->monitor = cerberus_monitor_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        CerberusViewMonitor,
        cerberus_monitor_view_get_view(app->monitor));

    app->log_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->log_count = 0;
    app->log_head = 0;
    app->alert_total = 0;
    app->armed = true;
    memset(app->count_threat, 0, sizeof(app->count_threat));
    memset(app->count_band, 0, sizeof(app->count_band));
    app->start_tick = furi_get_tick();

    for(uint8_t i = 0; i < CerberusSettingCount; i++) {
        app->setting_ctx[i].app = app;
        app->setting_ctx[i].field = i;
    }

    cerberus_config_load(&app->config);

    app->worker = cerberus_subghz_alloc(app->view_dispatcher);
    cerberus_apply_config(app);

    return app;
}

static void cerberus_app_free(Cerberus* app) {
    furi_assert(app);

    cerberus_subghz_free(app->worker);

    view_dispatcher_remove_view(app->view_dispatcher, CerberusViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, CerberusViewVarItemList);
    view_dispatcher_remove_view(app->view_dispatcher, CerberusViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, CerberusViewMonitor);

    submenu_free(app->submenu);
    variable_item_list_free(app->var_item_list);
    widget_free(app->widget);
    cerberus_monitor_view_free(app->monitor);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_mutex_free(app->log_mutex);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t cerberus_app(void* p) {
    UNUSED(p);
    Cerberus* app = cerberus_app_alloc();

    scene_manager_next_scene(app->scene_manager, CerberusSceneStart);
    view_dispatcher_run(app->view_dispatcher);

    cerberus_config_save(&app->config);
    cerberus_app_free(app);
    return 0;
}
