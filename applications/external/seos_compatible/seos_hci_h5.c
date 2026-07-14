#include "seos_hci_h5_i.h"

#define TAG "SeosHciH5"

#define MAX_OUT_OF_ORDER 3

/* Convenience macros for reading Three-wire header values */
#define H5_HDR_SEQ(hdr)      ((hdr)[0] & 0x07)
#define H5_HDR_ACK(hdr)      (((hdr)[0] >> 3) & 0x07)
#define H5_HDR_CRC(hdr)      (((hdr)[0] >> 6) & 0x01)
#define H5_HDR_RELIABLE(hdr) (((hdr)[0] >> 7) & 0x01)
#define H5_HDR_PKT_TYPE(hdr) ((hdr)[1] & 0x0f)
#define H5_HDR_LEN(hdr)      ((((hdr)[1] >> 4) & 0x0f) + ((hdr)[2] << 4))

#define SLIP_DELIMITER 0xc0
#define SLIP_ESC       0xdb
#define SLIP_ESC_DELIM 0xdc
#define SLIP_ESC_ESC   0xdd

#define MESSAGE_QUEUE_SIZE 10

/* H5 state flags */
enum {
    H5_RX_ESC, /* SLIP escape mode */
    H5_TX_ACK_REQ, /* Pending ack to send */
    H5_WAKEUP_DISABLE, /* Device cannot wake host */
    H5_HW_FLOW_CONTROL, /* Use HW flow control */
};

typedef struct {
    size_t len;
    uint8_t buf[SEOS_UART_RX_BUF_SIZE];
} HCI_MESSAGE;

int32_t seos_hci_h5_task(void* context);
void seos_hci_h5_link_control(SeosHciH5* seos_hci_h5, uint8_t* data, size_t len);

void seos_hci_h5_sync(SeosHciH5* seos_hci_h5) {
    seos_hci_h5->out_of_order_count = 0;
    uint8_t sync[] = {0x01, 0x7e};
    seos_hci_h5_link_control(seos_hci_h5, sync, sizeof(sync));
}

SeosHciH5* seos_hci_h5_alloc() {
    SeosHciH5* seos_hci_h5 = malloc(sizeof(SeosHciH5));
    memset(seos_hci_h5, 0, sizeof(SeosHciH5));
    seos_hci_h5->tx_win = H5_TX_WIN_MAX;
    seos_hci_h5->stage = STOPPED;

    seos_hci_h5->uart = seos_uart_alloc();
    seos_uart_set_receive_callback(seos_hci_h5->uart, seos_hci_h5_recv, seos_hci_h5);

    seos_hci_h5->thread =
        furi_thread_alloc_ex("SeosHciH5Worker", 5 * 1024, seos_hci_h5_task, seos_hci_h5);
    seos_hci_h5->messages = furi_message_queue_alloc(MESSAGE_QUEUE_SIZE, sizeof(HCI_MESSAGE));
    seos_hci_h5->mq_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    furi_thread_start(seos_hci_h5->thread);

    // Give the UART threads a little time to get started
    furi_delay_ms(3);

    return seos_hci_h5;
}

void seos_hci_h5_free(SeosHciH5* seos_hci_h5) {
    furi_assert(seos_hci_h5);

    furi_message_queue_free(seos_hci_h5->messages);
    furi_mutex_free(seos_hci_h5->mq_mutex);
    furi_thread_free(seos_hci_h5->thread);
    seos_uart_free(seos_hci_h5->uart);
    free(seos_hci_h5);
}

void seos_hci_h5_start(SeosHciH5* seos_hci_h5) {
    seos_hci_h5->stage = STARTED;
    seos_hci_h5->out_of_order_count = 0;

    /* Send initial sync request */
    seos_hci_h5_sync(seos_hci_h5);
}
void seos_hci_h5_stop(SeosHciH5* seos_hci_h5) {
    seos_hci_h5->stage = STOPPED;
    furi_thread_flags_set(furi_thread_get_id(seos_hci_h5->thread), WorkerEvtStop);
    furi_thread_join(seos_hci_h5->thread);
}

void seos_hci_h5_peer_reset(SeosHciH5* seos_hci_h5) {
    seos_hci_h5->state = H5_UNINITIALIZED;
    seos_hci_h5->tx_seq = 0;
    seos_hci_h5->tx_ack = 0;
}

