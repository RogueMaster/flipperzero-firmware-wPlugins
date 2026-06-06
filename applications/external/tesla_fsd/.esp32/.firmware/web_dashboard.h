#pragma once

#include "fsd_handler.h"
#include "can_driver.h"
#include <freertos/FreeRTOS.h>

/**
 * web_dashboard.h — HTTP + WebSocket dashboard
 *
 * HTTP  port 80 : serves the embedded HTML dashboard
 * WebSocket port 81 : pushes JSON state every 1 s;
 *                     receives control commands from the browser
 *
 * Call web_dashboard_init() once after wifi_ap_init() succeeds.
 * Call web_dashboard_update() every loop iteration (after CAN processing).
 * If init was never called, update() is a safe no-op.
 */

/**
 * Initialise HTTP and WebSocket servers.
 *
 * @param state      Pointer to the shared FSDState (read + written by command handler)
 * @param can_buses  CAN drivers to switch between listen-only and active mode
 * @param can_count  Number of entries in can_buses
 * @param state_mux  Spinlock protecting cross-core access to state
 */
void web_dashboard_init(FSDState *state,
                        CanDriver **can_buses,
                        uint8_t can_count,
                        portMUX_TYPE *state_mux);

/** Service HTTP requests and WebSocket messages; broadcast state at 1 Hz. */
void web_dashboard_update();
