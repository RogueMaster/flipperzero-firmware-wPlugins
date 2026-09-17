#include "../marauder_gui_app_i.h"
#include <stdio.h>
#include <string.h>

enum {
    WifiSpamEventStop,
};

/* "attack -t beacon -r/rickroll/funny" - fake-AP beacon spam types that need no target
   selection (unlike "attack -t beacon -a", which spams using the currently selected APs, or
   "-l", which needs an SSID list loaded first via the "ssid" command - both skipped for now).
   See ATTACK_TYPE_BEACON/_RR/_FUNNY handling in the ESP32 Marauder source's CommandLine.cpp.
   Exposed via accessor functions so marauder_gui_scene_wifi_spam_menu.c can list them without
   duplicating this table. */
static const struct {
    const char* label_tr;
    const char* label_en;
    const char* command;
    const char* description_tr;
    const char* description_en;
} marauder_wifi_spam_types[] = {
    {"Rastgele SSID",
     "Random SSID",
     "attack -t beacon -r",
     "Surekli farkli, rastgele isimli sahte WiFi aglari yayinlar. Sayi sabit degil, sinirsiz cesitlilik.",
     "Continuously broadcasts fake WiFi networks with different random names. Not a fixed count - endless variety."},
    {"Rick Roll",
     "Rick Roll",
     "attack -t rickroll",
     "Rick Astley sarki sozlerinden olusan komik sahte agler yayinlar.",
     "Broadcasts funny fake networks made of Rick Astley song lyrics."},
    {"Komik Isimler",
     "Funny Names",
     "attack -t funny",
     "Onceden tanimli esprili SSID isimleriyle sahte agler yayinlar.",
     "Broadcasts fake networks with pre-defined joke SSID names."},
};
#define MARAUDER_WIFI_SPAM_TYPE_COUNT \
    (sizeof(marauder_wifi_spam_types) / sizeof(marauder_wifi_spam_types[0]))

size_t marauder_gui_wifi_spam_type_count(void) {
    return MARAUDER_WIFI_SPAM_TYPE_COUNT;
}

const char* marauder_gui_wifi_spam_type_label(size_t index) {
    return (index < MARAUDER_WIFI_SPAM_TYPE_COUNT) ? marauder_wifi_spam_types[index].label_tr : "?";
}

const char* marauder_gui_wifi_spam_type_label_en(size_t index) {
    return (index < MARAUDER_WIFI_SPAM_TYPE_COUNT) ? marauder_wifi_spam_types[index].label_en : "?";
}

const char* marauder_gui_wifi_spam_type_command(size_t index) {
    return (index < MARAUDER_WIFI_SPAM_TYPE_COUNT) ? marauder_wifi_spam_types[index].command :
                                                       "attack -t beacon -r";
}

const char* marauder_gui_wifi_spam_type_description(size_t index) {
    return (index < MARAUDER_WIFI_SPAM_TYPE_COUNT) ?
               marauder_wifi_spam_types[index].description_tr :
               "?";
}

const char* marauder_gui_wifi_spam_type_description_en(size_t index) {
    return (index < MARAUDER_WIFI_SPAM_TYPE_COUNT) ?
               marauder_wifi_spam_types[index].description_en :
               "?";
}

static void marauder_gui_scene_wifi_spam_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    MarauderGuiApp* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, WifiSpamEventStop);
    }
}

static void marauder_gui_scene_wifi_spam_redraw(MarauderGuiApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "WiFi Spam");
    widget_add_string_element(
        app->widget, 64, 14, AlignCenter, AlignTop, FontSecondary, app->wifi_spam_status_label);
    widget_add_text_box_element(
        app->widget, 0, 26, 128, 22, AlignCenter, AlignTop, app->last_uart_line, true);
    widget_add_button_element(
        app->widget,
        GuiButtonTypeCenter,
        marauder_gui_text(app, "Durdur", "Stop"),
        marauder_gui_scene_wifi_spam_button_callback,
        app);
}

static void marauder_gui_scene_wifi_spam_uart_line(MarauderGuiApp* app, const char* line) {
    strncpy(app->last_uart_line, line, MARAUDER_LINE_MAX - 1);
    app->last_uart_line[MARAUDER_LINE_MAX - 1] = '\0';
    marauder_gui_scene_wifi_spam_redraw(app);
}

void marauder_gui_scene_wifi_spam_on_enter(void* context) {
    MarauderGuiApp* app = context;

    app->last_uart_line[0] = '\0';
    app->uart_line_handler = marauder_gui_scene_wifi_spam_uart_line;

    if(app->wifi_spam_use_count) {
        snprintf(
            app->wifi_spam_status_label,
            sizeof(app->wifi_spam_status_label),
            marauder_gui_text(app, "%d Sahte AG", "%d Fake AP"),
            app->wifi_spam_count);
    } else {
        strncpy(
            app->wifi_spam_status_label,
            marauder_gui_text(
                app,
                marauder_gui_wifi_spam_type_label(app->wifi_spam_type_index),
                marauder_gui_wifi_spam_type_label_en(app->wifi_spam_type_index)),
            sizeof(app->wifi_spam_status_label) - 1);
        app->wifi_spam_status_label[sizeof(app->wifi_spam_status_label) - 1] = '\0';
    }

    marauder_gui_scene_wifi_spam_redraw(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);

    if(app->wifi_spam_use_count) {
        /* Fixed-size fake AP list: clear any stale list, generate N random SSIDs, then spam
           from that list ("attack -t beacon -l") instead of the endless "-r" random stream. */
        char cmd[32];
        marauder_uart_send_line(app->uart, "clearlist -s");
        snprintf(cmd, sizeof(cmd), "ssid -a -g %d", app->wifi_spam_count);
        marauder_uart_send_line(app->uart, cmd);
        marauder_uart_send_line(app->uart, "attack -t beacon -l");
    } else {
        marauder_uart_send_line(app->uart, marauder_gui_wifi_spam_type_command(app->wifi_spam_type_index));
    }
}

bool marauder_gui_scene_wifi_spam_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if((event.type == SceneManagerEventTypeCustom && event.event == WifiSpamEventStop) ||
       event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_wifi_spam_on_exit(void* context) {
    MarauderGuiApp* app = context;

    marauder_uart_send_line(app->uart, "stopscan");

    app->uart_line_handler = NULL;
    widget_reset(app->widget);
}