void seos_hci_h5_reset_rx(SeosHciH5* seos_hci_h5) {
    bit_lib_set_bit((uint8_t*)&seos_hci_h5->flags, H5_RX_ESC, false);
}

void seos_hci_h5_link_control(SeosHciH5* seos_hci_h5, uint8_t* data, size_t len) {
    BitBuffer* message = bit_buffer_alloc(len);
    bit_buffer_append_bytes(message, data, len);

    seos_hci_h5_send(seos_hci_h5, HCI_3WIRE_LINK_PKT, message);
    bit_buffer_free(message);
}

void seos_hci_h5_handle_internal_rx(SeosHciH5* seos_hci_h5, BitBuffer* rx_skb) {
    uint8_t sync_req[] = {0x01, 0x7e};
    uint8_t sync_rsp[] = {0x02, 0x7d};
    uint8_t conf_req[3] = {0x03, 0xfc};
    uint8_t conf_rsp[] = {0x04, 0x7b};
    uint8_t wakeup_req[] = {0x05, 0xfa};
    uint8_t woken_req[] = {0x06, 0xf9};
    uint8_t sleep_req[] = {0x07, 0x78};
    const uint8_t* header = bit_buffer_get_data(rx_skb);
    const uint8_t* data = bit_buffer_get_data(rx_skb) + 4;

    if(H5_HDR_PKT_TYPE(header) != HCI_3WIRE_LINK_PKT) return;

    if(H5_HDR_LEN(header) < 2) return;

    conf_req[2] = seos_hci_h5->tx_win & 0x07;

    if(memcmp(data, sync_req, 2) == 0) {
        if(seos_hci_h5->state == H5_ACTIVE) seos_hci_h5_peer_reset(seos_hci_h5);
        seos_hci_h5_link_control(seos_hci_h5, sync_rsp, 2);
    } else if(memcmp(data, sync_rsp, 2) == 0) {
        if(seos_hci_h5->state == H5_ACTIVE) seos_hci_h5_peer_reset(seos_hci_h5);
        seos_hci_h5->state = H5_INITIALIZED;
        seos_hci_h5_link_control(seos_hci_h5, conf_req, 3);
    } else if(memcmp(data, conf_req, 2) == 0) {
        seos_hci_h5_link_control(seos_hci_h5, conf_rsp, 2);
        seos_hci_h5_link_control(seos_hci_h5, conf_req, 3);
    } else if(memcmp(data, conf_rsp, 2) == 0) {
        if(H5_HDR_LEN(header) > 2) seos_hci_h5->tx_win = (data[2] & 0x07);
        FURI_LOG_D(TAG, "---- Three-wire init complete. tx_win %u ----", seos_hci_h5->tx_win);
        seos_hci_h5->state = H5_ACTIVE;
        if(seos_hci_h5->init_callback) {
            seos_hci_h5->init_callback(seos_hci_h5->init_callback_context);
        }
        return;
    } else if(memcmp(data, sleep_req, 2) == 0) {
        FURI_LOG_D(TAG, "Peer went to sleep");
        seos_hci_h5->sleep = H5_SLEEPING;
        return;
    } else if(memcmp(data, woken_req, 2) == 0) {
        FURI_LOG_D(TAG, "Peer woke up");
        seos_hci_h5->sleep = H5_AWAKE;
    } else if(memcmp(data, wakeup_req, 2) == 0) {
        FURI_LOG_D(TAG, "Peer requested wakeup");
        seos_hci_h5_link_control(seos_hci_h5, woken_req, 2);
        seos_hci_h5->sleep = H5_AWAKE;
    } else {
        FURI_LOG_D(TAG, "Link Control: 0x%02hhx 0x%02hhx", data[0], data[1]);
        return;
    }

    // hci_uart_tx_wakeup(hu);
}

static void seos_hci_h5_slip_delim(BitBuffer* skb) {
    const char delim = SLIP_DELIMITER;

    bit_buffer_append_byte(skb, delim);
}

