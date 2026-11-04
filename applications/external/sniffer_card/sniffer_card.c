#include "sniffer_card.h"

#include <bit_lib/bit_lib.h>
#include <nfc/helpers/crypto1.h>
#include <nfc/protocols/mf_classic/mf_classic.h>

/** Steps to run the 16-bit PRNG state forward to recover the nonce the reader saw.
 *
 * Matches Proxmark3's `prng_successor(NT0, 31)` in `mfc_supercard_gen2_recovery()`. The card
 * stores the state the generator had reached, not its output.
 */
#define SNIFFER_CARD_NT_ADVANCE 31

bool sniffer_trace_parse_gen2(const uint8_t* raw, SnifferTrace* out) {
    furi_assert(raw);
    furi_assert(out);

    const uint8_t key_type = raw[4];
    if(key_type != SNIFFER_CARD_AUTH_KEY_A && key_type != SNIFFER_CARD_AUTH_KEY_B) return false;

    const uint32_t nr = bit_lib_bytes_to_num_be(raw + 8, 4);
    const uint32_t ar = bit_lib_bytes_to_num_be(raw + 12, 4);
    // An empty slot leaves both reader words zero. They are encrypted, so a real capture has no
    // reason to produce either - a reader that genuinely sent zeroes would still have them
    // keystreamed into something else.
    if(nr == 0 && ar == 0) return false;

    memcpy(out->cuid, raw, sizeof(out->cuid));
    out->key_type = key_type;
    out->block = raw[5];
    out->nt = crypto1_prng_successor(bit_lib_bytes_to_num_be(raw + 6, 2), SNIFFER_CARD_NT_ADVANCE);
    out->nr = nr;
    out->ar = ar;

    return true;
}

uint32_t sniffer_trace_get_cuid(const SnifferTrace* trace) {
    furi_assert(trace);

    return bit_lib_bytes_to_num_be(trace->cuid, sizeof(trace->cuid));
}

uint8_t sniffer_trace_get_sector(const SnifferTrace* trace) {
    furi_assert(trace);
    // Handles the 4K geometry, where the last eight sectors hold sixteen blocks rather than four.
    return mf_classic_get_sector_by_block(trace->block);
}

bool sniffer_trace_same_key(const SnifferTrace* a, const SnifferTrace* b) {
    furi_assert(a);
    furi_assert(b);

    // The card is part of the question. mfkey32 seeds the cipher with the UID, so two
    // authentications made against different UIDs cannot be intersected however well their sector
    // and key type match - the line would be arithmetically impossible and MFKey would spend
    // minutes proving it. One read can hold captures from two UIDs: these cards keep older
    // captures when a new one lands, so reprogramming between taps leaves both on the card.
    return sniffer_trace_get_cuid(a) == sniffer_trace_get_cuid(b) && a->key_type == b->key_type &&
           sniffer_trace_get_sector(a) == sniffer_trace_get_sector(b);
}
