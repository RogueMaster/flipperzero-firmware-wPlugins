#ifdef SEADER_HOST_TEST
#include "lib/host_tests/t_1_host_env.h"
#else
#include "t_1.h"
#endif

#include "t_1_logic.h"

#define TAG "Seader:T=1"

static SeaderT1State* seader_t1_state(SeaderUartBridge* seader_uart) {
    return &seader_uart->t1;
}

static uint8_t seader_next_dpcb(SeaderUartBridge* seader_uart) {
    SeaderT1State* t1 = seader_t1_state(seader_uart);
    uint8_t next_pcb = seader_t1_next_pcb(t1->send_pcb);
    t1->send_pcb = next_pcb;
    return t1->send_pcb;
}

void seader_t_1_reset(SeaderUartBridge* seader_uart) {
    SeaderT1State* t1 = seader_t1_state(seader_uart);
    t1->nad = 0x00;
    if(t1->ifsc == 0 || t1->ifsc > SEADER_T1_IFS_MAX) {
        t1->ifsc = SEADER_T1_IFS_DEFAULT;
    }
    t1->ifsd = SEADER_T1_IFS_DEFAULT;
    t1->ifsd_pending = 0;
    seader_t1_reset_link_state(t1);
}

void seader_t_1_set_IFSD(Seader* seader) {
    SeaderWorker* seader_worker = seader->worker;
    SeaderUartBridge* seader_uart = seader_worker->uart;
    SeaderT1State* t1 = seader_t1_state(seader_uart);
    uint8_t frame[5];
    uint8_t frame_len = 0;
    /* Negotiate the largest host receive size we support so chained responses stay predictable. */
    uint8_t requested_ifsd = SEADER_T1_IFS_MAX;

    frame[0] = t1->nad;
    frame[1] = SEADER_T1_PCB_S_BLOCK | SEADER_T1_S_BLOCK_IFS;
    frame[2] = 0x01;
    t1->ifsd_pending = requested_ifsd;
    frame[3] = requested_ifsd;
    frame_len = 4;

    frame_len = seader_add_lrc(frame, frame_len);
    seader_ccid_XfrBlock(seader_uart, frame, frame_len);
}

static void seader_t_1_IFSD_response(Seader* seader, uint8_t ifs_value) {
    SeaderWorker* seader_worker = seader->worker;
    SeaderUartBridge* seader_uart = seader_worker->uart;
    SeaderT1State* t1 = seader_t1_state(seader_uart);
    uint8_t frame[5];
    uint8_t frame_len = 0;

    frame[0] = t1->nad;
    frame[1] = 0xE0 | SEADER_T1_S_BLOCK_IFS;
    frame[2] = 0x01;
    frame[3] = ifs_value;
    frame_len = 4;

    frame_len = seader_add_lrc(frame, frame_len);
    seader_ccid_XfrBlock(seader_uart, frame, frame_len);
}

static void seader_t_1_WTX_response(Seader* seader, uint8_t multiplier) {
    SeaderWorker* seader_worker = seader->worker;
    SeaderUartBridge* seader_uart = seader_worker->uart;
    SeaderT1State* t1 = seader_t1_state(seader_uart);
    uint8_t frame[5];
    uint8_t frame_len = 0;

    frame[0] = t1->nad;
    frame[1] = 0xE0 | SEADER_T1_S_BLOCK_WTX;
    frame[2] = 0x01;
    frame[3] = multiplier;
    frame_len = 4;

    frame_len = seader_add_lrc(frame, frame_len);
    seader_ccid_XfrBlock(seader_uart, frame, frame_len);
}

static void seader_t_1_resynch_response(Seader* seader) {
    SeaderWorker* seader_worker = seader->worker;
    SeaderUartBridge* seader_uart = seader_worker->uart;
    SeaderT1State* t1 = seader_t1_state(seader_uart);
    uint8_t frame[4];
    uint8_t frame_len = 0;

    frame[0] = t1->nad;
    frame[1] = 0xE0 | SEADER_T1_S_BLOCK_RESYNCH;
    frame[2] = 0x00;
    frame_len = 3;

    frame_len = seader_add_lrc(frame, frame_len);
    seader_ccid_XfrBlock(seader_uart, frame, frame_len);
}

void seader_t_1_send_ack(Seader* seader) {
    SeaderWorker* seader_worker = seader->worker;
    SeaderUartBridge* seader_uart = seader_worker->uart;
    SeaderT1State* t1 = seader_t1_state(seader_uart);
    uint8_t frame[4];
    uint8_t frame_len = 0;

    frame[0] = t1->nad;
    frame[1] = SEADER_T1_PCB_R_BLOCK | (t1->recv_pcb >> 2);
    frame[2] = 0x00;
    frame_len = 3;

    frame_len = seader_add_lrc(frame, frame_len);
    seader_ccid_XfrBlock(seader_uart, frame, frame_len);
}

static void seader_t_1_send_nak(Seader* seader) {
    SeaderWorker* seader_worker = seader->worker;
    SeaderUartBridge* seader_uart = seader_worker->uart;
    SeaderT1State* t1 = seader_t1_state(seader_uart);
    uint8_t frame[4];
    uint8_t frame_len = 0;

    frame[0] = t1->nad;
    frame[1] = SEADER_T1_PCB_R_BLOCK | (t1->recv_pcb >> 2) | 0x01;
    frame[2] = 0x00;
    frame_len = 3;

    frame_len = seader_add_lrc(frame, frame_len);
    FURI_LOG_W(TAG, "Sending R-Block NACK: PCB: %02x", frame[1]);
    seader_ccid_XfrBlock(seader_uart, frame, frame_len);
}

