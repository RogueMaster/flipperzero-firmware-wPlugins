#pragma once

#include <furi.h>
#include <lib/toolbox/bit_buffer.h>

#include "seos_hci.h"
#include "seos_common.h"

typedef void (*SeosL2CapReceiveCallback)(void* context, BitBuffer* payload);
typedef void (*SeosL2CapCentralConnectionCallback)(void* context);

typedef struct {
    Seos* seos;
    SeosHci* seos_hci;
    // Where to collect up data
    BitBuffer* tx_accumulator;
    BitBuffer* rx_accumulator;
    uint16_t pdu_len;
    uint16_t handle;

    SeosL2CapReceiveCallback receive_callback;
    void* receive_callback_context;

    SeosL2CapCentralConnectionCallback central_connection_callback;
    void* central_connection_context;
} SeosL2Cap;

SeosL2Cap* seos_l2cap_alloc(Seos* seos);
void seos_l2cap_free(SeosL2Cap* seos_l2cap);
void seos_l2cap_start(SeosL2Cap* seos_l2cap, BleMode mode, FlowMode flow_mode);
void seos_l2cap_stop(SeosL2Cap* seos_l2cap);
void seos_l2cap_recv(void* context, uint16_t handle, uint8_t flags, BitBuffer* pdu);
void seos_l2cap_send_next_chunk(void* context);
void seos_l2cap_send(SeosL2Cap* seos_l2cap, BitBuffer* content);
void seos_l2cap_central_connection(void* context);
void seos_l2cap_set_receive_callback(
    SeosL2Cap* seos_l2cap,
    SeosL2CapReceiveCallback callback,
    void* context);

void seos_l2cap_set_central_connection_callback(
    SeosL2Cap* seos_l2cap,
    SeosL2CapCentralConnectionCallback callback,
    void* context);
