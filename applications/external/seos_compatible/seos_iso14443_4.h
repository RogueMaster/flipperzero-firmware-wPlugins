#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Block framing per ISO 14443-4.
 *
 * A block begins with a protocol control byte. Depending on bits in that PCB
 * it may be followed by a card identifier and a node address, one byte each,
 * before the payload.
 *
 * Separate from the listener so the arithmetic and the block rules can be
 * tested against short and malformed frames.
 */

/* PCB bits. The block number is the low bit of an I-block or an R-block. */
#define SEOS_ISO14443_4_PCB_BLOCK_NUMBER 0x01
#define SEOS_ISO14443_4_PCB_NAD          0x04
#define SEOS_ISO14443_4_PCB_CID          0x08
#define SEOS_ISO14443_4_PCB_CHAINING     0x10
#define SEOS_ISO14443_4_PCB_NAK          0x10

typedef enum {
    SeosIso14443_4BlockI,
    SeosIso14443_4BlockR,
    SeosIso14443_4BlockS,
    SeosIso14443_4BlockUnknown,
} SeosIso14443_4BlockType;

SeosIso14443_4BlockType seos_iso14443_4_classify(uint8_t pcb);

/* An I-block with more to follow. */
bool seos_iso14443_4_is_chaining(uint8_t pcb);

/* An R-block that did not acknowledge, asking for the last block again. */
bool seos_iso14443_4_is_nak(uint8_t pcb);

/* An S-block asking to end the exchange, as opposed to asking for more time. */
bool seos_iso14443_4_is_deselect(uint8_t pcb);

/* Bytes of header a block with this PCB carries, the PCB included. */
size_t seos_iso14443_4_header_len(uint8_t pcb);

/* Reports where the command starts in a received block and how long it is.
 *
 * False if the frame is shorter than the header its PCB describes, or holds a
 * header and nothing else. Either case would underflow the remaining
 * length. */
bool seos_iso14443_4_apdu_bounds(const uint8_t* data, size_t len, size_t* offset, size_t* apdu_len);

/* PCB for a reply to `rx_pcb`.
 *
 * A card answers with the block number it was sent, so the two stay in step.
 * An I-block is answered with an I-block, an S-block with the same S-block,
 * and an R-block with an I-block carrying whatever is being resent. */
uint8_t seos_iso14443_4_response_pcb(uint8_t rx_pcb, bool chaining);

/* PCB for an R-block refusing the block just received, keeping its block
 * number so the other end can tell which one is meant. */
uint8_t seos_iso14443_4_nak_pcb(uint8_t rx_pcb);

/* Bytes of payload a frame of `frame_size_max` can carry.
 *
 * A block spends one byte on its PCB, up to two more on a card identifier and
 * a node address, and two on the checksum at the end. Returns 0 for a frame
 * with no room for a payload, which the caller should treat as no
 * information. */
size_t seos_iso14443_4_payload_budget(uint16_t frame_size_max);

#ifdef __cplusplus
}
#endif
