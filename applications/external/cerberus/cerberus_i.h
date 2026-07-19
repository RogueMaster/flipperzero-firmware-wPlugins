/**
 * Cerberus - shared application state.
 *
 * Holds the GUI plumbing (view dispatcher + scene manager), the radio worker,
 * the user config, and the rolling alert log that the Alerts screen renders.
 */
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

#include "scenes/cerberus_scene.h"
#include "views/cerberus_monitor_view.h"
#include "helpers/cerberus_subghz.h"
#include "helpers/cerberus_detector.h"

#define CERBERUS_VERSION  "1.1"
#define CERBERUS_GITHUB   "github.com/at0m-b0mb/flipper-cerberus"
#define CERBERUS_LOG_MAX  32

// Settings rows - the order here must match the order they're added in the
// settings scene, since each row carries a context tagged with its field id.
typedef enum {
    CerberusSettingScanMode = 0,
    CerberusSettingSensitivity,
    CerberusSettingDetectJam,
    CerberusSettingDetectFlood,
    CerberusSettingDetectReplay,
    CerberusSettingNotifySound,
    CerberusSettingNotifyVibro,
    CerberusSettingNotifyLed,
    CerberusSettingLogCsv,
    CerberusSettingKeepScreen,
    CerberusSettingCount,
} CerberusSettingField;

// Full persisted configuration.
typedef struct {
    uint8_t scan_mode; // CerberusScanMode
    uint8_t sensitivity; // 0 low / 1 medium / 2 high
    bool detect_jam;
    bool detect_flood;
    bool detect_replay;
    bool notify_sound;
    bool notify_vibro;
    bool notify_led;
    bool log_csv; // append alerts to a CSV on the SD card
    bool keep_screen_on; // hold the backlight on while watching
} CerberusConfig;

// One logged alert.
typedef struct {
    uint32_t timestamp_s; // seconds since the app launched
    CerberusThreat threat;
    uint8_t band_index;
    uint16_t freq_mhz;
    int16_t rssi;
} CerberusAlertRecord;

// Tagged context handed to each settings row's change callback.
typedef struct Cerberus Cerberus;
typedef struct {
    Cerberus* app;
    uint8_t field;
} CerberusSettingCtx;

// View ids registered with the dispatcher.
typedef enum {
    CerberusViewSubmenu,
    CerberusViewMonitor,
    CerberusViewVarItemList,
    CerberusViewWidget,
} CerberusViewId;

// Scene-level custom events (must stay below CERBERUS_ALERT_EVENT_BASE; alert
// events at/above that base are intercepted by the app, not the scenes).
typedef enum {
    CerberusEventStartMonitor = 0,
    CerberusEventStartAlerts,
    CerberusEventStartSettings,
    CerberusEventStartAbout,
} CerberusSceneEvent;

struct Cerberus {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    Submenu* submenu;
    VariableItemList* var_item_list;
    Widget* widget;
    CerberusMonitorView* monitor;

    CerberusSubGhz* worker;
    CerberusConfig config;
    CerberusSettingCtx setting_ctx[CerberusSettingCount];

    bool armed; // false = silent (still detects + logs, no LED/buzz/tone)

    FuriMutex* log_mutex;
    CerberusAlertRecord log[CERBERUS_LOG_MAX];
    uint8_t log_count; // valid entries (capped at CERBERUS_LOG_MAX)
    uint8_t log_head; // next write slot (ring buffer)
    uint32_t alert_total; // lifetime alert counter
    uint32_t count_threat[4]; // alerts per CerberusThreat
    uint32_t count_band[CERBERUS_BAND_COUNT]; // alerts per band
    uint32_t start_tick; // app launch tick, for timestamps
};

/** Reset the alert log, counters and the on-SD CSV (used by the Stats screen). */
void cerberus_reset_stats(Cerberus* app);

/** Build the worker-facing config slice from the full user config. */
void cerberus_apply_config(Cerberus* app);
