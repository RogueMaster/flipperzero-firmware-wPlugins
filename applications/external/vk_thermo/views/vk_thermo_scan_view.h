#pragma once

#include <gui/view.h>
#include "../helpers/vk_thermo_custom_event.h"

typedef struct VkThermoScanView VkThermoScanView;

typedef void (*VkThermoScanViewCallback)(VkThermoCustomEvent event, void* context);

typedef enum {
    VkThermoScanStateScanning,
    VkThermoScanStateReading,
    VkThermoScanStateSuccess,
    VkThermoScanStateError,
} VkThermoScanState;

// Lifecycle
VkThermoScanView* vk_thermo_scan_view_alloc(void);
void vk_thermo_scan_view_free(VkThermoScanView* instance);
View* vk_thermo_scan_view_get_view(VkThermoScanView* instance);

// Callback
void vk_thermo_scan_view_set_callback(
    VkThermoScanView* instance,
    VkThermoScanViewCallback callback,
    void* context);

// State updates
void vk_thermo_scan_view_set_state(VkThermoScanView* instance, VkThermoScanState state);
void vk_thermo_scan_view_set_temperature(
    VkThermoScanView* instance,
    float celsius,
    const uint8_t* uid);
void vk_thermo_scan_view_set_temp_unit(VkThermoScanView* instance, uint32_t temp_unit);

// Animation tick (call from scene tick handler)
void vk_thermo_scan_view_tick(VkThermoScanView* instance);