static void seos_hci_h5_slip_one_byte(BitBuffer* skb, uint8_t c) {
    const char esc_delim[2] = {SLIP_ESC, SLIP_ESC_DELIM};
    const char esc_esc[2] = {SLIP_ESC, SLIP_ESC_ESC};

    switch(c) {
    case SLIP_DELIMITER:
        bit_buffer_append_byte(skb, esc_delim[0]);
        bit_buffer_append_byte(skb, esc_delim[1]);
        break;
    case SLIP_ESC:
        bit_buffer_append_byte(skb, esc_esc[0]);
        bit_buffer_append_byte(skb, esc_esc[1]);

        break;
    default:
        bit_buffer_append_byte(skb, c);
    }
}

static void seos_hci_h5_unslip_one_byte(SeosHciH5* seos_hci_h5, BitBuffer* skb, uint8_t c) {
    const uint8_t delim = SLIP_DELIMITER, esc = SLIP_ESC;
    uint8_t byte = c;

    bool test = bit_lib_get_bit((uint8_t*)&seos_hci_h5->flags, H5_RX_ESC);
    if(!test && c == SLIP_ESC) {
        bit_lib_set_bit((uint8_t*)&seos_hci_h5->flags, H5_RX_ESC, true);
        return;
    }

    if(test) {
        bit_lib_set_bit((uint8_t*)&seos_hci_h5->flags, H5_RX_ESC, false);
        switch(c) {
        case SLIP_ESC_DELIM:
            byte = delim;
            break;
        case SLIP_ESC_ESC:
            byte = esc;
            break;
        default:
            FURI_LOG_W(TAG, "Invalid esc byte 0x%02hhx", c);
            seos_hci_h5_reset_rx(seos_hci_h5);
            return;
        }
    }

    bit_buffer_append_byte(skb, byte);
}

void seos_hci_h5_send(SeosHciH5* seos_hci_h5, uint8_t pkt_type, BitBuffer* message) {
    SeosUart* seos_uart = seos_hci_h5->uart;

    const uint8_t* message_buf = message ? bit_buffer_get_data(message) : NULL;
    size_t message_len = message ? bit_buffer_get_size_bytes(message) : 0;

    if(message_len > 0) {
        // seos_log_buffer("seos_hci_h5_send", message);
    }
    uint8_t header[4];

    /*
   * Max len of packet: (original len + 4 (H5 hdr) + 2 (crc)) * 2
   * (because bytes 0xc0 and 0xdb are escaped, worst case is when
   * the packet is all made of 0xc0 and 0xdb) + 2 (0xc0
   * delimiters at start and end).
   */
    size_t max_packet_len = (message_len + 6) * 2 + 2;

    BitBuffer* nskb = bit_buffer_alloc(max_packet_len);
    seos_hci_h5_slip_delim(nskb);

    bit_lib_set_bit((uint8_t*)&seos_hci_h5->flags, H5_TX_ACK_REQ, false);
    header[0] = seos_hci_h5->tx_ack << 3;

    /* Reliable packet? */
    if(pkt_type == HCI_ACLDATA_PKT || pkt_type == HCI_COMMAND_PKT) {
        header[0] |= 1 << 7;
        header[0] |= seos_hci_h5->tx_seq;
        seos_hci_h5->tx_seq = (seos_hci_h5->tx_seq + 1) % 8;
    }

    header[1] = pkt_type | ((message_len & 0x0f) << 4);
    header[2] = message_len >> 4;
    header[3] = ~((header[0] + header[1] + header[2]) & 0xff);

    /*
    FURI_LOG_D(
        TAG,
        "tx: seq %u ack %u crc %u rel %u type %u len %u",
        H5_HDR_SEQ(header),
        H5_HDR_ACK(header),
        H5_HDR_CRC(header),
        H5_HDR_RELIABLE(header),
        H5_HDR_PKT_TYPE(header),
        H5_HDR_LEN(header));
    */

    for(size_t i = 0; i < sizeof(header); i++) {
        seos_hci_h5_slip_one_byte(nskb, header[i]);
    }
    for(size_t i = 0; i < message_len; i++) {
        seos_hci_h5_slip_one_byte(nskb, message_buf[i]);
    }

    seos_hci_h5_slip_delim(nskb);

    seos_uart_send(
        seos_uart, (uint8_t*)bit_buffer_get_data(nskb), bit_buffer_get_size_bytes(nskb));
    bit_buffer_free(nskb);
}

