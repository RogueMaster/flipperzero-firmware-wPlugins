#include "seos_iso14443_4.h"

#include <string.h>

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

/* ---- the card's side ---- */

/* The request that starts the protocol, and the mask for the card identifier
 * in the byte after it. */
#define RATS           0xe0
#define RATS_CID       0x0f
/* A request to change speed starts with 1101 and carries the identifier. */
#define PPS_MASK       0xf0
#define PPS_VALUE      0xd0
/* The low six bits of a request for more time are the multiplier. */
#define WTX_MULTIPLIER 0x3f

void seos_iso14443_4_card_init(SeosIso14443_4Card* card, const uint8_t* ats, size_t ats_len) {
    memset(card, 0, sizeof(*card));
    card->ats = ats;
    card->ats_len = ats_len;
}

void seos_iso14443_4_card_drop_command(SeosIso14443_4Card* card) {
    card->command_len = 0;
}

void seos_iso14443_4_card_answered(SeosIso14443_4Card* card, const uint8_t* block, size_t len) {
    card->command_len = 0;
    if(len > sizeof(card->answer)) return;
    memcpy(card->answer, block, len);
    card->answer_len = len;
}

static SeosIso14443_4CardResult
    card_reply(const uint8_t* data, size_t len, const uint8_t** reply, size_t* reply_len) {
    *reply = data;
    *reply_len = len;
    return len ? SeosIso14443_4CardReply : SeosIso14443_4CardSilent;
}

SeosIso14443_4CardResult seos_iso14443_4_card_receive(
    SeosIso14443_4Card* card,
    const uint8_t* frame,
    size_t frame_len,
    const uint8_t** reply,
    size_t* reply_len) {
    *reply = NULL;
    *reply_len = 0;
    if(frame_len < 1) return SeosIso14443_4CardSilent;
    uint8_t pcb = frame[0];

    /* The two requests that come before any block would both be taken for
     * S-blocks by their top bits, so they are looked for first. */
    if(pcb == RATS && frame_len >= 2) {
        card->cid = frame[1] & RATS_CID;
        card->command_len = 0;
        return card_reply(card->ats, card->ats_len, reply, reply_len);
    }
    if((pcb & PPS_MASK) == PPS_VALUE) {
        /* Accepted by sending its first byte back. */
        card->reply[0] = pcb;
        return card_reply(card->reply, 1, reply, reply_len);
    }

    bool has_cid = (pcb & SEOS_ISO14443_4_PCB_CID) != 0;
    size_t n = 0;

    switch(seos_iso14443_4_classify(pcb)) {
    case SeosIso14443_4BlockI: {
        /* A piece that would not fit is left out rather than written past the
         * end; a frame shorter than its own header carries nothing. */
        size_t header_len = seos_iso14443_4_header_len(pcb);
        if(header_len <= frame_len) {
            size_t piece = frame_len - header_len;
            if(card->command_len + piece <= sizeof(card->command)) {
                memcpy(card->command + card->command_len, frame + header_len, piece);
                card->command_len += piece;
            }
        }

        if(seos_iso14443_4_is_chaining(pcb)) {
            /* More to come: acknowledged with an R-block carrying the same
             * block number, which is a refusal without the bit that refuses. */
            card->reply[n++] = seos_iso14443_4_nak_pcb(pcb) & (uint8_t)~SEOS_ISO14443_4_PCB_NAK;
            if(has_cid) card->reply[n++] = card->cid;
            return card_reply(card->reply, n, reply, reply_len);
        }

        card->reply[n++] = seos_iso14443_4_response_pcb(pcb, false);
        if(has_cid) card->reply[n++] = card->cid;
        *reply = card->reply;
        *reply_len = n;
        return SeosIso14443_4CardCommand;
    }

    case SeosIso14443_4BlockR:
        /* Asks for the last answer again. */
        return card_reply(card->answer, card->answer_len, reply, reply_len);

    case SeosIso14443_4BlockS:
        /* Sent back as it came. Ending the exchange also ends any command in
         * progress; asking for more time carries its multiplier back. */
        card->reply[n++] = pcb;
        if(has_cid) card->reply[n++] = card->cid;
        if(seos_iso14443_4_is_deselect(pcb)) {
            card->command_len = 0;
        } else {
            size_t header_len = seos_iso14443_4_header_len(pcb);
            if(header_len < frame_len) card->reply[n++] = frame[header_len] & WTX_MULTIPLIER;
        }
        return card_reply(card->reply, n, reply, reply_len);

    default:
        return SeosIso14443_4CardSilent;
    }
}
