#pragma once

#include <storage/storage.h>

/* Returns a heap-allocated "<dir>/<prefix>_<n>.<extension>" path whose <n> is the first index
   not already present on disk (caller frees). Ported from the WiFi Marauder companion app. */
char* sequential_file_resolve_path(
    Storage* storage,
    const char* dir,
    const char* prefix,
    const char* extension);

/* Resolve a fresh sequential path (as above) and open it for writing (create-always). */
bool sequential_file_open(
    Storage* storage,
    File* file,
    const char* dir,
    const char* prefix,
    const char* extension);
