#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "argus_icons.h" // generated from icons/ by fbt

#include "helpers/argus_db.h"
#include "helpers/uart_link.h"
#include "views/monitor_view.h"
#include "views/ap_list_view.h"
#include "views/threat_log_view.h"
#include "scenes/argus_scene.h"

#define ARGUS_VERSION         "1.0"
#define ARGUS_ATTACK_HOLD_MS  4000u // keep the ALARM latched this long after the last deauth
#define ARGUS_GUARD_INPUT_LEN ARGUS_SSID_MAX

typedef enum {
    ArgusViewSubmenu,
    ArgusViewMonitor,
    ArgusViewApList,
    ArgusViewThreatLog,
    ArgusViewTextInput,
    ArgusViewSettings,
    ArgusViewAbout,
} ArgusViewId;

typedef enum {
    ArgusCustomEventTwinFound = 100,
    ArgusCustomEventStorm,
    ArgusCustomEventOpenLog,
} ArgusCustomEvent;

typedef struct {
    uint8_t channel_index; // 0 = Hop all, 1..13 = lock to that channel
    uint8_t sensitivity_index; // 0 High, 1 Medium, 2 Low (deauth storm threshold)
    bool sound;
    bool vibro;
    bool led;
} ArgusSettings;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    Submenu* submenu;
    VariableItemList* var_item_list;
    TextInput* text_input;
    Widget* widget;

    MonitorView* monitor_view;
    ApListView* ap_list_view;
    ThreatLogView* threat_log_view;

    ArgusDb* db;
    UartLink* uart;

    ArgusSettings settings;

    char guard_input[ARGUS_GUARD_INPUT_LEN]; // text-input scratch buffer

    volatile uint32_t last_rx_tick; // last byte/frame received from ESP32
    bool esp_connected;
    char esp_version[16];

    bool attack_active; // edge tracking for the alarm
} ArgusApp;

/* settings.c helpers */
uint8_t argus_settings_channel(const ArgusSettings* s);
const char* argus_settings_channel_label(uint8_t index);
uint32_t argus_settings_storm_threshold(const ArgusSettings* s);
const char* argus_settings_sensitivity_label(uint8_t index);

/* argus.c helpers */
void argus_notify_attack(ArgusApp* app); // deauth storm
void argus_notify_twin(ArgusApp* app); // rogue evil twin
void argus_link_arm(ArgusApp* app); // start link + push CHAN / GUARD config
void argus_link_disarm(ArgusApp* app); // STOP + tear the link down
bool argus_is_under_attack(ArgusApp* app, const ArgusStats* stats);
