#pragma once

#include <ble_profile/extra_profiles/hid_profile.h>
#include <furi_hal_version.h>

/* Older/different ufbt SDKs do not publish the internal BLE name-size macro.
 * Use the same fixed 32-byte storage for our application-side configuration.
 */
#ifndef FURI_HAL_BT_ADV_NAME_LENGTH
#define FURI_HAL_BT_ADV_NAME_LENGTH 32U
#endif

/* Same three known BLE pairing modes; some ufbt SDKs omit the Count enum. */
#define BADUSB_GAP_PAIRING_COUNT (GapPairingPinCodeVerifyYesNo + 1)

/** 
 * Optional arguments to pass along with profile template as 
 * FuriHalBleProfileParams for tuning profile behavior 
 **/
typedef struct {
    char name[FURI_HAL_BT_ADV_NAME_LENGTH]; /**< Full device name */
    uint8_t mac[GAP_MAC_ADDR_SIZE]; /**< Full device address */
    bool bonding; /**< Save paired devices */
    GapPairing pairing; /**< Pairing security method */
} BleProfileHidExtParams;

/** Hid Keyboard Profile descriptor */
extern const FuriHalBleProfileTemplate* ble_profile_hid_ext;
