#pragma once

#include <stdint.h>

typedef enum {
    // Scan view events
    VkThermoCustomEventScanUp,
    VkThermoCustomEventScanDown,
    VkThermoCustomEventScanLeft,
    VkThermoCustomEventScanRight,
    VkThermoCustomEventScanOk,
    VkThermoCustomEventScanBack,

    // NFC events
    VkThermoCustomEventNfcReadSuccess,
    VkThermoCustomEventNfcReadError,
    VkThermoCustomEventNfcTagDetected,
    VkThermoCustomEventNfcTagLost,

    // Log view events
    VkThermoCustomEventLogUp,
    VkThermoCustomEventLogDown,
    VkThermoCustomEventLogLeft,
    VkThermoCustomEventLogRight,
    VkThermoCustomEventLogOk,
    VkThermoCustomEventLogBack,
    VkThermoCustomEventLogClear,
    VkThermoCustomEventLogGraph,
    VkThermoCustomEventLogSettings,

    // Graph view events
    VkThermoCustomEventGraphUp,
    VkThermoCustomEventGraphDown,
    VkThermoCustomEventGraphLeft,
    VkThermoCustomEventGraphRight,
    VkThermoCustomEventGraphOk,
    VkThermoCustomEventGraphBack,

    // Settings events
    VkThermoCustomEventSettingsBack,

    // Dialog events
    VkThermoCustomEventDialogConfirm,
    VkThermoCustomEventDialogCancel,
} VkThermoCustomEvent;
