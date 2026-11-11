/**
 * @file furi_hal_version_i.h
 * Firmware-internal version overrides used by the name-spoof feature.
 *
 * These are NOT part of the public SDK: the `*_i.h` suffix is excluded from the
 * exported headers, so they add no api_symbols entries and are only linkable
 * from firmware translation units.
 *
 * Both setters MUST be called AFTER furi_hal_version_set_name(): that function
 * re-derives ble_mac and custom_uid from the (custom) name, so calling these
 * before it would be silently overwritten.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Override the advertised BLE MAC in the running version state (RAM only). */
void furi_hal_version_set_ble_mac(const uint8_t mac[6]);

/** Override the 8-byte serial/UID returned by furi_hal_version_uid() (RAM only). */
void furi_hal_version_set_custom_uid(const uint8_t uid[8]);

#ifdef __cplusplus
}
#endif
