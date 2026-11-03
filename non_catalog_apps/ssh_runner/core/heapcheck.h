/* Start-up memory check (pure C, host-tested in tests/test_heapcheck.c).
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * A FAP runs from the Flipper's system heap. When that heap is short (other apps, RPC
 * sessions, a long uptime), the app's own allocations can fail half-way and the Flipper
 * reboots ("out of memory"). The app checks before it allocates anything
 * and refuses to start with a stock dialog instead. */
#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    size_t need; /* bytes the app allocates after entry, at its peak (measured on the device) */
    size_t margin; /* left for the system and other services while the app runs */
    size_t biggest; /* the largest single allocation: needs one free block that large */
} HeapNeeds;

/* free_now: system free heap at entry; max_block: largest free block. */
bool heap_start_ok(size_t free_now, size_t max_block, const HeapNeeds* needs);
