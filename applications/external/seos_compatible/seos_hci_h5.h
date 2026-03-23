#pragma once

#include <string.h>
#include <stdlib.h>

#include <furi.h>
#include <lib/toolbox/bit_buffer.h>
#include <lib/bit_lib/bit_lib.h>

#include "uart.h"

// include/net/bluetooth/hci.h
/* HCI data types */
#define HCI_COMMAND_PKT 0x01
#define HCI_ACLDATA_PKT 0x02
#define HCI_SCODATA_PKT 0x03
#define HCI_EVENT_PKT   0x04
#define HCI_ISODATA_PKT 0x05
#define HCI_DIAG_PKT    0xf0
#define HCI_VENDOR_PKT  0xff

#define HCI_3WIRE_ACK_PKT  0
#define HCI_3WIRE_LINK_PKT 0x0F

#define H5_TX_WIN_MAX 4

typedef size_t (*SeosHciH5ReceiveCallback)(void* context, BitBuffer* frame);
typedef void (*SeosHciH5InitCallback)(void* context);

typedef struct {
    SeosUart* uart;

    uint8_t tx_seq; /* Next seq number to send */
    uint8_t tx_ack; /* Next ack number to send */
    uint8_t tx_win; /* Sliding window size */
    uint8_t rx_ack; /* Last ack number received */

    unsigned long flags;

    enum {
        H5_UNINITIALIZED,
        H5_INITIALIZED,
        H5_ACTIVE,
    } state;
    enum {
        H5_AWAKE,
        H5_SLEEPING,
        H5_WAKING_UP,
    } sleep;

    enum {
        STOPPED,
        STARTED
    } stage;

    SeosHciH5ReceiveCallback receive_callback;
    void* receive_callback_context;

    SeosHciH5InitCallback init_callback;
    void* init_callback_context;

    size_t out_of_order_count;

    FuriMessageQueue* messages;
    FuriMutex* mq_mutex;
    FuriThread* thread;
} SeosHciH5;

SeosHciH5* seos_hci_h5_alloc();
void seos_hci_h5_free(SeosHciH5* seos_hci_h5);
void seos_hci_h5_start(SeosHciH5* seos_hci_h5);
void seos_hci_h5_stop(SeosHciH5* seos_hci_h5);
void seos_hci_h5_send(SeosHciH5* seos_hci_h5, uint8_t pkt_type, BitBuffer* message);
size_t seos_hci_h5_recv(void* context, uint8_t* buffer, size_t len);
void seos_hci_h5_set_receive_callback(
    SeosHciH5* seos_hci_h5,
    SeosHciH5ReceiveCallback callback,
    void* context);
void seos_hci_h5_set_init_callback(
    SeosHciH5* seos_hci_h5,
    SeosHciH5InitCallback callback,
    void* context);
