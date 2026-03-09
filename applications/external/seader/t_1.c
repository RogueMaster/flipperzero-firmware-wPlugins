#ifdef SEADER_HOST_TEST
#include "lib/host_tests/t_1_host_env.h"
#else
#include "t_1.h"
#endif

#define TAG "Seader:T=1"

// http://www.sat-digest.com/SatXpress/SmartCard/ISO7816-4.htm

/* I know my T=1 is terrible, but I'm also only targetting one specific 'card' */

static SeaderT1State* seader_t1_state(SeaderUartBridge* seader_uart) {
    return &seader_uart->t1;
}

static uint8_t seader_next_dpcb(SeaderUartBridge* seader_uart) {
    SeaderT1State* t1 = seader_t1_state(seader_uart);
    uint8_t next_pcb = t1->send_pcb ^ SEADER_T1_PCB_SEQUENCE_BIT;
    //FURI_LOG_D(TAG, "dPCB was: %02X, current dPCB: %02X", dPCB, next_pcb);
    t1->send_pcb = next_pcb;
    return t1->send_pcb;
}

static uint8_t seader_next_cpcb(SeaderUartBridge* seader_uart) {
    SeaderT1State* t1 = seader_t1_state(seader_uart);
    uint8_t next_pcb = t1->recv_pcb ^ SEADER_T1_PCB_SEQUENCE_BIT;
    //FURI_LOG_D(TAG, "cPCB was: %02X, current cPCB: %02X", cPCB, next_pcb);
    t1->recv_pcb = next_pcb;
    return t1->recv_pcb;
}

void seader_t_1_reset(SeaderUartBridge* seader_uart) {
    SeaderT1State* t1 = seader_t1_state(seader_uart);
    t1->nad = 0x00;
    t1->send_pcb = SEADER_T1_PCB_SEQUENCE_BIT;
    t1->recv_pcb = 0x00;
    if(t1->tx_buffer != NULL) {
        bit_buffer_free(t1->tx_buffer);
    }
    t1->tx_buffer = NULL;
    t1->tx_buffer_offset = 0;
    if(t1->rx_buffer != NULL) {
        bit_buffer_free(t1->rx_buffer);
    }
    t1->rx_buffer = NULL;
}

void seader_t_1_set_IFSD(Seader* seader) {
    SeaderWorker* seader_worker = seader->worker;
    SeaderUartBridge* seader_uart = seader_worker->uart;
    SeaderT1State* t1 = seader_t1_state(seader_uart);
    uint8_t frame[5];
    uint8_t frame_len = 0;

    frame[0] = t1->nad;
    frame[1] = SEADER_T1_PCB_S_BLOCK | SEADER_T1_S_BLOCK_IFS; // S(IFS request)
    frame[2] = 0x01;
    frame[3] = t1->ifsc;
    frame_len = 4;

    frame_len = seader_add_lrc(frame, frame_len);

    seader_ccid_XfrBlock(seader_uart, frame, frame_len);
}

void seader_t_1_IFSD_response(Seader* seader) {
    SeaderWorker* seader_worker = seader->worker;
    SeaderUartBridge* seader_uart = seader_worker->uart;
    SeaderT1State* t1 = seader_t1_state(seader_uart);
    uint8_t frame[5];
    uint8_t frame_len = 0;

    frame[0] = t1->nad;
    frame[1] = 0xE0 | 0x01; // S(IFS response)
    frame[2] = 0x01;
    frame[3] = t1->ifsc;
    frame_len = 4;

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
    frame[1] = SEADER_T1_PCB_R_BLOCK | (seader_next_cpcb(seader_uart) >> 2);
    frame[2] = 0x00;
    frame_len = 3;

    frame_len = seader_add_lrc(frame, frame_len);

    //FURI_LOG_D(TAG, "Sending R-Block ACK: PCB: %02x", frame[1]);

    seader_ccid_XfrBlock(seader_uart, frame, frame_len);
}

void seader_send_t1_chunk(SeaderUartBridge* seader_uart, uint8_t PCB, uint8_t* chunk, size_t len) {
    SeaderT1State* t1 = seader_t1_state(seader_uart);
    uint8_t* frame = malloc(3 + len + 1);
    uint8_t frame_len = 0;

    frame[0] = t1->nad;
    frame[1] = PCB;
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
    uint8_t PCB,
    uint8_t* apdu,
    size_t len) {
    SeaderT1State* t1 = seader_t1_state(seader_uart);
    uint8_t* frame = apdu - 3;
    frame[0] = t1->nad;
    frame[1] = PCB;
    frame[2] = (uint8_t)len;

    size_t frame_len = seader_add_lrc(frame, 3 + len);
    seader_ccid_XfrBlock(seader_uart, frame, frame_len);
}

