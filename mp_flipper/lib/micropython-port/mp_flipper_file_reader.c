#include <stdio.h>

#include <furi.h>
#include <storage/storage.h>

#include <py/mperrno.h>

#include <mp_flipper_runtime.h>
#include <mp_flipper_file_reader.h>

#include "mp_flipper_context.h"
#include "mp_flipper_file_helper.h"

typedef struct {
    size_t pointer;
    FuriString* content;
    size_t size;
} FileDescriptor;

inline void* mp_flipper_file_reader_context_alloc(const char* filename) {
    mp_flipper_context_t* ctx = mp_flipper_context;

    FuriString* path = furi_string_alloc_printf("%s", filename);
    File* file = storage_file_alloc(ctx->storage);
    FileDescriptor* fd = NULL;

    do {
        if(mp_flipper_try_resolve_filesystem_path(path) == MP_FLIPPER_IMPORT_STAT_NO_EXIST) {
            mp_flipper_raise_os_error_with_filename(MP_ENOENT, filename);

            break;
        }

        if(!storage_file_open(file, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
            mp_flipper_raise_os_error_with_filename(MP_ENOENT, filename);

            break;
        }

        fd = malloc(sizeof(FileDescriptor));

        fd->pointer = 0;
        fd->content = furi_string_alloc();
        fd->size = storage_file_size(file);

        char character = '\0';

        for(size_t i = 0; i < fd->size; i++) {
            storage_file_read(file, &character, 1);

            furi_string_push_back(fd->content, character);
        }
    } while(false);

    storage_file_free(file);
    furi_string_free(path);

    return fd;
}

inline uint32_t mp_flipper_file_reader_read(void* data) {
    FileDescriptor* fd = data;

    if(fd->pointer >= fd->size) {
        return MP_FLIPPER_FILE_READER_EOF;
    }

    return furi_string_get_char(fd->content, fd->pointer++);
}

void mp_flipper_file_reader_close(void* data) {
    FileDescriptor* fd = data;

    furi_string_free(fd->content);

    free(data);
}
