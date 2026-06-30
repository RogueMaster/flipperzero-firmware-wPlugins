#pragma once

#include <bt/bt_service/bt.h>
#include "seos_common.h"
#include "seos_profile.h"

typedef struct {
    Seos* seos;

    Bt* bt;
    FuriHalBleProfileBase* ble_profile;

    uint8_t event_buffer[128];
    BitBuffer* rx_buffer;

    SeosPhase phase;
    AuthParameters params;
    SecureMessaging* secure_messaging;
    SeosCredential* credential;
    FlowMode flow_mode;

    FuriMessageQueue* messages;
    FuriMutex* mq_mutex;
    FuriThread* thread;
} SeosNativePeripheral;

SeosNativePeripheral* seos_native_peripheral_alloc(Seos* seos);

void seos_native_peripheral_free(SeosNativePeripheral* seos_native_peripheral);

void seos_native_peripheral_start(SeosNativePeripheral* seos_native_peripheral, FlowMode mode);
void seos_native_peripheral_stop(SeosNativePeripheral* seos_native_peripheral);