void seader_send_t1(SeaderUartBridge* seader_uart, uint8_t* apdu, size_t len) {
    SeaderT1State* t1 = seader_t1_state(seader_uart);
    uint8_t ifsc = t1->ifsc;

    bool in_scratchpad =
        (apdu >= seader_uart->tx_buf + 3 && apdu < seader_uart->tx_buf + SEADER_UART_RX_BUF_SIZE);

    if(len > ifsc) {
        if(t1->tx_buffer == NULL) {
            t1->tx_buffer = bit_buffer_alloc(768);
            bit_buffer_copy_bytes(t1->tx_buffer, apdu, len);
        }
        size_t remaining = (bit_buffer_get_size_bytes(t1->tx_buffer) - t1->tx_buffer_offset);
        size_t copy_length = remaining > ifsc ? ifsc : remaining;

        uint8_t* chunk = (uint8_t*)bit_buffer_get_data(t1->tx_buffer) + t1->tx_buffer_offset;

        if(remaining > ifsc) {
            uint8_t PCB = seader_next_dpcb(seader_uart) | SEADER_T1_PCB_I_BLOCK_MORE;
            seader_send_t1_chunk(seader_uart, PCB, chunk, copy_length);
        } else {
            uint8_t PCB = seader_next_dpcb(seader_uart);
            seader_send_t1_chunk(seader_uart, PCB, chunk, copy_length);
        }

        t1->tx_buffer_offset += copy_length;
        if(t1->tx_buffer_offset >= bit_buffer_get_size_bytes(t1->tx_buffer)) {
            bit_buffer_free(t1->tx_buffer);
            t1->tx_buffer = NULL;
            t1->tx_buffer_offset = 0;
        }
        return;
    }

    if(in_scratchpad) {
        seader_send_t1_scratchpad(seader_uart, seader_next_dpcb(seader_uart), apdu, len);
    } else {
        seader_send_t1_chunk(seader_uart, seader_next_dpcb(seader_uart), apdu, len);
    }
}

bool seader_recv_t1(Seader* seader, CCID_Message* message) {
    SeaderWorker* seader_worker = seader->worker;
    SeaderUartBridge* seader_uart = seader_worker->uart;
    SeaderT1State* t1 = seader_t1_state(seader_uart);
    // remove/validate NAD, PCB, LEN, LRC
    if(message->dwLength < 4) {
        FURI_LOG_W(TAG, "Invalid T=1 frame: too short");
        return false;
    }
    //uint8_t NAD = message->payload[0];
    uint8_t rPCB = message->payload[1];
    uint8_t LEN = message->payload[2];
    //uint8_t LRC = message->payload[3 + LEN];
    //FURI_LOG_D(TAG, "NAD: %02X, rPCB: %02X, LEN: %02X, LRC: %02X", NAD, rPCB, LEN, LRC);

    if(rPCB == 0xE1) {
        // S(IFS response)
        //FURI_LOG_D(TAG, "Received IFS Response");
        seader_worker_send_version(seader);
        SeaderWorker* seader_worker = seader->worker;
        if(seader_worker->callback) {
            seader_worker->callback(SeaderWorkerEventSamPresent, seader_worker->context);
        }
        return false;
    }

    if(rPCB == t1->recv_pcb) {
        seader_next_cpcb(seader_uart);
        if(t1->rx_buffer != NULL) {
            bit_buffer_append_bytes(t1->rx_buffer, message->payload + 3, LEN);

            // TODO: validate LRC

            seader_worker_process_sam_message(
                seader,
                (uint8_t*)bit_buffer_get_data(t1->rx_buffer),
                bit_buffer_get_size_bytes(t1->rx_buffer));

            bit_buffer_free(t1->rx_buffer);
            t1->rx_buffer = NULL;
            return true;
        }

        if(seader_validate_lrc(message->payload, message->dwLength) == false) {
            return false;
        }

        // Skip NAD, PCB, LEN
        message->payload = message->payload + 3;
        message->dwLength = LEN;

        if(message->dwLength == 0) {
            //FURI_LOG_D(TAG, "Received T=1 frame with no data");
            return true;
        }
        return seader_worker_process_sam_message(seader, message->payload, message->dwLength);
    } else if(rPCB == (t1->recv_pcb | SEADER_T1_PCB_I_BLOCK_MORE)) {
        //FURI_LOG_D(TAG, "Received T=1 frame with more bit set");
        if(t1->rx_buffer == NULL) {
            t1->rx_buffer = bit_buffer_alloc(512);
        }
        bit_buffer_append_bytes(t1->rx_buffer, message->payload + 3, LEN);
        seader_t_1_send_ack(seader);
        return false;
    } else if((rPCB & SEADER_T1_PCB_S_BLOCK) == SEADER_T1_PCB_S_BLOCK) {
        //FURI_LOG_D(TAG, "Received S-Block");
        if((rPCB & 0x0F) == SEADER_T1_S_BLOCK_IFS) {
            seader_t_1_IFSD_response(seader);
            return false;
        }

    } else if((rPCB & SEADER_T1_PCB_R_BLOCK) == SEADER_T1_PCB_R_BLOCK) {
        uint8_t R_SEQ = (rPCB & SEADER_T1_R_BLOCK_SEQUENCE_MASK) >> 4;
        uint8_t I_SEQ = (t1->send_pcb ^ SEADER_T1_PCB_SEQUENCE_BIT) >> 6;
        if(R_SEQ != I_SEQ) {
            /*
            FURI_LOG_D(
                TAG,
                "Received R-Block: Incorrect sequence.  Expected: %02X, Received: %02X",
                I_SEQ,
                R_SEQ);

            */
            // When this happens, the flipper freezes if it is doing NFC and my attempts to do events to stop that have failed
            return false;
        }

        if(t1->tx_buffer != NULL) {
            // Send more data, re-using the buffer to trigger the code path that sends the next block
            seader_send_t1(
                seader_uart,
                (uint8_t*)bit_buffer_get_data(t1->tx_buffer),
                bit_buffer_get_size_bytes(t1->tx_buffer));
            return false;
        }
    } else {
        FURI_LOG_W(
            TAG,
            "Invalid T=1 frame: PCB mismatch.  Expected: %02X, Received: %02X",
            t1->recv_pcb,
            rPCB);
    }

    return false;
}
