#pragma once

#include <furi.h>
#include <stdint.h>

// Append the indices of the set bits in `bitmap` (scanned over [0, count)) to `out`, ascending, as
// space-separated decimals -- e.g. "5 6 7 ". Bit N is index N. These bitmaps mark which block/page
// failed to write, and this list is shown on the Gen2, USCUID-UL and ISO15693 partial-result
// screens. If `max_shown` > 0 and that many indices have been appended, append "..." and stop;
// pass 0 for no cap.
void nfc_magic_partial_details_append_indices(
    FuriString* out,
    const uint8_t* bitmap,
    uint16_t count,
    uint16_t max_shown);

// Is any bit set in `bitmap` over [0, count)? The same range convention as append_indices, so a caller
// can ask "will that print anything" over exactly the range it is about to print, rather than tallying
// bits itself and comparing to zero, which is an existence test written as a count.
bool nfc_magic_partial_details_any_index(const uint8_t* bitmap, uint16_t count);
