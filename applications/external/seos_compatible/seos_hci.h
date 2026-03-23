#pragma once

#include <furi.h>
#include "seos_hci_h5.h"
#include "seos_common.h"

#define MAC_ADDRESS_LEN     6
#define MAX_SCANNED_ADDRESS 5

typedef void (
    *SeosHciReceiveCallback)(void* context, uint16_t handle, uint8_t flags, BitBuffer* pdu);

typedef void (*SeosHciCompletedPacketsCallback)(void* context);
typedef void (*SeosHciCentralConnectionCallback)(void* context);

typedef struct {
    uint8_t address[MAC_ADDRESS_LEN];
    bool used;
} ScanAddress;

typedef struct {
    Seos* seos;
    SeosHciH5* seos_hci_h5;
    uint16_t connection_handle;

    SeosHciReceiveCallback receive_callback;
    void* receive_callback_context;

    SeosHciCompletedPacketsCallback completed_packets_callback;
    void* completed_packets_context;

    SeosHciCentralConnectionCallback central_connection_callback;
    void* central_connection_context;

    BleMode mode;

    FlowMode flow_mode;

    bool scan_status;
    bool adv_status;
    uint8_t address[MAC_ADDRESS_LEN];
    uint8_t address_type;

    size_t adv_report_count;
    bool device_found;

    FuriTimer* timer;

    ScanAddress scanned_addresses[MAX_SCANNED_ADDRESS];
} SeosHci;

SeosHci* seos_hci_alloc(Seos* seos);
void seos_hci_free(SeosHci* seos_hci);
void seos_hci_start(SeosHci* seos_hci, BleMode mode, FlowMode flow_mode);
void seos_hci_stop(SeosHci* seos_hci);
size_t seos_hci_recv(void* context, BitBuffer* frame);
void seos_hci_acldata_send(SeosHci* seos_hci, uint8_t flags, BitBuffer* tx);
void seos_hci_init(void* context);

void seos_hci_set_receive_callback(
    SeosHci* seos_hci,
    SeosHciReceiveCallback callback,
    void* context);

void seos_hci_set_completed_packets_callback(
    SeosHci* seos_hci,
    SeosHciCompletedPacketsCallback callback,
    void* context);

void seos_hci_set_central_connection_callback(
    SeosHci* seos_hci,
    SeosHciCentralConnectionCallback callback,
    void* context);

void seos_hci_set_scan(SeosHci* seos_hci, bool enable);
void seos_hci_enable_advertising(SeosHci* seos_hci, bool enable);
void seos_hci_send_scan_params(SeosHci* seos_hci);
void seos_hci_connect(SeosHci* seos_hci);
