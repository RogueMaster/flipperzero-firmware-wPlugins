#include "seader_i.h"
#include "ccid_logic.h"

#define TAG "SeaderCCID"
const uint8_t SAM_ATR[] =
    {0x3b, 0x95, 0x96, 0x80, 0xb1, 0xfe, 0x55, 0x1f, 0xc7, 0x47, 0x72, 0x61, 0x63, 0x65, 0x13};
const uint8_t SAM_ATR2[] = {0x3b, 0x90, 0x96, 0x91, 0x81, 0xb1, 0xfe, 0x55, 0x1f, 0xc7, 0xd4};
//3b95968011fc47726163653c
const uint8_t SAM_ATR3[] = {0x3b, 0x95, 0x96, 0x80, 0x11, 0xfc, 0x47, 0x72, 0x61, 0x63, 0x65, 0x3c};

static SeaderCcidState* seader_ccid_state(SeaderUartBridge* seader_uart) {
    return &seader_uart->ccid;
}

static SeaderCcidSlotState* seader_ccid_slot_state(SeaderUartBridge* seader_uart, uint8_t slot) {
    furi_check(slot < SEADER_CCID_SLOT_COUNT);
    return &seader_ccid_state(seader_uart)->slots[slot];
}

static uint8_t seader_ccid_current_slot(SeaderUartBridge* seader_uart) {
    return seader_ccid_state(seader_uart)->sam_slot;
}

static void seader_ccid_reset_slot_sequence(SeaderUartBridge* seader_uart, uint8_t slot) {
    seader_ccid_slot_state(seader_uart, slot)->sequence = 0;
}

static uint8_t seader_ccid_next_sequence(SeaderUartBridge* seader_uart, uint8_t slot) {
    SeaderCcidSlotState* slot_state = seader_ccid_slot_state(seader_uart, slot);
    return seader_ccid_sequence_advance(&slot_state->sequence);
}

void seader_ccid_IccPowerOn(SeaderUartBridge* seader_uart, uint8_t slot) {
    SeaderCcidSlotState* slot_state = seader_ccid_slot_state(seader_uart, slot);
    if(slot_state->powered) {
        return;
    }
    slot_state->powered = true;

    FURI_LOG_D(TAG, "Sending Power On (%d)", slot);
    memset(seader_uart->tx_buf, 0, SEADER_UART_RX_BUF_SIZE);
    seader_uart->tx_buf[0] = SYNC;
    seader_uart->tx_buf[1] = CTRL;
    seader_uart->tx_buf[2 + 0] = CCID_MESSAGE_TYPE_PC_TO_RDR_ICC_POWER_ON;

    seader_uart->tx_buf[2 + 5] = slot;
    seader_uart->tx_buf[2 + 6] = seader_ccid_next_sequence(seader_uart, slot);
    seader_uart->tx_buf[2 + 7] = 1; //power

    seader_uart->tx_len = seader_add_lrc(seader_uart->tx_buf, 2 + 10);
    furi_thread_flags_set(furi_thread_get_id(seader_uart->tx_thread), WorkerEvtSamRx);
}

void seader_ccid_IccPowerOff(SeaderUartBridge* seader_uart, uint8_t slot) {
    seader_ccid_slot_state(seader_uart, slot)->powered = false;

    FURI_LOG_D(TAG, "Sending Power Off (%d)", slot);
    memset(seader_uart->tx_buf, 0, SEADER_UART_RX_BUF_SIZE);
    seader_uart->tx_buf[0] = SYNC;
    seader_uart->tx_buf[1] = CTRL;
    seader_uart->tx_buf[2 + 0] = CCID_MESSAGE_TYPE_PC_TO_RDR_ICC_POWER_OFF;

    seader_uart->tx_buf[2 + 5] = slot;
    seader_uart->tx_buf[2 + 6] = seader_ccid_next_sequence(seader_uart, slot);

    seader_uart->tx_len = seader_add_lrc(seader_uart->tx_buf, 2 + 10);
    furi_thread_flags_set(furi_thread_get_id(seader_uart->tx_thread), WorkerEvtSamRx);
}

