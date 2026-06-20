#include "seos_l2cap_i.h"

#define TAG "SeosL2Cap"

#define ACL_START_NO_FLUSH 0x00
#define ACL_CONT           0x01
#define ACL_START          0x02

#define CID_ATT       0x0004
#define CID_LE_SIGNAL 0x0005

// 5.4.2. HCI ACL Data packets
#define SEOS_MAX_ACLDATA_SEND_LENGTH 23

struct l2cap_header {
    uint16_t payload_len;
    uint16_t cid;
} __packed;

SeosL2Cap* seos_l2cap_alloc(Seos* seos) {
    SeosL2Cap* seos_l2cap = malloc(sizeof(SeosL2Cap));
    memset(seos_l2cap, 0, sizeof(SeosL2Cap));

    // TODO: match MTU
    seos_l2cap->tx_accumulator = bit_buffer_alloc(256);
    seos_l2cap->rx_accumulator = bit_buffer_alloc(256);

    seos_l2cap->seos = seos;
    seos_l2cap->seos_hci = seos_hci_alloc(seos);

    // Callback to lower level services to call this one
    seos_hci_set_receive_callback(seos_l2cap->seos_hci, seos_l2cap_recv, seos_l2cap);
    seos_hci_set_completed_packets_callback(
        seos_l2cap->seos_hci, seos_l2cap_send_next_chunk, seos_l2cap);
    seos_hci_set_central_connection_callback(
        seos_l2cap->seos_hci, seos_l2cap_central_connection, seos_l2cap);

    return seos_l2cap;
}

void seos_l2cap_free(SeosL2Cap* seos_l2cap) {
    furi_assert(seos_l2cap);

    bit_buffer_free(seos_l2cap->tx_accumulator);
    bit_buffer_free(seos_l2cap->rx_accumulator);

    seos_hci_free(seos_l2cap->seos_hci);
    free(seos_l2cap);
}

void seos_l2cap_start(SeosL2Cap* seos_l2cap, BleMode mode, FlowMode flow_mode) {
    seos_hci_start(seos_l2cap->seos_hci, mode, flow_mode);
}

void seos_l2cap_stop(SeosL2Cap* seos_l2cap) {
    seos_hci_stop(seos_l2cap->seos_hci);
}

void seos_l2cap_recv(void* context, uint16_t handle, uint8_t flags, BitBuffer* pdu) {
    SeosL2Cap* seos_l2cap = (SeosL2Cap*)context;
    seos_l2cap->handle = handle;

    // seos_log_bitbuffer(TAG, "recv", pdu);
    // uint8_t Broadcast_Flag = flags >> 2;
    uint8_t Packet_Boundary_Flag = flags & 0x03;

    const uint8_t* data = bit_buffer_get_data(pdu);
    struct l2cap_header* header = (struct l2cap_header*)(data);

    switch(Packet_Boundary_Flag) {
    case ACL_CONT:
        if(bit_buffer_get_size_bytes(seos_l2cap->rx_accumulator) == 0) {
            FURI_LOG_I(TAG, "Request to att continue when no previous data received");
            seos_log_bitbuffer(TAG, "cont", pdu);
            return;
        }
        // No header this time
        bit_buffer_append_bytes(seos_l2cap->rx_accumulator, data, bit_buffer_get_size_bytes(pdu));

        // Check for overage
        if(bit_buffer_get_size_bytes(seos_l2cap->rx_accumulator) > seos_l2cap->pdu_len) {
            FURI_LOG_W(
                TAG,
                "Oh shit, too much data: %d > %d",
                bit_buffer_get_size_bytes(seos_l2cap->rx_accumulator),
                seos_l2cap->pdu_len);
            seos_log_bitbuffer(TAG, "cont", seos_l2cap->rx_accumulator);
        }
        // Full PDU
        if(bit_buffer_get_size_bytes(seos_l2cap->rx_accumulator) == seos_l2cap->pdu_len) {
            // FURI_LOG_I(TAG, "Complete reassembled PDU");
            // When we've accumulated, we've skipped copying the header, so we can pass the accumulator directly in.
            if(seos_l2cap->receive_callback) {
                seos_l2cap->receive_callback(
                    seos_l2cap->receive_callback_context, seos_l2cap->rx_accumulator);
            }
        }
        break;
    case ACL_START:
    case ACL_START_NO_FLUSH:
        uint16_t payload_len = header->payload_len;
        uint16_t cid = header->cid;

        if(bit_buffer_get_size_bytes(pdu) < header->payload_len) {
            // FURI_LOG_W(TAG, "Incomplete PDU");
            seos_l2cap->pdu_len = payload_len;
            bit_buffer_reset(seos_l2cap->rx_accumulator);
            bit_buffer_append_bytes(
                seos_l2cap->rx_accumulator,
                data + sizeof(struct l2cap_header),
                bit_buffer_get_size_bytes(pdu) - sizeof(struct l2cap_header));
            return;
        }

        if(cid == CID_ATT) {
            BitBuffer* payload = bit_buffer_alloc(payload_len);
            bit_buffer_append_bytes(payload, data + sizeof(struct l2cap_header), payload_len);
            if(seos_l2cap->receive_callback) {
                seos_l2cap->receive_callback(seos_l2cap->receive_callback_context, payload);
            }
            bit_buffer_free(payload);
        } else if(cid == CID_LE_SIGNAL) {
            seos_log_bitbuffer(TAG, "LE Signal", pdu);
        } else {
            FURI_LOG_W(TAG, "Unhandled CID: %d", cid);
        }

        break;
    default:
        FURI_LOG_W(TAG, "unhandled Packet_Boundary_Flag %d", Packet_Boundary_Flag);
        break;
    }
}

