#include "t_1_logic.h"
#include "lrc.h"

#include <string.h>

#ifdef SEADER_HOST_TEST
#include "lib/host_tests/bit_buffer.h"
#else
#include <lib/toolbox/bit_buffer.h>
#endif

uint8_t seader_t1_next_pcb(uint8_t current_pcb) {
    return current_pcb ^ SEADER_T1_PCB_SEQUENCE_BIT;
}

bool seader_t1_apdu_in_scratchpad(const uint8_t* tx_buf, size_t tx_buf_size, const uint8_t* apdu) {
    return apdu >= tx_buf + 3 && apdu < tx_buf + tx_buf_size;
}

size_t seader_t1_chunk_length(size_t total_length, size_t offset, uint8_t ifsc) {
    size_t remaining = total_length - offset;
    if(ifsc == 0) {
        ifsc = SEADER_T1_IFS_DEFAULT;
    }
    return remaining > ifsc ? ifsc : remaining;
}

bool seader_t1_validate_block(const uint8_t* block, size_t dw_length) {
    if(!block || dw_length < 4) {
        return false;
    }

    uint8_t len = block[2];
    if(dw_length != (size_t)(len + 4)) {
        return false;
    }

    return seader_validate_lrc((uint8_t*)block, dw_length);
}

void seader_t1_reset_link_state(SeaderT1State* t1) {
    if(!t1) {
        return;
    }

    t1->send_pcb = SEADER_T1_PCB_SEQUENCE_BIT;
    t1->recv_pcb = 0x00;

    if(t1->tx_buffer != NULL) {
        bit_buffer_free(t1->tx_buffer);
    }
    t1->tx_buffer = NULL;
    t1->tx_buffer_offset = 0;
    t1->last_tx_len = 0;

    if(t1->rx_buffer != NULL) {
        bit_buffer_free(t1->rx_buffer);
    }
    t1->rx_buffer = NULL;
}

