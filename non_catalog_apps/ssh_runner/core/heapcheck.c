/* Start-up memory check (see heapcheck.h). SPDX-License-Identifier: GPL-3.0-only */
#include "heapcheck.h"

bool heap_start_ok(size_t free_now, size_t max_block, const HeapNeeds* needs) {
    if(free_now < needs->need + needs->margin) return false;
    /* enough in total but too fragmented for the biggest buffer is the same failure */
    return max_block >= needs->biggest;
}
