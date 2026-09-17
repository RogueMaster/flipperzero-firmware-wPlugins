#include "../marauder_gui_app_i.h"
#include <stdio.h>

/* "blespam -t <arg>" types, see CommandLine.h's HELP_BT_SPAM_CMD in the ESP32 Marauder source.
   Exposed via the accessor functions below so marauder_gui_scene_bt_menu.c can list them
   without duplicating this table. */
static const struct {
    const char* label_tr;
    const char* label_en;
    const char* arg;
    const char* description_tr;
    const char* description_en;
} marauder_bt_spam_types[] = {
    {"Sour Apple (iOS)",
     "Sour Apple (iOS)",
     "sourapple",
     "iPhone/iPad'lerde surekli sahte cihaz baglanti bildirimleri (AirTag/AirPods taklidi) tetikler.",
     "Triggers continuous fake device-connection notifications (AirTag/AirPods impersonation) on iPhone/iPad."},
    {"Apple Juice (iOS)",
     "Apple Juice (iOS)",
     "applejuice",
     "Sour Apple'a benzer, farkli bir Apple BLE bildirim spam yontemi.",
     "Similar to Sour Apple, a different Apple BLE notification spam method."},
    {"Swiftpair (Windows)",
     "Swiftpair (Windows)",
     "windows",
     "Windows bilgisayarlarda 'Yeni cihaz bulundu' eslesme bildirimlerini spamlar.",
     "Spams 'New device found' pairing notifications on Windows computers."},
    {"Samsung",
     "Samsung",
     "samsung",
     "Samsung cihazlarda sahte Galaxy Buds/aksesuar eslesme bildirimleri gonderir.",
     "Sends fake Galaxy Buds/accessory pairing notifications on Samsung devices."},
    {"Google (Fast Pair)",
     "Google (Fast Pair)",
     "google",
     "Android cihazlarda Google Fast Pair sahte cihaz bildirimleri gonderir.",
     "Sends fake Google Fast Pair device notifications on Android devices."},
    {"Flipper",
     "Flipper",
     "flipper",
     "Yakindaki cihazlara sahte Flipper Zero BLE reklamlari yayinlar.",
     "Broadcasts fake Flipper Zero BLE advertisements to nearby devices."},
    {"Hepsi (All)",
     "All",
     "all",
     "Yukaridaki tum BLE spam turlerini surekli degisen sekilde dondurerek gonderir.",
     "Continuously cycles through and sends all of the above BLE spam types."},
};
#define MARAUDER_BT_SPAM_TYPE_COUNT \
    (sizeof(marauder_bt_spam_types) / sizeof(marauder_bt_spam_types[0]))

size_t marauder_gui_bt_spam_type_count(void) {
    return MARAUDER_BT_SPAM_TYPE_COUNT;
}

const char* marauder_gui_bt_spam_type_label(size_t index) {
    return (index < MARAUDER_BT_SPAM_TYPE_COUNT) ? marauder_bt_spam_types[index].label_tr : "?";
}

const char* marauder_gui_bt_spam_type_label_en(size_t index) {
    return (index < MARAUDER_BT_SPAM_TYPE_COUNT) ? marauder_bt_spam_types[index].label_en : "?";
}

const char* marauder_gui_bt_spam_type_arg(size_t index) {
    return (index < MARAUDER_BT_SPAM_TYPE_COUNT) ? marauder_bt_spam_types[index].arg : "all";
}

const char* marauder_gui_bt_spam_type_description(size_t index) {
    return (index < MARAUDER_BT_SPAM_TYPE_COUNT) ? marauder_bt_spam_types[index].description_tr : "?";
}

const char* marauder_gui_bt_spam_type_description_en(size_t index) {
    return (index < MARAUDER_BT_SPAM_TYPE_COUNT) ? marauder_bt_spam_types[index].description_en : "?";
}

static void marauder_gui_scene_bt_spam_tick(MarauderGuiApp* app) {
    app->attack_spectrum_phase++;
    marauder_gui_attack_view_redraw(app);
}

void marauder_gui_scene_bt_spam_on_enter(void* context) {
    MarauderGuiApp* app = context;

    app->attack_view_title = "BT Spam";
    app->attack_view_target = marauder_gui_text(
        app,
        marauder_gui_bt_spam_type_label(app->bt_spam_type_index),
        marauder_gui_bt_spam_type_label_en(app->bt_spam_type_index));
    app->attack_view_style = MarauderAttackViewStyleRadar;
    app->attack_spectrum_phase = 0;
    app->tick_handler = marauder_gui_scene_bt_spam_tick;

    marauder_gui_attack_view_redraw(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewAttackStatus);

    char cmd[32];
    snprintf(
        cmd, sizeof(cmd), "blespam -t %s", marauder_gui_bt_spam_type_arg(app->bt_spam_type_index));
    marauder_uart_send_line(app->uart, cmd);
}

bool marauder_gui_scene_bt_spam_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if((event.type == SceneManagerEventTypeCustom &&
        event.event == MARAUDER_ATTACK_STOP_CUSTOM_EVENT) ||
       event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_bt_spam_on_exit(void* context) {
    MarauderGuiApp* app = context;

    marauder_uart_send_line(app->uart, "stopscan");

    app->tick_handler = NULL;
}
