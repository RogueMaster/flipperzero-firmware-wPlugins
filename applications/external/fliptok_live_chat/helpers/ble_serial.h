/*
 * This code is based on the Willy-JL's (https://github.com/Willy-JL) BLE fix.
 * 
 * Thank you to Willy-JL for providing this code and making it available under the https://github.com/Flipper-XFW/Xtreme-Apps repository.
 * Your contribution has been invaluable for this project.
 * 
 * Based on <targets/f7/ble_glue/profiles/serial_profile.h>
 * and on <lib/ble_profile/extra_profiles/hid_profile.h>
 */

#pragma once

#include <furi_ble/profile_interface.h>

#include <services/serial_service.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 
 * Optional arguments to pass along with profile template as 
 * FuriHalBleProfileParams for tuning profile behavior 
 **/
typedef struct {
    const char* device_name_prefix; /**< Prefix for device name. Length must be less than 8 */
    uint16_t mac_xor; /**< XOR mask for device address, for uniqueness */
} BleProfileSerialParams;

#define BLE_PROFILE_SERIAL_PACKET_SIZE_MAX BLE_SVC_SERIAL_DATA_LEN_MAX

/** Serial service callback type */
typedef SerialServiceEventCallback FuriHalBtSerialCallback;

/** Serial profile descriptor */
extern const FuriHalBleProfileTemplate* const ble_profile_serial;

/** Send data through BLE
 *
 * @param profile       Profile instance
 * @param data          data buffer
 * @param size          data buffer size
 *
 * @return      true on success
 */
bool ble_profile_serial_tx(FuriHalBleProfileBase* profile, uint8_t* data, uint16_t size);

/** Set Serial service events callback
 *
 * @param profile       Profile instance
 * @param buffer_size   Applicaition buffer size
 * @param calback       FuriHalBtSerialCallback instance
 * @param context       pointer to context
 */
void ble_profile_serial_set_event_callback(
    FuriHalBleProfileBase* profile,
    uint16_t buff_size,
    FuriHalBtSerialCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
