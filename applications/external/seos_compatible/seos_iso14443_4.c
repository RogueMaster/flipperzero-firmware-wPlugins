#include "seos_iso14443_4.h"

/* Block type lives in the top bits of the PCB: 00 for an I-block, 101 for an
 * R-block, 11 for an S-block. */
#define PCB_TYPE_I_MASK  0xc0
#define PCB_TYPE_I_VALUE 0x00
#define PCB_TYPE_R_MASK  0xe0
#define PCB_TYPE_R_VALUE 0xa0
#define PCB_TYPE_S_MASK  0xc0
#define PCB_TYPE_S_VALUE 0xc0

/* Bit 2 of every PCB is set. */
#define PCB_RESERVED 0x02

/* An S-block asking for more time sets both bits; deselect clears them. */
#define PCB_S_WTX 0x30

SeosIso14443_4BlockType seos_iso14443_4_classify(uint8_t pcb) {
    if((pcb & PCB_TYPE_R_MASK) == PCB_TYPE_R_VALUE) return SeosIso14443_4BlockR;
    if((pcb & PCB_TYPE_S_MASK) == PCB_TYPE_S_VALUE) return SeosIso14443_4BlockS;
    if((pcb & PCB_TYPE_I_MASK) == PCB_TYPE_I_VALUE) return SeosIso14443_4BlockI;
    return SeosIso14443_4BlockUnknown;
}

bool seos_iso14443_4_is_chaining(uint8_t pcb) {
    return seos_iso14443_4_classify(pcb) == SeosIso14443_4BlockI &&
           (pcb & SEOS_ISO14443_4_PCB_CHAINING) != 0;
}

bool seos_iso14443_4_is_nak(uint8_t pcb) {
    return seos_iso14443_4_classify(pcb) == SeosIso14443_4BlockR &&
           (pcb & SEOS_ISO14443_4_PCB_NAK) != 0;
}

bool seos_iso14443_4_is_deselect(uint8_t pcb) {
    return seos_iso14443_4_classify(pcb) == SeosIso14443_4BlockS && (pcb & PCB_S_WTX) != PCB_S_WTX;
}

size_t seos_iso14443_4_header_len(uint8_t pcb) {
    size_t len = 1;

    if(pcb & SEOS_ISO14443_4_PCB_CID) len++;
    /* Only an I-block carries a node address; the bit means nothing else. */
    if(seos_iso14443_4_classify(pcb) == SeosIso14443_4BlockI && (pcb & SEOS_ISO14443_4_PCB_NAD))
        len++;

    return len;
}

bool seos_iso14443_4_apdu_bounds(const uint8_t* data, size_t len, size_t* offset, size_t* apdu_len) {
    /* The PCB must be present before the rest can be interpreted. */
    if(len < 1) return false;

    size_t header_len = seos_iso14443_4_header_len(data[0]);

    /* Compared rather than subtracted: len - header_len would underflow for a
     * frame shorter than its header. */
    if(len <= header_len) return false;

    *offset = header_len;
    *apdu_len = len - header_len;
    return true;
}

uint8_t seos_iso14443_4_response_pcb(uint8_t rx_pcb, bool chaining) {
    uint8_t block_number = rx_pcb & SEOS_ISO14443_4_PCB_BLOCK_NUMBER;
    uint8_t cid = rx_pcb & SEOS_ISO14443_4_PCB_CID;

    /* An S-block is answered with the same S-block. */
    if(seos_iso14443_4_classify(rx_pcb) == SeosIso14443_4BlockS) return rx_pcb;

    /* Everything else is answered with an I-block. A node address is not sent
     * back: the reply carries no addressing of its own. */
    uint8_t pcb = PCB_TYPE_I_VALUE | PCB_RESERVED | block_number | cid;
    if(chaining) pcb |= SEOS_ISO14443_4_PCB_CHAINING;
    return pcb;
}

uint8_t seos_iso14443_4_nak_pcb(uint8_t rx_pcb) {
    uint8_t block_number = rx_pcb & SEOS_ISO14443_4_PCB_BLOCK_NUMBER;
    uint8_t cid = rx_pcb & SEOS_ISO14443_4_PCB_CID;

    return PCB_TYPE_R_VALUE | PCB_RESERVED | SEOS_ISO14443_4_PCB_NAK | block_number | cid;
}

/* PCB, a card identifier and a node address, and the two checksum bytes. */
#define FRAME_OVERHEAD 5

size_t seos_iso14443_4_payload_budget(uint16_t frame_size_max) {
    if(frame_size_max <= FRAME_OVERHEAD) return 0;
    return (size_t)frame_size_max - FRAME_OVERHEAD;
}
