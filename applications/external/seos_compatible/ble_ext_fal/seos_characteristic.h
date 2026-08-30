#pragma once

#include "../seos_ble_plugin.h"

#include <furi.h>
#include <lib/toolbox/bit_buffer.h>

#include <mbedtls/des.h>
#include <mbedtls/aes.h>

#include "secure_messaging.h"
#include "seos_common.h"
#include "seos_credential.h"
#include "seos.h"
#include "seos_att.h"
#include "keys.h"
#include "seos_sio_collect.h"

typedef struct {
    Seos* seos;
    SeosAtt* seos_att;
    uint16_t handle;
    BitBuffer* rx_buffer;

    SeosPhase phase;

    FlowMode flow_mode;

    AuthParameters params;
    SecureMessaging* secure_messaging;
    SeosCredential* credential;

    /* A read answer longer than one frame arrives over several notifications,
     * so where it has got to lives here rather than on a stack. */
    SeosSioCollector collector;
    BitBuffer* assembled;
} SeosCharacteristic;

SeosCharacteristic* seos_characteristic_alloc(Seos* seos);
void seos_characteristic_free(SeosCharacteristic* seos_characteristic);
void seos_characteristic_start(SeosCharacteristic* seos_characteristic, FlowMode mode);
void seos_characteristic_stop(SeosCharacteristic* seos_characteristic);
void seos_characteristic_write_request(void* context, BitBuffer* attribute_value);
void seos_characteristic_on_subscribe(void* context, uint16_t handle);