void seader_ccid_check_for_sam(SeaderUartBridge* seader_uart) {
    SeaderCcidState* ccid_state = seader_ccid_state(seader_uart);
    ccid_state->has_sam = false; // If someone is calling this, reset sam state
    ccid_state->sam_slot = 0;
    ccid_state->retries = 3;
    for(size_t slot = 0; slot < SEADER_CCID_SLOT_COUNT; slot++) {
        ccid_state->slots[slot].powered = false;
    }
    seader_ccid_GetSlotStatus(seader_uart, 0);
}

void seader_ccid_GetSlotStatus(SeaderUartBridge* seader_uart, uint8_t slot) {
    FURI_LOG_D(TAG, "seader_ccid_GetSlotStatus(%d)", slot);
    memset(seader_uart->tx_buf, 0, SEADER_UART_RX_BUF_SIZE);
    seader_uart->tx_buf[0] = SYNC;
    seader_uart->tx_buf[1] = CTRL;
    seader_uart->tx_buf[2 + 0] = CCID_MESSAGE_TYPE_PC_TO_RDR_GET_SLOT_STATUS;
    seader_uart->tx_buf[2 + 5] = slot;
    seader_uart->tx_buf[2 + 6] = seader_ccid_next_sequence(seader_uart, slot);

    seader_uart->tx_len = seader_add_lrc(seader_uart->tx_buf, 2 + 10);
    furi_thread_flags_set(furi_thread_get_id(seader_uart->tx_thread), WorkerEvtSamRx);
}

void seader_ccid_SetParameters(Seader* seader, uint8_t slot) {
    SeaderWorker* seader_worker = seader->worker;
    SeaderUartBridge* seader_uart = seader_worker->uart;
    FURI_LOG_D(TAG, "seader_ccid_SetParameters(%d)", slot);

    uint8_t payloadLen = 0;
    if(seader_uart->T == 0) {
        payloadLen = 5;
    } else if(seader_uart->T == 1) {
        payloadLen = 7;
    }
    memset(seader_uart->tx_buf, 0, SEADER_UART_RX_BUF_SIZE);
    seader_uart->tx_buf[0] = SYNC;
    seader_uart->tx_buf[1] = CTRL;
    seader_uart->tx_buf[2 + 0] = CCID_MESSAGE_TYPE_PC_TO_RDR_SET_PARAMETERS;
    seader_uart->tx_buf[2 + 1] = payloadLen;
    seader_uart->tx_buf[2 + 5] = slot;
    seader_uart->tx_buf[2 + 6] = seader_ccid_next_sequence(seader_uart, slot);
    seader_uart->tx_buf[2 + 7] = seader_uart->T;
    seader_uart->tx_buf[2 + 8] = 0;
    seader_uart->tx_buf[2 + 9] = 0;

    uint8_t* atr = seader->ATR;
    seader_uart->t1.ifsc = atr[5];

    if(seader_uart->T == 0) {
        // I'm leaving this here for completeness, but it was actually causing ICC_MUTE on the first apdu.
        seader_uart->tx_buf[2 + 10] = 0x11; //atr[2]; //bmFindexDindex
        seader_uart->tx_buf[2 + 11] = 0x00; //bmTCCKST1
        seader_uart->tx_buf[2 + 12] = 0x00; //bGuardTimeT0
        seader_uart->tx_buf[2 + 13] = 0x0a; //bWaitingIntegerT0
        seader_uart->tx_buf[2 + 14] = 0x00; //bClockStop
    } else if(atr[4] == 0xB1 && seader_uart->T == 1) {
        seader_uart->tx_buf[2 + 10] = atr[2]; //bmFindexDindex
        seader_uart->tx_buf[2 + 11] = 0x10; //bmTCCKST1
        seader_uart->tx_buf[2 + 12] = 0xfe; //bGuardTimeT1
        seader_uart->tx_buf[2 + 13] = atr[6]; //bWaitingIntegerT1
        seader_uart->tx_buf[2 + 14] = atr[8]; //bClockStop
        seader_uart->tx_buf[2 + 15] = seader_uart->t1.ifsc; //bIFSC
        seader_uart->tx_buf[2 + 16] = 0x00; //bNadValue
    } else if(seader_uart->T == 1) {
        seader_uart->tx_buf[2 + 10] = 0x11; //atr[2]; //bmFindexDindex
        seader_uart->tx_buf[2 + 11] = 0x10; //bmTCCKST1
        seader_uart->tx_buf[2 + 12] = 0x00; //bGuardTimeT1
        seader_uart->tx_buf[2 + 13] = 0x4d; //atr[6]; //bWaitingIntegerT1
        seader_uart->tx_buf[2 + 14] = 0x00; //atr[8]; //bClockStop
        seader_uart->tx_buf[2 + 15] = seader_uart->t1.ifsc; //bIFSC
        seader_uart->tx_buf[2 + 16] = 0x00; //bNadValue
    }

    seader_uart->tx_len = seader_add_lrc(seader_uart->tx_buf, 2 + 10 + payloadLen);
    furi_thread_flags_set(furi_thread_get_id(seader_uart->tx_thread), WorkerEvtSamRx);
}

