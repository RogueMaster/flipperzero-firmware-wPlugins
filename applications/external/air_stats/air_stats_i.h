#pragma once

/*
 * air_stats_i.h — internal header for Air Stats FAP.
 *
 * App struct and extern App* app are defined in sensors/unitemp/unitemp.h
 * (so sensor drivers can include that header without depending on this file).
 * Here we include the full Sensors.h (which brings in unitemp.h transitively)
 * and add app-specific declarations.
 */

/* Bring in App struct + settings enums + extern app + all sensor types */
#include "sensors/unitemp/Sensors.h"

/* MH-Z19C sensor types: PWM and UART */
#include "sensors/mhz19c_sensor.h"
#include "sensors/mhz19c_uart_sensor.h"

/* View declarations — implemented in views/ */
#include "views/views.h"

/* Notifications: LED + sound alerts */
#include "helpers/notifications.h"

/* Backlight */
void air_stats_apply_backlight(void);
void air_stats_backlight_activity(void);

/* Entry point */
int32_t air_stats_main(void* p);
