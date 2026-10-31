#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Offers become less frequent as successful opens accumulate. An exhausted
 * counter must not wrap back to the first-open offer. */
static inline bool flippass_open_offer_due(uint32_t completed_opens, uint32_t first_offer) {
    if(completed_opens == UINT32_MAX) return false;
    const uint32_t next = completed_opens + 1U;
    return next >= first_offer && (next & (next - 1U)) == 0U;
}
