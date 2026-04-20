#pragma once
#include <furi.h>
#include <storage/storage.h>

#ifdef __cplusplus
extern "C"
{
#endif

    static inline size_t storage_read(const char *file_path, void *buffer, size_t buffer_size)
    {
        Storage *storage = (Storage *)furi_record_open(RECORD_STORAGE);
        File *file = storage_file_alloc(storage);

        // Open the file for reading
        if (!storage_file_open(file, file_path, FSAM_READ, FSOM_OPEN_EXISTING))
        {
            storage_file_free(file);
            furi_record_close(RECORD_STORAGE);
            return 0;
        }

        // Read data into the buffer
        size_t read_count = storage_file_read(file, buffer, buffer_size);
        if (storage_file_get_error(file) != FSE_OK)
        {
            FURI_LOG_E("storage", "Error reading from file.");
            storage_file_close(file);
            storage_file_free(file);
            furi_record_close(RECORD_STORAGE);
            return 0;
        }

        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);

        return read_count;
    }

#ifdef __cplusplus
}
#endif
