// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "../recon_app_i.h"

static const char* const backend_text[] = {"Companion", "Marauder"};
static const char* const port_text[] = {"USART 13/14", "LPUART 15/16"};
static const char* const onoff_text[] = {"OFF", "ON"};

static const uint32_t esp_baud_val[] = {115200, 921600};
static const char* const esp_baud_text[] = {"115200", "921600"};
static const uint32_t gps_baud_val[] = {9600, 115200, 57600};
static const char* const gps_baud_text[] = {"9600", "115200", "57600"};

// Index-aligned with ESP_MARAUDER_CMDS in helpers/esp_link.c.
#define MARAUDER_CMD_COUNT 4
static const char* const marauder_text[] = {"Probe req", "AP scan", "Beacon", "Raw"};

static uint8_t index_of_u32(const uint32_t* arr, size_t n, uint32_t val) {
    for(size_t i = 0; i < n; i++) {
        if(arr[i] == val) return (uint8_t)i;
    }
    return 0;
}

static void backend_changed(VariableItem* item) {
    ReconApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.backend = (idx == 1) ? EspBackendGeneric : EspBackendCompanion;
    variable_item_set_current_value_text(item, backend_text[idx]);
    recon_settings_save(app);
}

static void esp_port_changed(VariableItem* item) {
    ReconApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.esp_uart = (idx == 1) ? FuriHalSerialIdLpuart : FuriHalSerialIdUsart;
    variable_item_set_current_value_text(item, port_text[idx]);
    recon_settings_save(app);
}

static void esp_baud_changed(VariableItem* item) {
    ReconApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.esp_baud = esp_baud_val[idx];
    variable_item_set_current_value_text(item, esp_baud_text[idx]);
    recon_settings_save(app);
}

static void marauder_cmd_changed(VariableItem* item) {
    ReconApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.marauder_cmd = idx;
    variable_item_set_current_value_text(item, marauder_text[idx]);
    recon_settings_save(app);
}

static void gps_enabled_changed(VariableItem* item) {
    ReconApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.gps_enabled = (idx == 1);
    variable_item_set_current_value_text(item, onoff_text[idx]);
    recon_settings_save(app);
}

// Where NMEA comes from. "ESP32" is for carrier boards whose GPS module is wired
// to the ESP rather than to the Flipper's header -- the Flipper cannot see those
// on any pin, so the companion relays each sentence instead (issue #5).
static const char* const gps_source_text[] = {"Flipper", "ESP32"};

static void gps_source_changed(VariableItem* item) {
    ReconApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.gps_source = idx;
    variable_item_set_current_value_text(item, gps_source_text[idx]);
    recon_settings_save(app);
}

// ESP-side GPS RX pin for the relay. There is no standard pin, so this is a
// short list of the ones ESP32 carrier boards actually use rather than a free
// numeric entry (a VariableItem cannot do arbitrary numbers, and a wrong pin is
// indistinguishable from a dead receiver). 1 and 3 are excluded: they carry the
// UART0 link to the Flipper, and taking them would cut the board off entirely.
static const uint8_t esp_gps_pin_vals[] = {4, 5, 12, 13, 16, 17, 32, 33, 34, 35};
static const char* const esp_gps_pin_text[] =
    {"4", "5", "12", "13", "16", "17", "32", "33", "34", "35"};
#define ESP_GPS_PIN_COUNT (sizeof(esp_gps_pin_vals) / sizeof(esp_gps_pin_vals[0]))

static void esp_gps_pin_changed(VariableItem* item) {
    ReconApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= ESP_GPS_PIN_COUNT) idx = 0;
    app->settings.esp_gps_pin = esp_gps_pin_vals[idx];
    variable_item_set_current_value_text(item, esp_gps_pin_text[idx]);
    recon_settings_save(app);
}

static void gps_port_changed(VariableItem* item) {
    ReconApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.gps_uart = (idx == 1) ? FuriHalSerialIdLpuart : FuriHalSerialIdUsart;
    variable_item_set_current_value_text(item, port_text[idx]);
    recon_settings_save(app);
}

