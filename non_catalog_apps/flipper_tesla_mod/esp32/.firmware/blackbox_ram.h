#pragma once
/*
 * blackbox_ram.h — RAM-backend save memory discipline for the black-box
 * recorder (blackbox.cpp, #124 / #191). Header-only and Arduino-free, so
 * test/test_esp32_core.cpp exercises the bookkeeping on the host.
 *
 * The volatile RAM backend (min_spiffs boards, e.g. a generic ESP32 + MCP2515)
 * keeps the single most-recent event in a heap buffer the dashboard downloads.
 * Saving one used to cost TWO windows of RAM at once: the window was copied out
 * of the ring into a scratch buffer, then copied AGAIN into the slot while the
 * scratch was still held, and only then was the scratch freed. On a no-PSRAM
 * board with ~83-87 KB free a ~57 KB window needs ~114 KB that way, so the
 * allocation fails and the event is dropped — the dashboard showed a capture go
 * active and then an empty list with "0 saved" (#191).
 *
 * bb_ram_reserve() keeps the peak at ONE window. It frees the previous event's
 * buffer FIRST, then allocates the new one, and the caller copies the ring
 * window straight into it (no intermediate scratch copy). If even one window
 * can't be had, the slot is left empty and the caller reports a low-memory save
 * failure to the dashboard instead of silently dropping the event.
 *
 * Freeing the old buffer before the new allocation means a save that runs out
 * of memory also discards the previous event — the same as the old code, and
 * unavoidable if the peak is to stay at one window.
 */

#include <stddef.h>
#include <stdint.h>

// A single owned heap buffer: the most-recent RAM event's frozen frames. It
// tracks its own size so a reserve can free the old one before taking the new.
typedef struct {
    void*    buf;    // owned; NULL when empty
    uint32_t bytes;  // size of buf (0 when empty)
} BBRamSlot;

// Free the previous buffer FIRST, then allocate `want` bytes, so a save never
// holds two windows at once (#191). Returns the new buffer, or NULL on OOM; on
// OOM the slot is left empty (the previous event is gone either way). A `want`
// of 0 is rounded up to 1 so an empty window still owns a valid buffer. alloc()
// and freefn() wrap malloc/free; the host tests pass a capped allocator.
static inline void* bb_ram_reserve(BBRamSlot* slot, size_t want,
                                   void* (*alloc)(size_t), void (*freefn)(void*)) {
    if (slot->buf) { freefn(slot->buf); slot->buf = NULL; slot->bytes = 0; }
    size_t n = want ? want : 1;
    void* p = alloc(n);
    if (!p) return NULL;
    slot->buf = p;
    slot->bytes = (uint32_t)n;
    return p;
}
