#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    SEADER_T1_PCB_I_BLOCK_MORE = 0x20,
    SEADER_T1_PCB_SEQUENCE_BIT = 0x40,
    SEADER_T1_PCB_R_BLOCK = 0x80,
    SEADER_T1_PCB_S_BLOCK = 0xC0,
    SEADER_T1_R_BLOCK_SEQUENCE_MASK = 0x10,
    SEADER_T1_S_BLOCK_RESPONSE_BIT = 0x20,
    SEADER_T1_S_BLOCK_TYPE_MASK = 0x1F,
    SEADER_T1_S_BLOCK_RESYNCH = 0x00,
    SEADER_T1_S_BLOCK_IFS = 0x01,
    SEADER_T1_S_BLOCK_ABORT = 0x02,
    SEADER_T1_S_BLOCK_WTX = 0x03,
} SeaderT1Constant;

typedef enum {
    SeaderT1ActionNone,
    SeaderT1ActionDeliverAPDU,
    SeaderT1ActionSendAck,
    SeaderT1ActionSendIFSResponse,
    SeaderT1ActionSendWTXResponse,
    SeaderT1ActionSendResynchResponse,
    SeaderT1ActionSendVersion,
    SeaderT1ActionSendMoreData,
    SeaderT1ActionRetransmit,
    SeaderT1ActionSendNak,
    SeaderT1ActionError,
} SeaderT1Action;

typedef struct BitBuffer BitBuffer;

typedef struct {
    /* ICC information field size (card receive capability; host transmit chunking). */
    uint8_t ifsc;
    /* IFD information field size (host receive capability; card transmit chunking). */
    uint8_t ifsd;
    /* Pending IFSD value proposed through S(IFS request); 0 means none pending. */
    uint8_t ifsd_pending;
    /* Host NAD used for T=1 block exchange. */
    uint8_t nad;
    /* Last transmit I-block sequence bit. */
    uint8_t send_pcb;
    /* Last receive I-block sequence bit. */
    uint8_t recv_pcb;
    BitBuffer* tx_buffer;
    size_t tx_buffer_offset;
    /* Length of the last transmitted chunk, used to roll back on NACK. */
    size_t last_tx_len;
    BitBuffer* rx_buffer;
} SeaderT1State;

enum {
    SEADER_T1_IFS_DEFAULT = 32,
    SEADER_T1_IFS_MAX = 254,
};

uint8_t seader_t1_next_pcb(uint8_t current_pcb);
bool seader_t1_apdu_in_scratchpad(const uint8_t* tx_buf, size_t tx_buf_size, const uint8_t* apdu);
size_t seader_t1_chunk_length(size_t total_length, size_t offset, uint8_t ifsc);
bool seader_t1_validate_block(const uint8_t* block, size_t dw_length);
void seader_t1_reset_link_state(SeaderT1State* t1);

SeaderT1Action seader_t1_handle_block(
    SeaderT1State* t1,
    const uint8_t* payload,
    size_t dw_length,
    uint8_t** apdu_out,
    size_t* apdu_len_out);
