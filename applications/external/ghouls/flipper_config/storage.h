#pragma once
#include <furi.h>
#include <storage/storage.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t storage_read(const char* file_path, void* buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif
