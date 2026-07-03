#pragma once

#include <furi.h>
#include <lib/toolbox/bit_buffer.h>

#include "seos_l2cap.h"
#include "seos_common.h"

#define ATT_ERROR_RSP              0x01
#define ATT_EXCHANGE_MTU_REQ       0x02
#define ATT_EXCHANGE_MTU_RSP       0x03
#define ATT_FIND_INFORMATION_REQ   0x04
#define ATT_FIND_INFORMATION_RSP   0x05
#define ATT_FIND_BY_TYPE_VALUE_REQ 0x06
#define ATT_FIND_BY_TYPE_VALUE_RSP 0x07
#define ATT_READ_BY_TYPE_REQ       0x08
#define ATT_READ_BY_TYPE_RSP       0x09
#define ATT_READ_REQ               0x0a
#define ATT_READ_RSP               0x0b
#define ATT_READ_BLOB_REQ          0x0c
#define ATT_READ_BLOB_RSP          0x0d
#define ATT_READ_MULTIPLE_REQ      0x0e
#define ATT_READ_MULTIPLE_RSP      0x0f
#define ATT_READ_BY_GROUP_TYPE_REQ 0x10
#define ATT_READ_BY_GROUP_TYPE_RSP 0x11
#define ATT_WRITE_REQ              0x12
#define ATT_WRITE_RSP              0x13
#define ATT_HANDLE_VALUE_NTF       0x1b
#define ATT_HANDLE_VALUE_IND       0x1d
#define ATT_HANDLE_VALUE_CFM       0x1e

#define ATT_READ_MULTIPLE_VARIABLE_REQ 0x20
#define ATT_READ_MULTIPLE_VARIABLE_RSP 0x21

#define ATT_WRITE_CMD 0x52

#define ATT_ERROR_CODE_ATTRIBUTE_NOT_FOUND   0x0a
#define ATT_ERROR_CODE_INVALID_HANDLE        0x01
#define ATT_ERROR_CODE_REQUEST_NOT_SUPPORTED 0x06

#define PRIMARY_SERVICE 0x2800
#define CHARACTERISTIC  0x2803
#define CCCD            0x2902
#define DEVICE_NAME     0x2a00
// service_changed = 2a05

#define DISABLE_NOTIFICATION_VALUE 0x0000
#define ENABLE_NOTIFICATION_VALUE  0x0001
#define ENABLE_INDICATION_VALUE    0x0002

typedef void (*SeosAttOnSubscribeCallback)(void* context, uint16_t handle);
typedef void (*SeosAttWriteRequestCallback)(void* context, BitBuffer* attribute_value);
typedef void (*SeosAttNotifyCallback)(void* context, const uint8_t* buffer, size_t buffer_len);

typedef struct {
    Seos* seos;
    SeosL2Cap* seos_l2cap;
    BitBuffer* tx_buf;
    uint16_t tx_mtu;
    uint16_t rx_mtu;

    SeosAttOnSubscribeCallback on_subscribe;
    void* on_subscribe_context;

    SeosAttWriteRequestCallback write_request;
    void* write_request_context;

    SeosAttNotifyCallback notify;
    void* notify_context;

    BleMode ble_mode;
    FlowMode flow_mode;

    uint16_t service_start_handle;
    uint16_t service_end_handle;
    uint16_t characteristic_handle;
    uint16_t value_handle;
    uint16_t cccd_handle;
} SeosAtt;

SeosAtt* seos_att_alloc(Seos* seos);
void seos_att_free(SeosAtt* seos_att);
void seos_att_start(SeosAtt* seos_att, BleMode mode, FlowMode flow_mode);
void seos_att_stop(SeosAtt* seos_att);
void seos_att_notify(SeosAtt* seos_att, uint16_t handle, BitBuffer* content);
void seos_att_write_request(SeosAtt* seos_att, BitBuffer* content);
void seos_att_process_payload(void* context, BitBuffer* message);
void seos_att_central_connection_start(void* context);

void seos_att_set_on_subscribe_callback(
    SeosAtt* seos_att,
    SeosAttOnSubscribeCallback callback,
    void* context);

void seos_att_set_write_request_callback(
    SeosAtt* seos_att,
    SeosAttWriteRequestCallback callback,
    void* context);

void seos_att_set_notify_callback(SeosAtt* seos_att, SeosAttNotifyCallback callback, void* context);
