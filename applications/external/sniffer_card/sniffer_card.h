/**
 * @file sniffer_card.h
 * @brief What a Super Sniffer card stores, and how to turn it into MFKey32 nonces.
 */
#pragma once

#include <furi.h>
#include <stdint.h>

/** Trace slots read off the card.
 *
 * The vendor's own Android app (`com.fnstar.sniffercard`) reads pages 0..7; Proxmark3's
 * `hf mf supercard` reads 0..6 and calls seven the maximum, and the AliExpress listing says the
 * card holds seven. The app's built-in sample set resolves it: its eighth record is the card's
 * own UID followed by twelve zero bytes, so page 7 is not a trace slot on that card - but reading
 * it costs one frame and sniffer_trace_parse_gen2() rejects it on content rather than on index,
 * so a revision that does use eight slots is not silently truncated.
 */
#define SNIFFER_CARD_TRACE_SLOTS 8

/** Bytes in one Gen2 trace slot, which is one `30 <page>` response without its CRC. */
#define SNIFFER_CARD_TRACE_SIZE 16

/** Auth command byte stored in a trace, as the reader sent it. */
#define SNIFFER_CARD_AUTH_KEY_A 0x60
#define SNIFFER_CARD_AUTH_KEY_B 0x61

/** One authentication the card captured from a reader.
 *
 * Deliberately not a copy of the on-card layout: the three revisions of this card store the same
 * six fields at different offsets and different widths (Gen2 keeps `nt` as the 16-bit PRNG state,
 * Furui keeps it full width, and Furui swaps the block and command bytes), so the layout belongs
 * in each revision's parser and everything above this line works on the common form.
 *
 * @c nt is always the full 32-bit tag nonce here, expanded if the card stored it short.
 */
typedef struct {
    uint8_t cuid[4]; /**< The 4 bytes of UID that take part in authentication. */
    uint8_t key_type; /**< SNIFFER_CARD_AUTH_KEY_A or _B, as the reader asked. */
    uint8_t block; /**< Block the reader authenticated against, not the sector. */
    uint32_t nt; /**< Tag nonce, full width. */
    uint32_t nr; /**< Reader nonce, encrypted. */
    uint32_t ar; /**< Reader answer, encrypted. */
} SnifferTrace;

/** Read one Gen2 trace slot.
 *
 * Gen2 stores `nt` as the 16-bit Crypto1 PRNG state rather than the nonce the reader saw, which
 * is lossless - the generator is a 16-bit LFSR - but has to be run forward 31 steps to get the
 * nonce back. Proxmark3 does the same thing in `mfc_supercard_gen2_recovery()`
 * (`client/src/cmdhfmf.c`), and the vendor app's JNI shim hands its native mfkey32 the same two
 * bytes rather than four.
 *
 * Validity is judged on content, because an unused slot is not distinguishable by index: the
 * command byte has to be one a reader would actually send, and a slot that captured nothing
 * leaves the reader's two words zero.
 *
 * @param[in] raw SNIFFER_CARD_TRACE_SIZE bytes, CRC already stripped
 * @param[out] out filled only when this returns true
 * @return true when the slot holds a real capture
 */
bool sniffer_trace_parse_gen2(const uint8_t* raw, SnifferTrace* out);

/** The capture's UID as the one number the nonce log carries it as. */
uint32_t sniffer_trace_get_cuid(const SnifferTrace* trace);

/** Sector a trace's block belongs to. */
uint8_t sniffer_trace_get_sector(const SnifferTrace* trace);

/** Whether two traces could have been made with the same key.
 *
 * Same card, same sector, same key type. Not same block: one key covers every block in its
 * sector, and a reader walking a sector authenticates against whichever block it wants, so
 * pairing on the block throws away most usable pairs - Proxmark3 compares sectors here for the
 * same reason.
 *
 * "Could have been": a reader may present several keys to one sector, the real one among decoys,
 * so two traces passing this test still need not share a key. Which ones do is what mfkey32
 * answers, not something that can be decided here.
 */
bool sniffer_trace_same_key(const SnifferTrace* a, const SnifferTrace* b);
