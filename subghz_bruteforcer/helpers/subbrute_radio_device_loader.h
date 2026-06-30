#pragma once

#include <lib/subghz/devices/devices.h>

/**
 * @file subghz_radio.h
 * @brief Defines the Sub-GHz radio device type.
 */
typedef enum {
    SubGhzRadioDeviceTypeInternal,
    SubGhzRadioDeviceTypeExternalCC1101,
} SubGhzRadioDeviceType;

/**
 * @brief Sets the SubGhz radio device type.
 *
 * This function is used to set the SubGhz radio device type for the SubBrute radio device loader.
 *
 * @param [in] current_radio_device Pointer to the current SubGhz radio device.
 * @param [in] radio_device_type The desired SubGhz radio device type.
 *
 * @return @c const SubGhzDevice* Pointer to the new SubGhz radio device.
 *
 * @remark This function sets the SubGhz radio device type for the SubBrute radio device loader.
 *         The current radio device will be replaced with a new instance of the specified radio device type.
 *         If @p current_radio_device is NULL, a new instance of the specified radio device type will be created.
 *
 * @note The caller is responsible for handling memory deallocation of the returned pointer.
 *
 */
const SubGhzDevice* subbrute_radio_device_loader_set(
    const SubGhzDevice* current_radio_device,
    SubGhzRadioDeviceType radio_device_type);

/**
 * @brief Unloads the radio device in the subbrute radio device loader.
 *
 * This function is called to unload the specified radio device in the
 * subbrute radio device loader.
 * It first checks if the radio device is valid and then performs the necessary steps to
 * power off the device.
 * If the radio device is not the CC1101 internal device, it calls @c subghz_devices_end()
 * function to terminate the radio device.
 *
 * @param[in] radio_device A pointer to the radio device to unload.
 *
 */
void subbrute_radio_device_loader_end(const SubGhzDevice* radio_device);
