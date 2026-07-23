#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "ghosttag_icons.h" // generated from icons/ by fbt

#include "helpers/ble_signatures.h"
#include "helpers/tracker_db.h"
#include "helpers/uart_link.h"
#include "views/radar_view.h"
#include "views/device_list_view.h"
#include "views/device_detail_view.h"
#include "views/alert_view.h"
#include "scenes/ghosttag_scene.h"

#define GHOSTTAG_VERSION "1.0"

typedef enum {
    GhostTagViewSubmenu,
    GhostTagViewRadar,
    GhostTagViewDeviceList,
    GhostTagViewDeviceDetail,
    GhostTagViewAlert,
    GhostTagViewSettings,
    GhostTagViewAbout,
} GhostTagViewId;

typedef enum {
    GhostTagCustomEventScanOpenList = 100,
    GhostTagCustomEventNewFollower,
    GhostTagCustomEventOpenDetail,
} GhostTagCustomEvent;

typedef struct {
    uint8_t sensitivity_index; // 0 Near .. 2 Far (RSSI cutoff)
    uint8_t follow_index; // dwell time before "following" flag
    bool sound;
    bool vibro;
    bool led;
} GhostTagSettings;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    Submenu* submenu;
    VariableItemList* var_item_list;
    Widget* widget;

    RadarView* radar_view;
    DeviceListView* device_list_view;
    DeviceDetailView* device_detail_view;
    AlertView* alert_view;

    TrackerDb* db;
    UartLink* uart;

    GhostTagSettings settings;

    TrackerRecord selected_record; // for the detail view
    volatile uint32_t last_rx_tick; // last byte received from ESP32
    bool esp_connected;
    char esp_version[16];
} GhostTagApp;

/* settings.c helpers */
int8_t ghosttag_settings_rssi_cutoff(const GhostTagSettings* s);
uint32_t ghosttag_settings_follow_ms(const GhostTagSettings* s);
const char* ghosttag_settings_sensitivity_label(uint8_t index);
const char* ghosttag_settings_follow_label(uint8_t index);

/* fired on the GUI thread to play the configured alert feedback */
void ghosttag_notify_alert(GhostTagApp* app);
