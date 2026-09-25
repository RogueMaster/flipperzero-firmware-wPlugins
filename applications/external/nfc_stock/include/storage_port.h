#pragma once

#include "stock.h"
#include "store_status.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Outcome of trying to read the whole stock DB file as a set of StockItem
 * records. */
typedef enum {
    StockReadOk = 0, /* whole file is complete StockItem records (possibly zero of them) */
    StockReadFraming, /* size is not a multiple of sizeof(StockItem): content is
                        corrupt */
    StockReadTooLarge, /* well-formed, aligned size, but past the safety cap:
                      * confirmed bad, safe to quarantine like any other
                      * unreadable data */
    StockReadBlocked, /* allocation failed, or a read/stat error despite a
                      * well-formed,  in-cap size -- state genuinely unknown,
                      * never guess */
} StockReadOutcome;

/**
 * Minimal file operations the stock DB recovery algorithm needs, injected so it
 * can run against real Storage on-device or an in-memory fake on host. `rename`
 * must reproduce the firmware's own semantics: remove `dst` (ignored if
 * absent), copy `src` to `dst`, then remove `src` -- NOT atomic. A crash
 * between those steps can leave `dst` missing or partially written while `src`
 * is still fully intact.
 */
typedef struct {
    void* ctx;
    /* A save whose new content would exceed this many bytes is refused before
   * writing anything; the DB has no header, so this is the only cap available.
   */
    size_t max_bytes;
    StoreStat (*stat)(void* ctx, const char* path);
    bool (*exists)(void* ctx, const char* path);
    /** On StockReadOk, `*out_items` is malloc'd (caller frees) and holds
   * `*out_count` records, or is NULL with `*out_count == 0` for an empty file.
   * On any other outcome both are left untouched. */
    StockReadOutcome (
        *read_all)(void* ctx, const char* path, StockItem** out_items, size_t* out_count);
    /** Reads whatever raw bytes are at `path`, with no framing validation -- used
   * only to compare a main file that already decoded against `.tmp`, byte for
   * byte, since the DB has no header to tell a genuine save from a partial
   * copy. False only on a genuine open/read failure or a size over `max_bytes`;
   * an empty file is true with
   * `*out_len == 0`. Caller frees `*out_bytes` on success. */
    bool (*read_raw)(void* ctx, const char* path, uint8_t** out_bytes, size_t* out_len);
    bool (*write_all)(void* ctx, const char* path, const StockItem* items, size_t count);
    bool (*rename)(void* ctx, const char* src, const char* dst);
    bool (*remove)(void* ctx, const char* path);
} StockStoragePort;
