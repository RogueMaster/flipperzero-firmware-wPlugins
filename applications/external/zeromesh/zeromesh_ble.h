#pragma once

#include <furi_hal_bt.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ZeroMeshBleRxCallback)(const uint8_t* data, uint16_t len, void* context);

extern const FuriHalBleProfileTemplate* zeromesh_ble_profile;

uint16_t zeromesh_ble_profile_max_frame(void);

bool zeromesh_ble_profile_tx(FuriHalBleProfileBase* profile, const uint8_t* data, uint16_t len);

void zeromesh_ble_profile_set_rx_callback(
    FuriHalBleProfileBase* profile,
    ZeroMeshBleRxCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