void seader_ccid_GetParameters(SeaderUartBridge* seader_uart) {
    memset(seader_uart->tx_buf, 0, SEADER_UART_RX_BUF_SIZE);
    seader_uart->tx_buf[0] = SYNC;
    seader_uart->tx_buf[1] = CTRL;
    seader_uart->tx_buf[2 + 0] = CCID_MESSAGE_TYPE_PC_TO_RDR_GET_PARAMETERS;
    seader_uart->tx_buf[2 + 1] = 0;
    seader_uart->tx_buf[2 + 5] = seader_ccid_current_slot(seader_uart);
    seader_uart->tx_buf[2 + 6] =
        seader_ccid_next_sequence(seader_uart, seader_ccid_current_slot(seader_uart));
    seader_uart->tx_buf[2 + 7] = 0;
    seader_uart->tx_buf[2 + 8] = 0;
    seader_uart->tx_buf[2 + 9] = 0;

    seader_uart->tx_len = seader_add_lrc(seader_uart->tx_buf, 2 + 10);

    furi_thread_flags_set(furi_thread_get_id(seader_uart->tx_thread), WorkerEvtSamRx);
}

void seader_ccid_XfrBlock(SeaderUartBridge* seader_uart, uint8_t* data, size_t len) {
    seader_ccid_XfrBlockToSlot(seader_uart, seader_ccid_current_slot(seader_uart), data, len);
}