size_t seos_hci_h5_recv(void* context, uint8_t* buffer, size_t len) {
    SeosHciH5* seos_hci_h5 = (SeosHciH5*)context;

    // Must get start byte + 4 byte header + end byte to even give a shit
    if(len < 6) {
        return 0;
    }

    if(buffer[0] != SLIP_DELIMITER) {
        // FURI_LOG_E(TAG, "UART didn't start with SLIP_DELIMITER (%02x)", buffer[0]);
        return 1;
    }

    BitBuffer* rx_skb = bit_buffer_alloc(len);

    // i = 1 -> Skip first c0 byte
    for(size_t i = 1; i < len; i++) {
        uint8_t c = buffer[i];
        seos_hci_h5_unslip_one_byte(seos_hci_h5, rx_skb, c);
    }
    // seos_log_buffer("HCI H5 Recv", rx_skb);

    // From h5_rx_3wire_hdr
    const uint8_t* header = bit_buffer_get_data(rx_skb);

    /*
    FURI_LOG_D(
        TAG,
        "rx: seq %u ack %u crc %u rel %u type %u len %u",
        H5_HDR_SEQ(header),
        H5_HDR_ACK(header),
        H5_HDR_CRC(header),
        H5_HDR_RELIABLE(header),
        H5_HDR_PKT_TYPE(header),
        H5_HDR_LEN(header));
        */

    if(((header[0] + header[1] + header[2] + header[3]) & 0xff) != 0xff) {
        FURI_LOG_W(TAG, "Invalid header checksum");
        bit_buffer_free(rx_skb);
        return len;
    }

    if(len < (size_t)(1 + 4 + H5_HDR_LEN(header) + 1)) {
        // FURI_LOG_W(TAG, "Incomplete packet (%d), wait for more data", len);
        bit_buffer_free(rx_skb);
        return 0;
    }

    if(H5_HDR_RELIABLE(header) && H5_HDR_SEQ(header) != seos_hci_h5->tx_ack) {
        FURI_LOG_W(
            TAG, "Out-of-order packet arrived (%u != %u)", H5_HDR_SEQ(header), seos_hci_h5->tx_ack);
        bit_lib_set_bit((uint8_t*)&seos_hci_h5->flags, H5_TX_ACK_REQ, true);
        seos_hci_h5_reset_rx(seos_hci_h5);

        seos_hci_h5->out_of_order_count++;
        if(seos_hci_h5->out_of_order_count >= MAX_OUT_OF_ORDER) {
            seos_hci_h5_sync(seos_hci_h5);
        }
        bit_buffer_free(rx_skb);
        return len;
    }
    // When a packet isn't out of order, reset the count
    seos_hci_h5->out_of_order_count = 0;

    if(seos_hci_h5->state != H5_ACTIVE && H5_HDR_PKT_TYPE(header) != HCI_3WIRE_LINK_PKT) {
        FURI_LOG_W(TAG, "Non-link packet received in non-active state");
        seos_hci_h5_reset_rx(seos_hci_h5);
        bit_buffer_free(rx_skb);
        return len;
    }

    if(H5_HDR_CRC(header)) {
        // TODO: Check header
    }

    // From h5_complete_rx_pkt
    if(H5_HDR_RELIABLE(header)) {
        seos_hci_h5->tx_ack = (seos_hci_h5->tx_ack + 1) % 8;
        bit_lib_set_bit((uint8_t*)&seos_hci_h5->flags, H5_TX_ACK_REQ, true);
        //hci_uart_tx_wakeup(hu);
    }

    seos_hci_h5->rx_ack = H5_HDR_ACK(header);
    size_t payload_len = H5_HDR_LEN(header);

    // Determine amount of data we consumed
    BitBuffer* tmp = bit_buffer_alloc(len);
    for(size_t i = 0; i < 4 + payload_len; i++) {
        seos_hci_h5_slip_one_byte(tmp, bit_buffer_get_byte(rx_skb, i));
    }
    size_t consumed = 1 + bit_buffer_get_size_bytes(tmp) + 1;
    if(consumed > len) {
        // At one point I was double processing the header, and not processing all the paylaod, and ended up with consumed > len.
        // This is to track if there are edge cases I missed.
        FURI_LOG_W(TAG, "Consumed %d > len %d", consumed, len);
        consumed = len;
    }
    bit_buffer_free(tmp);

    // Remove from send queue?
    // h5_pkt_cull(h5);

    switch(H5_HDR_PKT_TYPE(header)) {
    case HCI_EVENT_PKT:
    case HCI_ACLDATA_PKT:
    case HCI_SCODATA_PKT:
    case HCI_ISODATA_PKT:
        uint32_t space = furi_message_queue_get_space(seos_hci_h5->messages);
        if(space > 0) {
            HCI_MESSAGE message = {};
            message.len = 4 + payload_len;
            if(message.len > sizeof(message.buf)) {
                FURI_LOG_W(TAG, "Too big to queue");
                return len;
            }
            memcpy(message.buf, bit_buffer_get_data(rx_skb), message.len);

            if(furi_mutex_acquire(seos_hci_h5->mq_mutex, FuriWaitForever) == FuriStatusOk) {
                furi_message_queue_put(seos_hci_h5->messages, &message, FuriWaitForever);
                furi_mutex_release(seos_hci_h5->mq_mutex);
            }
            if(space < MESSAGE_QUEUE_SIZE / 2) {
                FURI_LOG_D(TAG, "Queue message.  %ld remaining", space);
            }
        } else {
            if(seos_hci_h5->stage != STOPPED) {
                FURI_LOG_E(TAG, "No space in message queue");
            }
        }
        break;
    default:
        seos_hci_h5_handle_internal_rx(seos_hci_h5, rx_skb);
        break;
    }

    if(bit_lib_get_bit((uint8_t*)&seos_hci_h5->flags, H5_TX_ACK_REQ)) {
        seos_hci_h5_send(seos_hci_h5, HCI_3WIRE_ACK_PKT, NULL);
    }

    seos_hci_h5_reset_rx(seos_hci_h5);

    bit_buffer_free(rx_skb);

    // FURI_LOG_D(TAG, "%d consumed", consumed);
    return consumed;
}

