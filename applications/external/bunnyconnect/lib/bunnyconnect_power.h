#pragma once

#include <furi.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_cdc.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize USB power management
 */
void bunnyconnect_power_init(void);

/**
 * @brief Deinitialize USB power management
 */
void bunnyconnect_power_deinit(void);

/**
 * @brief Enable/disable USB power output
 * 
 * @param enabled Power state
 * @return true if successful, false otherwise
 */
bool bunnyconnect_power_set_usb_enabled(bool enabled);

/**
 * @brief Check if USB power is enabled
 * 
 * @return true if USB power is on, false otherwise
 */
bool bunnyconnect_power_is_usb_enabled(void);

/**
 * @brief Get USB connection status
 * 
 * @return true if USB device is connected, false otherwise
 */
bool bunnyconnect_power_is_usb_connected(void);

#ifdef __cplusplus
}
#endif
