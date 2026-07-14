#pragma once

#include <gui/view.h>

typedef struct UVMeterWiring UVMeterWiring;
typedef void (*UVMeterWiringEnterSettingsCallback)(void* context);

UVMeterWiring* uv_meter_wiring_alloc(void);
void uv_meter_wiring_free(UVMeterWiring* instance);
View* uv_meter_wiring_get_view(UVMeterWiring* instance);

void uv_meter_wiring_set_enter_settings_callback(
    UVMeterWiring* instance,
    UVMeterWiringEnterSettingsCallback callback,
    void* context);
