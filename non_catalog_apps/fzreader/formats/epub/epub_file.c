/**
 * @file epub_file.c
 * @brief Firmware storage backend for the EPUB driver.
 *
 * The only file here that knows about furi and storage: epub.c works through
 * EpubFile and ReaderIo, so the drivers themselves stay testable on a host.
 */
#include "epub_file.h"

#include <furi.h>
#include <storage/storage.h>
#include <stdlib.h>

struct EpubFile {
    Storage* storage;
    File* file;
};

static bool epub_file_read_at(void* context, uint64_t offset, void* buffer, size_t size) {
    EpubFile* reader = context;
    if(reader == NULL || reader->file == NULL || buffer == NULL) {
        return false;
    }
    if(size == 0) {
        return true;
    }
    if(offset > UINT32_MAX) {
        return false;
    }
    if(!storage_file_seek(reader->file, (uint32_t)offset, true)) {
        return false;
    }
    return storage_file_read(reader->file, buffer, size) == size;
}

static uint64_t epub_file_size_cb(void* context) {
    EpubFile* reader = context;
    if(reader == NULL || reader->file == NULL) {
        return 0;
    }
    return storage_file_size(reader->file);
}

EpubFile* epub_file_open(const char* path) {
    if(path == NULL) {
        return NULL;
    }

    EpubFile* reader = malloc(sizeof(EpubFile));
    if(reader == NULL) {
        return NULL;
    }
    reader->storage = NULL;
    reader->file = NULL;

    reader->storage = furi_record_open(RECORD_STORAGE);
    reader->file = storage_file_alloc(reader->storage);
    if(reader->file == NULL) {
        furi_record_close(RECORD_STORAGE);
        free(reader);
        return NULL;
    }

    if(!storage_file_open(reader->file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_close(reader->file);
        storage_file_free(reader->file);
        furi_record_close(RECORD_STORAGE);
        free(reader);
        return NULL;
    }

    return reader;
}

void epub_file_close(EpubFile* reader) {
    if(reader == NULL) {
        return;
    }
    if(reader->file != NULL) {
        storage_file_close(reader->file);
        storage_file_free(reader->file);
    }
    if(reader->storage != NULL) {
        furi_record_close(RECORD_STORAGE);
    }
    free(reader);
}

ReaderIo epub_file_io(EpubFile* reader) {
    ReaderIo io = {
        .read_at = epub_file_read_at,
        .size = epub_file_size_cb,
        .context = reader,
    };
    return io;
}
