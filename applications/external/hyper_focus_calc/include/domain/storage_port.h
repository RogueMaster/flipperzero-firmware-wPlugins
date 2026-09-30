#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// The three outcomes of stat'ing a path. Only StoreStatMissing is a genuine FSE_NOT_EXIST;
// any other failure is StoreStatError and must never be treated as "missing".
typedef enum {
    StoreStatOk = 0,
    StoreStatMissing,
    StoreStatError,
} StoreStat;

// The three outcomes of reading a whole file. ReadTooLarge is deliberately NOT an IO fault:
// a file bigger than any valid record could be is corruption (or a bogus size field), not a
// storage problem, so it must be handled like any other unreadable file (quarantined), not
// treated as a reason to block saves forever.
typedef enum {
    ReadOk = 0,
    ReadTooLarge,
    ReadError,
} ReadResult;

// Minimal file operations the recovery algorithm needs, injected so it can run against
// real Storage on-device or an in-memory fake on host. `rename` must reproduce the
// firmware's own semantics: remove `dst` (ignored if absent), copy `src` to `dst`, then
// remove `src` — NOT atomic. A crash between those steps can leave `dst` missing or
// partially written while `src` is still intact, or leave both present.
typedef struct {
    void* ctx;
    StoreStat (*stat)(void* ctx, const char* path);
    bool (*exists)(void* ctx, const char* path);
    // Reads the whole file at `path` into `buf` (capacity `buf_size`). On ReadOk, `*out_len`
    // is the number of bytes read.
    ReadResult (
        *read_all)(void* ctx, const char* path, uint8_t* buf, size_t buf_size, size_t* out_len);
    bool (*write_all)(void* ctx, const char* path, const uint8_t* buf, size_t len);
    bool (*rename)(void* ctx, const char* src, const char* dst);
} StoragePort;