int32_t seos_hci_h5_task(void* context) {
    SeosHciH5* seos_hci_h5 = (SeosHciH5*)context;
    bool running = true;

    while(running) {
        uint32_t events = furi_thread_flags_get();
        if(events & WorkerEvtStop) {
            running = false;
            break;
        }

        if(furi_mutex_acquire(seos_hci_h5->mq_mutex, 1) == FuriStatusOk) {
            uint32_t count = furi_message_queue_get_count(seos_hci_h5->messages);
            if(count > 0) {
                if(count > MESSAGE_QUEUE_SIZE / 2) {
                    FURI_LOG_I(TAG, "Dequeue message [%ld messages]", count);
                }

                HCI_MESSAGE message = {};
                FuriStatus status =
                    furi_message_queue_get(seos_hci_h5->messages, &message, FuriWaitForever);
                if(status != FuriStatusOk) {
                    FURI_LOG_W(TAG, "furi_message_queue_get fail %d", status);
                }

                if(seos_hci_h5->receive_callback) {
                    size_t payload_len = H5_HDR_LEN(message.buf);
                    BitBuffer* frame = bit_buffer_alloc(payload_len + 1);
                    bit_buffer_append_byte(frame, H5_HDR_PKT_TYPE(message.buf));
                    bit_buffer_append_bytes(frame, message.buf + 4, payload_len);
                    seos_hci_h5->receive_callback(seos_hci_h5->receive_callback_context, frame);
                    bit_buffer_free(frame);
                }
            }
            furi_mutex_release(seos_hci_h5->mq_mutex);
        } else {
            FURI_LOG_W(TAG, "Failed to acquire mutex");
        }

        // A beat for event flags
        // furi_delay_ms(1);
    }

    return 0;
}

void seos_hci_h5_set_receive_callback(
    SeosHciH5* seos_hci_h5,
    SeosHciH5ReceiveCallback callback,
    void* context) {
    seos_hci_h5->receive_callback = callback;
    seos_hci_h5->receive_callback_context = context;
}

void seos_hci_h5_set_init_callback(
    SeosHciH5* seos_hci_h5,
    SeosHciH5InitCallback callback,
    void* context) {
    seos_hci_h5->init_callback = callback;
    seos_hci_h5->init_callback_context = context;
}