static void gps_baud_changed(VariableItem* item) {
    ReconApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.gps_baud = gps_baud_val[idx];
    variable_item_set_current_value_text(item, gps_baud_text[idx]);
    recon_settings_save(app);
}

// Index-aligned with ReconAlertMode in helpers/alerts.h.
static const char* const alert_text[] = {"OFF", "Vibrate", "Beep", "Beep+Vibe"};

static void alert_mode_changed(VariableItem* item) {
    ReconApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.alert_mode = idx;
    variable_item_set_current_value_text(item, alert_text[idx]);
    recon_settings_save(app);
}

// Index-aligned with AlertConfChoice in helpers/detect_rules.h.
//
// Kept to 7 characters: VariableItemList clips the value column past that, and
// "Confirmed" rendered on hardware as "Confirme" (the same way "Companion"
// shows as "ompanio" on Board Mode). "Any" rather than "Possible+" because the
// rung it admits is the OUI-only lead -- "alert me about everything" is what
// the operator is actually choosing, and it fits.
static const char* const alert_conf_text[] = {"Any", "Likely", "Confirm"};

static void alert_min_conf_changed(VariableItem* item) {
    ReconApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.alert_min_conf = idx;
    variable_item_set_current_value_text(item, alert_conf_text[idx]);
    recon_settings_save(app);
}

static void sound_changed(VariableItem* item) {
    ReconApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.sound = (idx == 1);
    variable_item_set_current_value_text(item, onoff_text[idx]);
    recon_settings_save(app);
}

static const char* const flash_speed_text[] = {"Safe 115k", "Fast 921k"};

static void flash_fast_changed(VariableItem* item) {
    ReconApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.flash_fast = (idx == 1);
    variable_item_set_current_value_text(item, flash_speed_text[idx]);
    recon_settings_save(app);
}

static void save_hits_changed(VariableItem* item) {
    ReconApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.save_hits = (idx == 1);
    variable_item_set_current_value_text(item, onoff_text[idx]);
    recon_settings_save(app);
    // Turning it OFF erases the trail rather than just stopping new writes: a
    // privacy toggle that leaves the old file sitting on the card is worse than
    // no toggle, because it reads as "off" while the record is still there.
    if(!app->settings.save_hits) recon_hits_clear(app);
}

static void log_serials_changed(VariableItem* item) {
    ReconApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.log_serials = (idx == 1);
    variable_item_set_current_value_text(item, onoff_text[idx]);
    recon_settings_save(app);
}

static void anomaly_flag_changed(VariableItem* item) {
    ReconApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.anomaly_flag = (idx == 1);
    variable_item_set_current_value_text(item, onoff_text[idx]);
    recon_settings_save(app);
}