void seos_l2cap_central_connection(void* context) {
    SeosL2Cap* seos_l2cap = (SeosL2Cap*)context;
    if(seos_l2cap->central_connection_callback) {
        seos_l2cap->central_connection_callback(seos_l2cap->central_connection_context);
    }
}

void seos_l2cap_send_chunk(void* context) {
    SeosL2Cap* seos_l2cap = (SeosL2Cap*)context;
    // FURI_LOG_D(TAG, "seos_l2cap_send_chunk");
    uint16_t cid = CID_ATT;

    uint8_t Packet_Boundary_Flag = 0x00;

    bool continuation = bit_buffer_get_size_bytes(seos_l2cap->tx_accumulator) <
                        seos_l2cap->pdu_len;
    if(continuation) {
        Packet_Boundary_Flag = 0x01;
    }

    uint16_t tx_len =
        MIN((size_t)SEOS_MAX_ACLDATA_SEND_LENGTH,
            bit_buffer_get_size_bytes(seos_l2cap->tx_accumulator));
    if(tx_len == 0) {
        seos_l2cap->pdu_len = 0;
        return;
    }

    BitBuffer* response = bit_buffer_alloc(tx_len + sizeof(cid) + sizeof(tx_len));

    if(!continuation) {
        //pdu length
        bit_buffer_append_bytes(
            response, (uint8_t*)&seos_l2cap->pdu_len, sizeof(seos_l2cap->pdu_len));
        // cid
        bit_buffer_append_bytes(response, (uint8_t*)&cid, sizeof(cid));
    }
    // tx
    bit_buffer_append_bytes(response, bit_buffer_get_data(seos_l2cap->tx_accumulator), tx_len);
    FURI_LOG_D(TAG, "send_chunk: %d/%d bytes", tx_len, seos_l2cap->pdu_len);

    seos_hci_acldata_send(seos_l2cap->seos_hci, Packet_Boundary_Flag, response);
    bit_buffer_free(response);

    // trim off send bytes
    int new_len = bit_buffer_get_size_bytes(seos_l2cap->tx_accumulator) - tx_len;
    if(new_len <= 0) {
        bit_buffer_reset(seos_l2cap->tx_accumulator);
        seos_l2cap->pdu_len = 0;
        // FURI_LOG_D(TAG, "TX accumulator empty");
        return;
    }

    BitBuffer* tmp = bit_buffer_alloc(new_len);
    bit_buffer_append_bytes(
        tmp, bit_buffer_get_data(seos_l2cap->tx_accumulator) + tx_len, new_len);
    bit_buffer_reset(seos_l2cap->tx_accumulator);
    bit_buffer_append_bytes(
        seos_l2cap->tx_accumulator, bit_buffer_get_data(tmp), bit_buffer_get_size_bytes(tmp));
    bit_buffer_free(tmp);
    // FURI_LOG_D(TAG, "tx accumulator length = %d", bit_buffer_get_size_bytes(seos_l2cap->tx_accumulator));
}

void seos_l2cap_send(SeosL2Cap* seos_l2cap, BitBuffer* content) {
    // seos_log_buffer("seos_l2cap_send", content);

    if(bit_buffer_get_size_bytes(seos_l2cap->tx_accumulator) > 0) {
        FURI_LOG_W(TAG, "Failed to add message to L2CAP accumulator: it isn't empty");
        return;
    }
    bit_buffer_append_bytes(
        seos_l2cap->tx_accumulator,
        bit_buffer_get_data(content),
        bit_buffer_get_size_bytes(content));
    seos_l2cap->pdu_len = bit_buffer_get_size_bytes(content);

    seos_l2cap_send_chunk(seos_l2cap);
}

void seos_l2cap_send_next_chunk(void* context) {
    SeosL2Cap* seos_l2cap = (SeosL2Cap*)context;
    if(bit_buffer_get_size_bytes(seos_l2cap->tx_accumulator) > 0) {
        seos_l2cap_send_chunk(seos_l2cap);
    }
}

void seos_l2cap_set_receive_callback(
    SeosL2Cap* seos_l2cap,
    SeosL2CapReceiveCallback callback,
    void* context) {
    seos_l2cap->receive_callback = callback;
    seos_l2cap->receive_callback_context = context;
}

void seos_l2cap_set_central_connection_callback(
    SeosL2Cap* seos_l2cap,
    SeosL2CapCentralConnectionCallback callback,
    void* context) {
    seos_l2cap->central_connection_callback = callback;
    seos_l2cap->central_connection_context = context;
}
