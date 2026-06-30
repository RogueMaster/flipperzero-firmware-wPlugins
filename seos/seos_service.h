#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "seos_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * Seos service. Implements RPC over BLE, with flow control.
 */

#define BLE_SVC_SEOS_DATA_LEN_MAX       (486)
#define BLE_SVC_SEOS_CHAR_VALUE_LEN_MAX (243)

// Number of bytes per chunk, after header byte
// Total length = BLE_CHUNK_SIZE + 1
#define BLE_CHUNK_SIZE 19

#define BLE_FLAG_SOM 0x80
#define BLE_FLAG_EOM 0x40
#define BLE_FLAG_ERR 0x20

typedef enum {
    SeosServiceEventTypeDataReceived,
    SeosServiceEventTypeDataSent,
    SeosServiceEventTypesBleResetRequest,
} SeosServiceEventType;

typedef struct {
    uint8_t* buffer;
    uint16_t size;
} SeosServiceData;

typedef struct {
    SeosServiceEventType event;
    SeosServiceData data;
} SeosServiceEvent;

typedef uint16_t (*SeosServiceEventCallback)(SeosServiceEvent event, void* context);

typedef struct BleServiceSeos BleServiceSeos;

BleServiceSeos* ble_svc_seos_start(FlowMode mode);

void ble_svc_seos_stop(BleServiceSeos* service);

void ble_svc_seos_set_callbacks(
    BleServiceSeos* service,
    uint16_t buff_size,
    SeosServiceEventCallback callback,
    void* context);

void ble_svc_seos_set_rpc_active(BleServiceSeos* service, bool active);

void ble_svc_seos_notify_buffer_is_empty(BleServiceSeos* service);

bool ble_svc_seos_update_tx(BleServiceSeos* service, uint8_t* data, uint16_t data_len);

#ifdef __cplusplus
}
#endif
