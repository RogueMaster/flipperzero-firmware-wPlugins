#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <furi_hal_serial.h>
#include <furi_hal_i2c.h>
#include <furi_hal_power.h>

#include "sensors/sensor_types.h"

typedef struct Co2App Co2App;

typedef enum {
    Co2ViewMain,
    Co2ViewSettings,
    Co2ViewCount,
} Co2View;

typedef enum {
    Co2AppCustomEventTick,
    Co2AppCustomEventSettings,
} Co2AppCustomEvent;
