#pragma once

#include "mfp_poller.h"
#include <furi.h>

/* Decoded card identification data, similar to what PM3 "hf mfp info" shows.
 * Fills a FuriString with a multi-line human-readable description. */

/** Return human-readable manufacturer name for NXP-style manufacturer ID byte.
 *  Returns a static string; caller must not free. */
const char* mfp_card_info_manufacturer_name(uint8_t vendor_id);

/** Return SAK-based card type hint (e.g. "ISO 14443-4 compliant"). */
const char* mfp_card_info_sak_type(uint8_t sak);

/** Append a full decoded card report to `out`, including:
 *  - UID + manufacturer
 *  - ATQA, SAK
 *  - ATS interface bytes (TL, T0, TA1, TB1, TC1)
 *  - ATS historical bytes (category, MFP type, memory size, generation)
 *  - Fingerprint / security level hint
 *  - Sector 0 block 0 manufacturer data (BCC, SAK, ATQA from block)
 */
void mfp_card_info_format(
    FuriString* out,
    const MfpVersion* version,
    uint8_t sak,
    const uint8_t atqa[2],
    const uint8_t* ats_bytes,
    uint8_t ats_len,
    const uint8_t* block0);
