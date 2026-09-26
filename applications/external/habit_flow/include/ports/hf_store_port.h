#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Only "does not exist" counts as Missing; any other failure to stat a path is Error,
// since the caller cannot tell whether the file is actually there.
typedef enum {
    HfPathOk = 0,
    HfPathMissing,
    HfPathError,
} HfPathStat;

// Port: persistent storage for the habit store. The adapter (platform/) wraps furi
// Storage; host tests use an in-memory fake. `rename` is NOT assumed atomic: the firmware
// implements it as remove-destination, copy, remove-source, so a crash partway through can
// leave the destination missing or the source still present. Callers must be able to
// recover from that, not just from a clean rename.
typedef struct {
    void* self;
    HfPathStat (*stat)(void* self, const char* path);
    // Reads up to `cap` bytes; returns bytes read, or 0 on any failure.
    size_t (*read)(void* self, const char* path, uint8_t* buf, size_t cap);
    // Creates or truncates `path` and writes `len` bytes.
    bool (*write)(void* self, const char* path, const uint8_t* data, size_t len);
    bool (*rename)(void* self, const char* old_path, const char* new_path);
    bool (*remove)(void* self, const char* path);
    bool (*mkdir)(void* self, const char* path);
} HfStorePort;
