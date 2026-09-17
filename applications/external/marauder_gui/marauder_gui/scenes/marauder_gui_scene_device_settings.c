#include "../marauder_gui_app_i.h"
#include <stdio.h>
#include <string.h>

/* Marauder only exposes bool settings for toggling from the CLI ("settings -s <name>
   enable/disable" - see CommandLine.cpp's SETTINGS_CMD handler; ClientSSID/ClientPW and the
   WiGLE/WDGWars fields are String-typed and have no CLI setter, so they're left out here.
   Names/order confirmed against settings.cpp's createDefaultSettings(). The setting names
   themselves (ForcePMKID, Channel Hop, ...) are left untranslated in both languages - they're
   Marauder's own technical/CLI terms, already in English even in the Turkish UI. */
typedef struct {
    const char* key;
    const char* label;
    const char* description_tr;
    const char* description_en;
} MarauderDeviceSettingDef;

static const MarauderDeviceSettingDef marauder_device_settings[] = {
    {"ForcePMKID",
     "Force PMKID",
     "WPA2 aglarda PMKID yakalamayi zorunlu kilar. SD kart gerektiren pcap kaydiyla ilgilidir.",
     "Forces PMKID capture on WPA2 networks. Relates to pcap logging, which needs an SD card."},
    {"ForceProbe",
     "Force Probe",
     "Baglantisiz cihazlarin prob isteklerini daha agresif isler/tetikler.",
     "Processes/triggers disconnected devices' probe requests more aggressively."},
    {"SavePCAP",
     "Save Pcap",
     "Taramalari SD karta .pcap olarak kaydeder. Bu cihazda SD kart yok, etkisi olmaz.",
     "Saves scans to SD card as .pcap. This device has no SD card, so it has no effect."},
    {"EnableLED",
     "Enable LED",
     "ESP32 uzerindeki durum LED'ini ac/kapa.",
     "Toggles the status LED on the ESP32."},
    {"EPDeauth",
     "EP Deauth",
     "Evil Portal/Karma calisirken hedef agin gercek istemcilerine otomatik deauth gonderir.",
     "Automatically deauths the target network's real clients while Evil Portal/Karma is running."},
    {"ChanHop",
     "Channel Hop",
     "Tarama sirasinda WiFi kanallari arasinda otomatik gecis yapar.",
     "Automatically switches between WiFi channels during a scan."},
};

#define MARAUDER_DEVICE_SETTINGS_COUNT \
    (sizeof(marauder_device_settings) / sizeof(marauder_device_settings[0]))

static char marauder_device_settings_labels_tr[MARAUDER_DEVICE_SETTINGS_COUNT][40];
static char marauder_device_settings_labels_en[MARAUDER_DEVICE_SETTINGS_COUNT][40];
static MarauderMenuItem marauder_device_settings_items[MARAUDER_DEVICE_SETTINGS_COUNT];
static bool marauder_device_settings_values[MARAUDER_DEVICE_SETTINGS_COUNT];
static bool marauder_device_settings_known[MARAUDER_DEVICE_SETTINGS_COUNT];
/* Index the last-seen "Name: X" line matched, awaiting its "Value: Y" a couple lines later
   (same triple-line parse trick as wifi_join_status.c's ClientSSID lookup) - -1 when the most
   recent Name line didn't match anything we track (ClientSSID, wu, wt, ...). */
static int marauder_device_settings_pending_index;

static void marauder_device_settings_relabel(size_t index) {
    const char* state_tr;
    const char* state_en;
    if(!marauder_device_settings_known[index]) {
        state_tr = "?";
        state_en = "?";
    } else if(marauder_device_settings_values[index]) {
        state_tr = "Acik";
        state_en = "On";
    } else {
        state_tr = "Kapali";
        state_en = "Off";
    }
    snprintf(
        marauder_device_settings_labels_tr[index],
        sizeof(marauder_device_settings_labels_tr[index]),
        "%s: %s",
        marauder_device_settings[index].label,
        state_tr);
    snprintf(
        marauder_device_settings_labels_en[index],
        sizeof(marauder_device_settings_labels_en[index]),
        "%s: %s",
        marauder_device_settings[index].label,
        state_en);
}

static void marauder_gui_scene_device_settings_uart_line(MarauderGuiApp* app, const char* line) {
    if(strncmp(line, "Name: ", 6) == 0) {
        marauder_device_settings_pending_index = -1;
        for(size_t i = 0; i < MARAUDER_DEVICE_SETTINGS_COUNT; i++) {
            if(strcmp(line + 6, marauder_device_settings[i].key) == 0) {
                marauder_device_settings_pending_index = (int)i;
                break;
            }
        }
    } else if(marauder_device_settings_pending_index >= 0 && strncmp(line, "Value: ", 7) == 0) {
        size_t idx = (size_t)marauder_device_settings_pending_index;
        marauder_device_settings_values[idx] = (strcmp(line + 7, "true") == 0);
        marauder_device_settings_known[idx] = true;
        marauder_device_settings_pending_index = -1;
        marauder_device_settings_relabel(idx);
        marauder_gui_menu_redraw(app);
    }
}

void marauder_gui_scene_device_settings_on_enter(void* context) {
    MarauderGuiApp* app = context;

    marauder_device_settings_pending_index = -1;
    for(size_t i = 0; i < MARAUDER_DEVICE_SETTINGS_COUNT; i++) {
        marauder_device_settings_known[i] = false;
        marauder_device_settings_relabel(i);
        marauder_device_settings_items[i].label_tr = marauder_device_settings_labels_tr[i];
        marauder_device_settings_items[i].label_en = marauder_device_settings_labels_en[i];
        marauder_device_settings_items[i].description_tr = marauder_device_settings[i].description_tr;
        marauder_device_settings_items[i].description_en = marauder_device_settings[i].description_en;
    }

    marauder_gui_menu_set_items(
        app,
        marauder_device_settings_items,
        MARAUDER_DEVICE_SETTINGS_COUNT,
        marauder_gui_text(app, "Ayarlar", "Settings"));

    app->uart_line_handler = marauder_gui_scene_device_settings_uart_line;
    marauder_uart_send_line(app->uart, "settings");
}

bool marauder_gui_scene_device_settings_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event < MARAUDER_DEVICE_SETTINGS_COUNT) {
            size_t idx = event.event;
            /* Current state has to be known before toggling, or we'd be guessing which
               direction to send - ignore the press if the "settings" reply hasn't landed yet. */
            if(marauder_device_settings_known[idx]) {
                bool new_value = !marauder_device_settings_values[idx];
                char cmd[64];
                snprintf(
                    cmd,
                    sizeof(cmd),
                    "settings -s %s %s",
                    marauder_device_settings[idx].key,
                    new_value ? "enable" : "disable");
                marauder_uart_send_line(app->uart, cmd);

                /* "settings -s ... enable/disable" prints nothing on success (only an error
                   line on failure), so reflect the change optimistically rather than wait for a
                   confirmation that never comes. */
                marauder_device_settings_values[idx] = new_value;
                marauder_device_settings_relabel(idx);
                marauder_gui_menu_redraw(app);
            }
            consumed = true;
        }
    }

    return consumed;
}

void marauder_gui_scene_device_settings_on_exit(void* context) {
    MarauderGuiApp* app = context;
    app->uart_line_handler = NULL;
    app->tick_handler = NULL;
}
