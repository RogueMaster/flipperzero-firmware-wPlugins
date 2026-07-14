#pragma once

#include <gui/view.h>
#include "../helpers/vk_thermo_custom_event.h"
#include "../helpers/vk_thermo_storage.h"

typedef struct VkThermoLogView VkThermoLogView;

typedef void (*VkThermoLogViewCallback)(VkThermoCustomEvent event, void* context);

// Lifecycle
VkThermoLogView* vk_thermo_log_view_alloc(void);
void vk_thermo_log_view_free(VkThermoLogView* instance);
View* vk_thermo_log_view_get_view(VkThermoLogView* instance);

// Callback
void vk_thermo_log_view_set_callback(
    VkThermoLogView* instance,
    VkThermoLogViewCallback callback,
    void* context);

// Data updates
void vk_thermo_log_view_set_log(VkThermoLogView* instance, VkThermoLog* log);
void vk_thermo_log_view_set_temp_unit(VkThermoLogView* instance, uint32_t temp_unit);