SeaderT1Action seader_t1_handle_block(
    SeaderT1State* t1,
    const uint8_t* payload,
    size_t dw_length,
    uint8_t** apdu_out,
    size_t* apdu_len_out) {
    if(!seader_t1_validate_block(payload, dw_length)) {
        return SeaderT1ActionSendNak;
    }

    uint8_t rPCB = payload[1];
    uint8_t LEN = payload[2];

    if((rPCB & SEADER_T1_PCB_S_BLOCK) == SEADER_T1_PCB_S_BLOCK) {
        uint8_t type = rPCB & SEADER_T1_S_BLOCK_TYPE_MASK;
        bool is_response = (rPCB & SEADER_T1_S_BLOCK_RESPONSE_BIT) != 0;

        /* The regression suite expects malformed S-block lengths to be rejected before payload use. */
        if((type == SEADER_T1_S_BLOCK_IFS || type == SEADER_T1_S_BLOCK_WTX) && LEN != 1) {
            return SeaderT1ActionSendNak;
        }
        if((type == SEADER_T1_S_BLOCK_RESYNCH || type == SEADER_T1_S_BLOCK_ABORT) && LEN != 0) {
            return SeaderT1ActionSendNak;
        }

        switch(type) {
        case SEADER_T1_S_BLOCK_IFS:
            if(is_response) {
                /* Only accept the pending IFSD we negotiated so a stray response cannot retune framing. */
                uint8_t ifs = payload[3];
                if(ifs == 0 || ifs > SEADER_T1_IFS_MAX) {
                    return SeaderT1ActionSendNak;
                }
                if(t1->ifsd_pending != 0 && ifs != t1->ifsd_pending) {
                    return SeaderT1ActionSendNak;
                }
                t1->ifsd = ifs;
                t1->ifsd_pending = 0;
                return SeaderT1ActionSendVersion;
            } else {
                /* Card-initiated IFS updates our outbound chunk size and must be echoed back verbatim. */
                uint8_t ifs = payload[3];
                if(ifs == 0 || ifs > SEADER_T1_IFS_MAX) {
                    return SeaderT1ActionSendNak;
                }
                t1->ifsc = ifs;
                *apdu_out = (uint8_t*)payload + 3;
                *apdu_len_out = 1;
                return SeaderT1ActionSendIFSResponse;
            }
        case SEADER_T1_S_BLOCK_WTX:
            if(!is_response) {
                /* WTX requests do not advance link state; they only request a matching S(WTX response). */
                *apdu_out = (uint8_t*)payload + 3;
                *apdu_len_out = LEN;
                return SeaderT1ActionSendWTXResponse;
            }
            break;
        case SEADER_T1_S_BLOCK_RESYNCH:
            if(!is_response) {
                /* RESYNCH resets sequence tracking before we send the response frame. */
                return SeaderT1ActionSendResynchResponse;
            }
            break;
        case SEADER_T1_S_BLOCK_ABORT:
            return SeaderT1ActionError;
        }
        return SeaderT1ActionNone;
    }

    if(rPCB == t1->recv_pcb) {
        if(LEN > t1->ifsd) {
            return SeaderT1ActionSendNak;
        }
        t1->recv_pcb = seader_t1_next_pcb(t1->recv_pcb);

        if(t1->tx_buffer != NULL) {
            bit_buffer_free(t1->tx_buffer);
            t1->tx_buffer = NULL;
            t1->tx_buffer_offset = 0;
            t1->last_tx_len = 0;
        }

        if(t1->rx_buffer != NULL) {
            bit_buffer_append_bytes(t1->rx_buffer, payload + 3, LEN);
            *apdu_out = (uint8_t*)bit_buffer_get_data(t1->rx_buffer);
            *apdu_len_out = bit_buffer_get_size_bytes(t1->rx_buffer);
            return SeaderT1ActionDeliverAPDU;
        }

        if(LEN == 0) {
            return SeaderT1ActionNone;
        }

        *apdu_out = (uint8_t*)payload + 3;
        *apdu_len_out = LEN;
        return SeaderT1ActionDeliverAPDU;

    } else if(rPCB == (t1->recv_pcb | SEADER_T1_PCB_I_BLOCK_MORE)) {
        if(LEN > t1->ifsd) {
            return SeaderT1ActionSendNak;
        }
        t1->recv_pcb = seader_t1_next_pcb(t1->recv_pcb);
        if(t1->rx_buffer == NULL) {
            t1->rx_buffer = bit_buffer_alloc(512);
        }
        bit_buffer_append_bytes(t1->rx_buffer, payload + 3, LEN);
        return SeaderT1ActionSendAck;

    } else if((rPCB & SEADER_T1_PCB_R_BLOCK) == SEADER_T1_PCB_R_BLOCK) {
        uint8_t r_seq = (rPCB & SEADER_T1_R_BLOCK_SEQUENCE_MASK) >> 4;
        uint8_t next_i_seq = (t1->send_pcb ^ SEADER_T1_PCB_SEQUENCE_BIT) >> 6;
        uint8_t err = rPCB & 0x0F;

        if(err == 0 && r_seq == next_i_seq) {
            /* Matching R-block ACKs advance a chained transmit if more buffered data remains. */
            if(t1->tx_buffer != NULL) {
                if(t1->tx_buffer_offset < bit_buffer_get_size_bytes(t1->tx_buffer)) {
                    return SeaderT1ActionSendMoreData;
                }
                return SeaderT1ActionNone;
            }
            return SeaderT1ActionNone;
        } else {
            if(t1->tx_buffer != NULL) {
                /* NACK retransmits the previous chunk by rewinding exactly last_tx_len bytes. */
                if(t1->last_tx_len == 0 || t1->last_tx_len > t1->tx_buffer_offset) {
                    return SeaderT1ActionError;
                }
                t1->tx_buffer_offset -= t1->last_tx_len;
                t1->send_pcb ^= SEADER_T1_PCB_SEQUENCE_BIT;
                return SeaderT1ActionRetransmit;
            }
            return SeaderT1ActionError;
        }
    }

    return SeaderT1ActionError;
}
