#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <notification/notification_messages.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/dialog_ex.h>
#include "scenes/vk_thermo_scene.h"
#include "views/vk_thermo_scan_view.h"
#include "helpers/vk_thermo_storage.h"
#include "vk_thermo_icons.h"

#define TAG               "VkThermo"
#define VK_THERMO_VERSION "1.0"

// Forward declarations
typedef struct VkThermoNfc VkThermoNfc;
typedef struct VkThermoLogView VkThermoLogView;
typedef struct VkThermoGraphView VkThermoGraphView;

typedef struct {
    Gui* gui;
    NotificationApp* notification;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    // Views
    VkThermoScanView* scan_view;
    VkThermoLogView* log_view;
    VkThermoGraphView* graph_view;
    VariableItemList* variable_item_list; // For settings
    DialogEx* dialog; // For confirmation dialogs

    // NFC
    VkThermoNfc* nfc;

    // Data
    VkThermoLog log;

    // Settings
    uint32_t haptic;
    uint32_t speaker;
    uint32_t led;
    uint32_t temp_unit; // 0 = Celsius, 1 = Fahrenheit
    uint32_t eh_timeout; // EH stabilization timeout index
    uint32_t debug; // Debug diagnostics on/off
} VkThermo;

typedef enum {
    VkThermoViewIdScan,
    VkThermoViewIdLog,
    VkThermoViewIdGraph,
    VkThermoViewIdSettings,
    VkThermoViewIdDialog,
} VkThermoViewId;

typedef enum {
    VkThermoHapticOff,
    VkThermoHapticOn,
} VkThermoHapticState;

typedef enum {
    VkThermoSpeakerOff,
    VkThermoSpeakerOn,
} VkThermoSpeakerState;

typedef enum {
    VkThermoLedOff,
    VkThermoLedOn,
} VkThermoLedState;

typedef enum {
    VkThermoTempUnitCelsius,
    VkThermoTempUnitFahrenheit,
    VkThermoTempUnitKelvin,
} VkThermoTempUnit;

typedef enum {
    VkThermoEhTimeout1s,
    VkThermoEhTimeout2s,
    VkThermoEhTimeout5s,
    VkThermoEhTimeout10s,
    VkThermoEhTimeout30s,
    VkThermoEhTimeoutNone,
    VkThermoEhTimeoutCount,
} VkThermoEhTimeout;

typedef enum {
    VkThermoDebugOff,
    VkThermoDebugOn,
} VkThermoDebugState;
