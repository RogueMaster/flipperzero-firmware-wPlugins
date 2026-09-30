#pragma once

#include <stdbool.h>
#include <stddef.h>

/* Exact-size blobs keep settings and key-vault records atomic to callers. */
bool uhf_storage_read_blob(const char* path, void* data, size_t size);
bool uhf_storage_write_blob(const char* directory, const char* path, const void* data, size_t size);
