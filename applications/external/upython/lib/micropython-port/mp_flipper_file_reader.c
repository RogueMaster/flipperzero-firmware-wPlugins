#include <stdio.h>

#include <furi.h>
#include <storage/storage.h>

#include <mp_flipper_runtime.h>
#include <mp_flipper_file_reader.h>

#include "mp_flipper_file_helper.h"

typedef struct {
    size_t pointer;
    FuriString* content;
    size_t size;
} FileReaderContext;

inline void* mp_flipper_file_reader_context_alloc(const char* filename) {
    FuriString* path = furi_string_alloc_printf("%s", filename);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    FileReaderContext* ctx = NULL;

    do {
        if(mp_flipper_try_resolve_filesystem_path(path) == MP_FLIPPER_IMPORT_STAT_NO_EXIST) {
            furi_string_free(path);

            mp_flipper_raise_os_error_with_filename(MP_ENOENT, filename);
        }

        if(!storage_file_open(file, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
            storage_file_free(file);

            mp_flipper_raise_os_error_with_filename(MP_ENOENT, filename);

            break;
        }

        ctx = malloc(sizeof(FileReaderContext));

        ctx->pointer = 0;
        ctx->content = furi_string_alloc();
        ctx->size = storage_file_size(file);

        char character = '\0';

        for(size_t i = 0; i < ctx->size; i++) {
            storage_file_read(file, &character, 1);

            furi_string_push_back(ctx->content, character);
        }
    } while(false);

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(path);

    return ctx;
}

inline uint32_t mp_flipper_file_reader_read(void* data) {
    FileReaderContext* ctx = data;

    if(ctx->pointer >= ctx->size) {
        return MP_FLIPPER_FILE_READER_EOF;
    }

    return furi_string_get_char(ctx->content, ctx->pointer++);
}

void mp_flipper_file_reader_close(void* data) {
    FileReaderContext* ctx = data;

    furi_string_free(ctx->content);

    free(data);
}