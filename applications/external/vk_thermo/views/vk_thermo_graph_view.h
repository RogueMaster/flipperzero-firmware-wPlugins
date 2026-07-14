#pragma once

#include <gui/view.h>
#include "../helpers/vk_thermo_custom_event.h"
#include "../helpers/vk_thermo_storage.h"

typedef struct VkThermoGraphView VkThermoGraphView;

typedef void (*VkThermoGraphViewCallback)(VkThermoCustomEvent event, void* context);

// Lifecycle
VkThermoGraphView* vk_thermo_graph_view_alloc(void);
void vk_thermo_graph_view_free(VkThermoGraphView* instance);
View* vk_thermo_graph_view_get_view(VkThermoGraphView* instance);

// Callback
void vk_thermo_graph_view_set_callback(
    VkThermoGraphView* instance,
    VkThermoGraphViewCallback callback,
    void* context);

// Data updates
void vk_thermo_graph_view_set_log(VkThermoGraphView* instance, VkThermoLog* log);
void vk_thermo_graph_view_set_temp_unit(VkThermoGraphView* instance, uint32_t temp_unit);
void vk_thermo_graph_view_cycle_thermo(VkThermoGraphView* instance, bool forward);
