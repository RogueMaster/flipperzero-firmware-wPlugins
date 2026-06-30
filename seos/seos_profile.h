#pragma once

#include <furi_ble/profile_interface.h>

#include "seos_common.h"
#include "seos_service.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_PROFILE_SEOS_PACKET_SIZE_MAX BLE_SVC_SEOS_DATA_LEN_MAX

typedef struct ble_profile_params {
    FlowMode mode;
} BleProfileParams;

/** Seos service callback type */
typedef SeosServiceEventCallback FuriHalBtSeosCallback;

/** Seos profile descriptor */
extern const FuriHalBleProfileTemplate* ble_profile_seos;

/** Send data through BLE
 *
 * @param profile       Profile instance
 * @param data          data buffer
 * @param size          data buffer size
 *
 * @return      true on success
 */
bool ble_profile_seos_tx(FuriHalBleProfileBase* profile, uint8_t* data, uint16_t size);

/** Set Seos service events callback
 *
 * @param profile       Profile instance
 * @param buffer_size   Applicaition buffer size
 * @param calback       FuriHalBtSeosCallback instance
 * @param context       pointer to context
 */
void ble_profile_seos_set_event_callback(
    FuriHalBleProfileBase* profile,
    uint16_t buff_size,
    FuriHalBtSeosCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
