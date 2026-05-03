#pragma once
#include <furi.h>
#include <storage/storage.h>

#ifdef __cplusplus
extern "C"
{
#endif

    uint16_t storage_file_list(const char *pattern, char **output, uint16_t offset, uint16_t max_files);
    size_t storage_read(const char *file_path, void *buffer, size_t buffer_size);
    bool storage_write(const char *file_path, const void *data, size_t data_size);

#ifdef __cplusplus
}
#endif