void seader_ccid_XfrBlockToSlot(
    SeaderUartBridge* seader_uart,
    uint8_t slot,
    uint8_t* data,
    size_t len) {
    uint8_t header_len = 2 + 10;
    if(!seader_ccid_payload_fits_frame(len, SEADER_UART_RX_BUF_SIZE, header_len)) {
        FURI_LOG_E(TAG, "CCID frame too long: %d", (int)(header_len + len));
        return;
    }

    uint8_t* tx_start = (uint8_t*)seader_uart->tx_buf;
    uint8_t* data_addr = (uint8_t*)data;
    bool in_scratchpad = seader_ccid_data_in_scratchpad(
        tx_start, SEADER_UART_RX_BUF_SIZE, header_len, data_addr, len);
    uint8_t* frame;

    if(in_scratchpad) {
        frame = data - header_len;
        seader_uart->tx_len = header_len + len;
        // Shift frame to start of tx_buf for UART worker
        if(frame != seader_uart->tx_buf) {
            memmove(seader_uart->tx_buf, frame, seader_uart->tx_len);
            frame = seader_uart->tx_buf;
        }
    } else {
        frame = seader_uart->tx_buf;
        memset(frame, 0, header_len);
        memcpy(frame + header_len, data, len);
        seader_uart->tx_len = header_len + len;
    }

    frame[0] = SYNC;
    frame[1] = CTRL;
    frame[2 + 0] = CCID_MESSAGE_TYPE_PC_TO_RDR_XFR_BLOCK;
    frame[2 + 1] = (len >> 0) & 0xff;
    frame[2 + 2] = (len >> 8) & 0xff;
    frame[2 + 3] = (len >> 16) & 0xff;
    frame[2 + 4] = (len >> 24) & 0xff;
    frame[2 + 5] = slot;
    frame[2 + 6] = seader_ccid_next_sequence(seader_uart, slot);
    frame[2 + 7] = 5;
    frame[2 + 8] = 0;
    frame[2 + 9] = 0;

    seader_uart->tx_len = seader_add_lrc(frame, seader_uart->tx_len);

    /*
    char* display = malloc(seader_uart->tx_len * 2 + 1);
    for(uint8_t i = 0; i < seader_uart->tx_len; i++) {
        snprintf(display + (i * 2), sizeof(display), "%02x", frame[i]);
    }
    FURI_LOG_D(TAG, "seader_ccid_XfrBlockToSlot(%d) %d: %s", slot, seader_uart->tx_len, display);
    free(display);
    */

    furi_thread_flags_set(furi_thread_get_id(seader_uart->tx_thread), WorkerEvtSamRx);
}

size_t seader_ccid_process(Seader* seader, uint8_t* cmd, size_t cmd_len) {
    SeaderWorker* seader_worker = seader->worker;
    SeaderUartBridge* seader_uart = seader_worker->uart;
    CCID_Message message;
    message.consumed = 0;
    SeaderCcidState* ccid_state = seader_ccid_state(seader_uart);

    char* display = malloc(cmd_len * 2 + 1);
    for(size_t i = 0; i < cmd_len; i++) {
        snprintf(display + (i * 2), sizeof(display), "%02x", cmd[i]);
    }
    FURI_LOG_D(TAG, "seader_ccid_process %d: %s", cmd_len, display);
    free(display);

    if(cmd_len == 2) {
        if(cmd[0] == CCID_MESSAGE_TYPE_RDR_TO_PC_NOTIFY_SLOT_CHANGE) {
            switch(cmd[1] & CCID_SLOT_0_MASK) {
            case 0:
            case 1:
                // No change, no-op
                break;
            case CCID_SLOT_0_CARD_IN:
                FURI_LOG_D(TAG, "Card Inserted (0)");
                if(ccid_state->has_sam && ccid_state->sam_slot == 0) {
                    break;
                }
                seader_ccid_reset_slot_sequence(seader_uart, 0);
                seader_ccid_IccPowerOn(seader_uart, 0);
                break;
            case CCID_SLOT_0_CARD_OUT:
                FURI_LOG_D(TAG, "Card Removed (0)");
                if(ccid_state->has_sam && ccid_state->sam_slot == 0) {
                    ccid_state->slots[0].powered = false;
                    ccid_state->has_sam = false;
                    ccid_state->retries = 3;
                    if(seader_worker->callback) {
                        seader_worker->callback(
                            SeaderWorkerEventSamMissing, seader_worker->context);
                    }
                }
                break;
            };

            switch(cmd[1] & CCID_SLOT_1_MASK) {
            case 0:
            case 1:
                // No change, no-op
                break;
            case CCID_SLOT_1_CARD_IN:
                FURI_LOG_D(TAG, "Card Inserted (1)");
                if(ccid_state->has_sam && ccid_state->sam_slot == 1) {
                    break;
                }
                seader_ccid_reset_slot_sequence(seader_uart, 1);
                seader_ccid_IccPowerOn(seader_uart, 1);
                break;
            case CCID_SLOT_1_CARD_OUT:
                FURI_LOG_D(TAG, "Card Removed (1)");
                if(ccid_state->has_sam && ccid_state->sam_slot == 1) {
                    ccid_state->slots[1].powered = false;
                    ccid_state->has_sam = false;
                    ccid_state->retries = 3;
                    if(seader_worker->callback) {
                        seader_worker->callback(
                            SeaderWorkerEventSamMissing, seader_worker->context);
                    }
                }
                break;
            };

            return 2;
        }
    }

    while(cmd_len >= 3 && cmd[0] == SYNC && cmd[1] == NAK) {
        // 031516
        FURI_LOG_W(TAG, "NAK");
        cmd += 3;
        cmd_len -= 3;
        message.consumed += 3;
    }

    while(cmd_len > 2 && (cmd[0] != SYNC || cmd[1] != CTRL)) {
        FURI_LOG_W(TAG, "invalid start: %02x", cmd[0]);
        cmd += 1;
        cmd_len -= 1;
        message.consumed += 1;
    }

    if(cmd_len > 12 && cmd[0] == SYNC && cmd[1] == CTRL) {
        uint8_t* ccid = cmd + 2;
        message.bMessageType = ccid[0];
        message.dwLength = *((uint32_t*)(ccid + 1));
        message.bSlot = ccid[5];
        message.bSeq = ccid[6];
        message.bStatus = ccid[7];
        message.bError = ccid[8];
        message.payload = ccid + 10;

        if(cmd_len < 2 + 10 + message.dwLength + 1) {
            // Incomplete
            return message.consumed;
        }
        message.consumed += 2 + 10 + message.dwLength + 1;

        if(seader_validate_lrc(cmd, 2 + 10 + message.dwLength + 1) == false) {
            FURI_LOG_W(
                TAG,
                "Invalid LRC.  Recv: %02x vs Calc: %02x",
                cmd[2 + 10 + message.dwLength],
                seader_calc_lrc(cmd, 2 + 10 + message.dwLength));
            // TODO: Should I respond with an error?
            return message.consumed;
        }

        //0306 81 00000000 0000 0200 01 87
        //0306 81 00000000 0000 0100 01 84
        if(message.bMessageType == CCID_MESSAGE_TYPE_RDR_TO_PC_SLOT_STATUS) {
            uint8_t status = (message.bStatus & BMICCSTATUS_MASK);
            if(status == 0 || status == 1) {
                seader_ccid_IccPowerOn(seader_uart, message.bSlot);
                return message.consumed;
            } else if(status == CCID_ICC_STATUS_NOT_PRESENT) {
                FURI_LOG_W(TAG, "No ICC is present [retries %d]", ccid_state->retries);
                if(ccid_state->retries-- > 1 && ccid_state->has_sam == false) {
                    furi_delay_ms(100);
                    seader_ccid_GetSlotStatus(seader_uart, ccid_state->retries % 2);
                } else {
                    if(seader_worker->callback) {
                        seader_worker->callback(
                            SeaderWorkerEventSamMissing, seader_worker->context);
                    }
                }
                return message.consumed;
            }
        }

        //0306 80 00000000 0001 42fe 00 38
        if(message.bStatus == 0x41 && message.bError == 0xfe) {
            FURI_LOG_W(TAG, "card probably upside down");
            ccid_state->has_sam = false;
            if(seader_worker->callback) {
                seader_worker->callback(SeaderWorkerEventSamMissing, seader_worker->context);
            }
            return message.consumed;
        }
        if(message.bStatus == 0x42 && message.bError == 0xfe) {
            FURI_LOG_W(TAG, "No card");
            if(seader_worker->callback) {
                seader_worker->callback(SeaderWorkerEventSamMissing, seader_worker->context);
            }
            return message.consumed;
        }
        if(message.bError != 0) {
            switch(message.bError) {
            case CCID_ERROR_ICC_MUTE:
                FURI_LOG_W(TAG, "CCID error ICC_MUTE");
                break;
            case CCID_ERROR_HW_ERROR:
                FURI_LOG_W(TAG, "CCID error HW_ERROR");
                break;
            default:
                FURI_LOG_W(TAG, "Unhandled CCID error %02x", message.bError);
                break;
            }
            message.consumed = cmd_len;
            if(seader_worker->callback) {
                seader_worker->callback(SeaderWorkerEventSamMissing, seader_worker->context);
            }
            return message.consumed;
        }

        if(message.bMessageType == CCID_MESSAGE_TYPE_RDR_TO_PC_PARAMETERS) {
            FURI_LOG_D(TAG, "Got Parameters");
            if(seader_uart->T == 1) {
                seader_t_1_set_IFSD(seader);
            } else {
                seader_worker_send_version(seader);
                if(seader_worker->callback) {
                    seader_worker->callback(SeaderWorkerEventSamPresent, seader_worker->context);
                }
            }
        } else if(message.bMessageType == CCID_MESSAGE_TYPE_RDR_TO_PC_DATA_BLOCK) {
            if(ccid_state->has_sam) {
                if(message.bSlot == ccid_state->sam_slot) {
                    if(seader_uart->T == 0) {
                        seader_worker_process_sam_message(
                            seader, message.payload, message.dwLength);
                    } else if(seader_uart->T == 1) {
                        seader_recv_t1(seader, &message);
                    }
                } else {
                    FURI_LOG_D(TAG, "Discarding message on non-sam slot");
                }
            } else {
                if(memcmp(SAM_ATR, message.payload, sizeof(SAM_ATR)) == 0) {
                    FURI_LOG_I(TAG, "SAM ATR!");
                    ccid_state->has_sam = true;
                    ccid_state->sam_slot = message.bSlot;
                    seader->ATR_len = sizeof(SAM_ATR);
                    memcpy(seader->ATR, message.payload, seader->ATR_len);
                    if(seader_uart->T == 0) {
                        seader_ccid_GetParameters(seader_uart);
                    } else if(seader_uart->T == 1) {
                        seader_ccid_SetParameters(seader, ccid_state->sam_slot);
                    }
                } else if(memcmp(SAM_ATR2, message.payload, sizeof(SAM_ATR2)) == 0) {
                    FURI_LOG_I(TAG, "SAM ATR2!");
                    ccid_state->has_sam = true;
                    ccid_state->sam_slot = message.bSlot;
                    seader->ATR_len = sizeof(SAM_ATR);
                    memcpy(seader->ATR, message.payload, seader->ATR_len);
                    // I don't have an ATR2 to test with
                    seader_ccid_GetParameters(seader_uart);
                } else if(memcmp(SAM_ATR3, message.payload, sizeof(SAM_ATR3)) == 0) {
                    FURI_LOG_I(TAG, "SAM ATR3!");
                    ccid_state->has_sam = true;
                    ccid_state->sam_slot = message.bSlot;
                    seader->ATR_len = sizeof(SAM_ATR);
                    memcpy(seader->ATR, message.payload, seader->ATR_len);
                    if(seader_uart->T == 0) {
                        seader_ccid_GetParameters(seader_uart);
                    } else if(seader_uart->T == 1) {
                        seader_ccid_SetParameters(seader, ccid_state->sam_slot);
                    }
                } else {
                    FURI_LOG_W(TAG, "Unknown ATR");
                    if(seader_worker->callback) {
                        seader_worker->callback(SeaderWorkerEventSamWrong, seader_worker->context);
                    }
                }
            }
        } else {
            FURI_LOG_W(TAG, "Unhandled CCID message type %02x", message.bMessageType);
        }
    }

    return message.consumed;
}