void seader_send_t1_chunk(SeaderUartBridge* seader_uart, uint8_t pcb, uint8_t* chunk, size_t len) {
    SeaderT1State* t1 = seader_t1_state(seader_uart);
    uint8_t* frame = malloc(3 + len + 1);
    uint8_t frame_len = 0;

    frame[0] = t1->nad;
    frame[1] = pcb;
    frame[2] = len;
    frame_len = 3;

    if(len > 0) {
        memcpy(frame + frame_len, chunk, len);
        frame_len += len;
    }

    frame_len = seader_add_lrc(frame, frame_len);
    seader_ccid_XfrBlock(seader_uart, frame, frame_len);
    free(frame);
}

void seader_send_t1_scratchpad(
    SeaderUartBridge* seader_uart,
    uint8_t pcb,
    uint8_t* apdu,
    size_t len) {
    SeaderT1State* t1 = seader_t1_state(seader_uart);
    uint8_t* frame = apdu - 3;

    frame[0] = t1->nad;
    frame[1] = pcb;
    frame[2] = (uint8_t)len;

    size_t frame_len = seader_add_lrc(frame, 3 + len);
    seader_ccid_XfrBlock(seader_uart, frame, frame_len);
}

void seader_send_t1(SeaderUartBridge* seader_uart, uint8_t* apdu, size_t len) {
    SeaderT1State* t1 = seader_t1_state(seader_uart);
    uint8_t ifsc = t1->ifsc;

    if(ifsc == 0 || ifsc > SEADER_T1_IFS_MAX) {
        ifsc = SEADER_T1_IFS_DEFAULT;
    }

    if(t1->tx_buffer == NULL) {
        bool in_scratchpad =
            apdu != NULL &&
            seader_t1_apdu_in_scratchpad(seader_uart->tx_buf, SEADER_UART_RX_BUF_SIZE, apdu);

        if(in_scratchpad && len <= ifsc) {
            seader_send_t1_scratchpad(seader_uart, seader_next_dpcb(seader_uart), apdu, len);
            t1->last_tx_len = len;
            return;
        }

        t1->tx_buffer = bit_buffer_alloc(768);
        bit_buffer_copy_bytes(t1->tx_buffer, apdu, len);
        t1->tx_buffer_offset = 0;
    }

    size_t total_len = bit_buffer_get_size_bytes(t1->tx_buffer);
    size_t remaining = total_len - t1->tx_buffer_offset;
    size_t copy_length = seader_t1_chunk_length(total_len, t1->tx_buffer_offset, ifsc);
    uint8_t* chunk = (uint8_t*)bit_buffer_get_data(t1->tx_buffer) + t1->tx_buffer_offset;

    uint8_t pcb;
    if(remaining > ifsc) {
        pcb = seader_next_dpcb(seader_uart) | SEADER_T1_PCB_I_BLOCK_MORE;
    } else {
        pcb = seader_next_dpcb(seader_uart);
    }

    seader_send_t1_chunk(seader_uart, pcb, chunk, copy_length);
    t1->last_tx_len = copy_length;
    t1->tx_buffer_offset += copy_length;
}

bool seader_recv_t1(Seader* seader, CCID_Message* message) {
    SeaderWorker* seader_worker = seader->worker;
    SeaderUartBridge* seader_uart = seader_worker->uart;
    SeaderT1State* t1 = seader_t1_state(seader_uart);
    uint8_t* apdu = NULL;
    size_t apdu_len = 0;

    SeaderT1Action action =
        seader_t1_handle_block(t1, message->payload, message->dwLength, &apdu, &apdu_len);

    /* Keep transport decisions here so host tests exercise the same action-to-wire mapping. */
    switch(action) {
    case SeaderT1ActionDeliverAPDU:
        if(t1->rx_buffer != NULL) {
            seader_worker_process_sam_message(seader, apdu, apdu_len);
            bit_buffer_free(t1->rx_buffer);
            t1->rx_buffer = NULL;
            return true;
        }
        return seader_worker_process_sam_message(seader, apdu, apdu_len);

    case SeaderT1ActionSendAck:
        seader_t_1_send_ack(seader);
        return false;

    case SeaderT1ActionSendNak:
        seader_t_1_send_nak(seader);
        return false;

    case SeaderT1ActionSendIFSResponse:
        if(apdu_len == 1 && apdu != NULL) {
            seader_t_1_IFSD_response(seader, apdu[0]);
        } else {
            seader_t_1_send_nak(seader);
        }
        return false;

    case SeaderT1ActionSendWTXResponse:
        seader_t_1_WTX_response(seader, apdu[0]);
        return false;

    case SeaderT1ActionSendResynchResponse:
        seader_t1_reset_link_state(t1);
        seader_t_1_resynch_response(seader);
        return false;

    case SeaderT1ActionSendVersion:
        seader_worker_send_version(seader);
        if(seader_worker->callback) {
            seader_worker->callback(SeaderWorkerEventSamPresent, seader_worker->context);
        }
        return false;

    case SeaderT1ActionSendMoreData:
        seader_send_t1(
            seader_uart,
            (uint8_t*)bit_buffer_get_data(t1->tx_buffer),
            bit_buffer_get_size_bytes(t1->tx_buffer));
        return false;

    case SeaderT1ActionRetransmit:
        seader_send_t1(seader_uart, NULL, 0);
        return false;

    case SeaderT1ActionNone:
        return true;

    case SeaderT1ActionError:
    default:
        FURI_LOG_W(TAG, "T=1 error or unhandled action %d", action);
        return false;
    }
}