void recon_scene_settings_on_enter(void* context) {
    ReconApp* app = context;
    VariableItemList* list = app->var_item_list;
    variable_item_list_reset(list);
    VariableItem* item;
    uint8_t idx;

    idx = (app->settings.backend == EspBackendGeneric) ? 1 : 0;
    item = variable_item_list_add(list, "Board Mode", 2, backend_changed, app);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, backend_text[idx]);

    idx = (app->settings.esp_uart == FuriHalSerialIdLpuart) ? 1 : 0;
    item = variable_item_list_add(list, "ESP Port", 2, esp_port_changed, app);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, port_text[idx]);

    idx = index_of_u32(esp_baud_val, COUNT_OF(esp_baud_val), app->settings.esp_baud);
    item = variable_item_list_add(list, "ESP Baud", COUNT_OF(esp_baud_val), esp_baud_changed, app);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, esp_baud_text[idx]);

    idx = (app->settings.marauder_cmd < MARAUDER_CMD_COUNT) ? app->settings.marauder_cmd : 0;
    item = variable_item_list_add(
        list, "Marauder Cmd", MARAUDER_CMD_COUNT, marauder_cmd_changed, app);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, marauder_text[idx]);

    idx = app->settings.gps_enabled ? 1 : 0;
    item = variable_item_list_add(list, "GPS", 2, gps_enabled_changed, app);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, onoff_text[idx]);

    // Out-of-range falls back to Flipper, the wiring the docs and the pin table
    // describe -- never silently to the relay, which needs companion support.
    idx = (app->settings.gps_source < ReconGpsSourceCount) ? app->settings.gps_source :
                                                             ReconGpsSourceFlipper;
    item = variable_item_list_add(list, "GPS From", ReconGpsSourceCount, gps_source_changed, app);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, gps_source_text[idx]);

    // Which ESP pin the relay listens on. Only meaningful with GPS From = ESP32,
    // but shown unconditionally: hiding items as other settings change makes the
    // list jump around under the cursor.
    idx = 0;
    for(uint8_t i = 0; i < ESP_GPS_PIN_COUNT; i++) {
        if(esp_gps_pin_vals[i] == app->settings.esp_gps_pin) {
            idx = i;
            break;
        }
    }
    item =
        variable_item_list_add(list, "ESP GPS Pin", ESP_GPS_PIN_COUNT, esp_gps_pin_changed, app);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, esp_gps_pin_text[idx]);

    idx = (app->settings.gps_uart == FuriHalSerialIdLpuart) ? 1 : 0;
    item = variable_item_list_add(list, "GPS Port", 2, gps_port_changed, app);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, port_text[idx]);

    idx = index_of_u32(gps_baud_val, COUNT_OF(gps_baud_val), app->settings.gps_baud);
    item = variable_item_list_add(list, "GPS Baud", COUNT_OF(gps_baud_val), gps_baud_changed, app);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, gps_baud_text[idx]);

    // Announce a new Flock/ALPR hit so it isn't missed while the screen is out of
    // sight. Fires once per device on its first Likely-or-better sighting.
    idx = (app->settings.alert_mode < ReconAlertModeCount) ? app->settings.alert_mode : 0;
    item =
        variable_item_list_add(list, "Alert on hit", ReconAlertModeCount, alert_mode_changed, app);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, alert_text[idx]);

    // Lowest confidence rung that may alert. "Likely+" is the shipped default;
    // "Possible+" is an opt-in that trades precision for recall (an OUI-prefix
    // lead will buzz at unrelated hardware) and is the only way to be told about
    // a deployment that never scores higher -- GitHub issue #5.
    // Out-of-range falls back to Likely, NOT index 0: 0 is the loosest rung, and a
    // corrupt settings file must never present (or select) the noisy mode. Matches
    // flock_alert_min_conf_rung()'s own fallback.
    idx = (app->settings.alert_min_conf < AlertConfCount) ? app->settings.alert_min_conf :
                                                            AlertConfLikely;
    item =
        variable_item_list_add(list, "Alert level", AlertConfCount, alert_min_conf_changed, app);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, alert_conf_text[idx]);

    idx = app->settings.sound ? 1 : 0;
    item = variable_item_list_add(list, "Sound", 2, sound_changed, app);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, onoff_text[idx]);

    idx = app->settings.flash_fast ? 1 : 0;
    item = variable_item_list_add(list, "Flash Speed", 2, flash_fast_changed, app);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, flash_speed_text[idx]);

    // Persist detections across app restarts. OFF by default -- a hit log is a
    // durable record of where you have been. Turning it off deletes hits.csv.
    idx = app->settings.save_hits ? 1 : 0;
    item = variable_item_list_add(list, "Save hits", 2, save_hits_changed, app);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, onoff_text[idx]);

    idx = app->settings.log_serials ? 1 : 0;
    item = variable_item_list_add(list, "Log Flock serials", 2, log_serials_changed, app);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, onoff_text[idx]);

    // Net Guardian: flag unidentified strong/persistent BLE devices as suspicious.
    // Off by default -- it trades a higher false-positive rate for more coverage.
    idx = app->settings.anomaly_flag ? 1 : 0;
    item = variable_item_list_add(list, "Anomaly flag", 2, anomaly_flag_changed, app);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, onoff_text[idx]);

    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewVarItemList);
}

bool recon_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void recon_scene_settings_on_exit(void* context) {
    ReconApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
