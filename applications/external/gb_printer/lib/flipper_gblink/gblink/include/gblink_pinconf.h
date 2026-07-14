// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023 KBEmbedded

#ifndef __GBLINK_PINCONF_H__
#define __GBLINK_PINCONF_H__

#pragma once

#include <furi.h>
#include <furi_hal.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Set one of the pre-configured pinouts
 *
 * @param handle Pointer to gblink handle
 * @param pinout Which pinout to use
 *
 * @note The gblink instance must not be gblink_start()'ed!
 *
 * @returns 0 on success, -1 if gblink instance is not gblink_stop()'ed.
 */
int gblink_pin_set_default(void *handle, gblink_pinouts pinout);

/**
 * Check if the pinout set matches a pre-configured one
 *
 * @param handle Pointer to gblink handle
 *
 * @returns The index of the pre-configured pinout or -1 on error
 */
int gblink_pin_get_default(void *handle);

/**
 * Set a GPIO pin to a specific pin mode for the EXT interface
 *
 * @param handle Pointer to gblink handle
 * @param pin Pin mode to assign to the gpio pin
 * @param pinnum GPIO pin number to assign pin mode to
 *
 * @note The gblink instance must not be gblink_start()'ed!
 *
 * @returns 0 on success, -1 if gblink instance is not gblink_stop()ed
 */
int gblink_pin_set(void *handle, gblink_bus_pins pin, unsigned int pinum);

/**
 * Get the pin number associated with the requested pin mode
 *
 *
 * @param handle Pointer to gblink handle
 * @param pin Pin mode to inquire about
 *
 * @returns Pin number of the requested pin or -1 on error
 */
int gblink_pin_get(void *handle, gblink_bus_pins pin);

/**
 * Set a gpio pin to a specific pin mode via GpioPin
 *
 * @param handle Pointer to gblink handle
 * @param pin Pin mode to assign to the gpio pin
 * @param gpio Which gpio pin to assign the pin mode
 *
 * @note The gblink instance must not be gblink_start()'ed!
 * @note There is no sane way to do bounds checking with this function, so be
 * careful when passing pointers to ensure they are actually Flipper GpioPin
 * references.
 *
 * @returns 0 on success, -1 if gblink instance is not gblink_stop()'ed.
 */
int gblink_pin_set_by_gpiopin(void *handle, gblink_bus_pins pin, const GpioPin *gpio);

/**
 * Get the GpioPin associated with the requested pin mode
 *
 * @param handle Pointer to gblink handle
 * @param pin Pin mode to inquire about
 *
 * @returns GpioPin pointer
 */
const GpioPin *gblink_pin_get_by_gpiopin(void *handle, gblink_bus_pins pin);

/**
 * Returns the pinnum of the highest usable, non-debug pin.
 *
 * @returns count of highest usable, non-debug pin or -1 on error
 */
int gblink_pin_count_max(void);

/**
 * Get the next usable, non-debug GPIO pin by number.
 *
 * The return value is the next usable, non-debug pin starting from pinnum and
 * will include pinnum if it is a valid pin.
 *
 * @param pinnum GPIO pin number to check if it is a valid pin.
 *
 * @returns -1 on error or no more valid pins. Any other value is the next usable
 * pin which can include pinnum if it is usable. If pinnum is not usable, the next
 * numbered pin which is will be returned or -1 if there are no further pins.
 */
int gblink_pin_get_next(unsigned int pinnum);

/**
 * Get the previous usable, non-debug GPIO pin by number.
 *
 * The return value is the previous usable, non-debug pin starting from pinnum and
 * will include pinnum if it is a valid pin.
 *
 * @param pinnum GPIO pin number to check if it is a valid pin.
 *
 * @returns -1 on error or no more valid pins. Any other value is the previous usable
 * pin which can include pinnum if it is usable. If pinnum is not usable, the previous
 * numbered pin which is will be returned or -1 if there are no further pins.
 */
int gblink_pin_get_prev(unsigned int pinnum);

/**
 * Load pin configuration from file automatically
 * Loads from .gblink_pinconf in app data folder
 *
 * @param gblink Pointer to gblink instance to set pins configurations
 *
 * @returns true on success, false on error
 *
 * @note This function should be called from the context of the application and
 * not any other threads as it uses the app data folder to load data from
 */
bool gblink_pinconf_load(void *gblink);

/**
 * Save current pin configuration to file automatically
 * Saves to .gblink_pinconf in app data folder
 *
 * @param gblink Pointer to gblink instance to save pins configurations
 *
 * @returns true on success, false on error
 *
 * @note This function should be called from the context of the application and
 * not any other threads as it uses the app data folder to save data to.
 */
bool gblink_pinconf_save(void *gblink);

#ifdef __cplusplus
}
#endif

#endif // __GBLINK_PINCONF_H__
