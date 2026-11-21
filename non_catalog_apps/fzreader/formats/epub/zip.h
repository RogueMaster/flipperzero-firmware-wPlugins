#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "reader_io.h"

/** Sequential reader over the entries of a ZIP archive. */
typedef struct ReaderZip ReaderZip;

#define READER_ZIP_NAME_MAX 128

typedef struct {
    uint16_t method; /* 0 = stored, 8 = deflate */
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint32_t local_header_offset;
    char name[READER_ZIP_NAME_MAX]; /* truncated to fit, zero-terminated */
} ReaderZipEntry;

/**
 * An entry opened for sequential reading.
 *
 * At most one stream may be open per archive at a time: the DEFLATE window is
 * shared by all the streams of the archive and is only reset when a stream is
 * opened, so a second stream would corrupt the first one.
 */
typedef struct ReaderZipStream ReaderZipStream;

/**
 * Allocate an archive.
 *
 * The DEFLATE window (32 KiB) is not allocated here: it is created on the first
 * deflated entry and then reused by every stream until reader_zip_free().
 */
ReaderZip* reader_zip_alloc(ReaderIo io);

/** Close the archive, if any, and release the DEFLATE window. */
void reader_zip_free(ReaderZip* zip);

/** Find the End Of Central Directory record and remember the directory location. */
bool reader_zip_open(ReaderZip* zip);

/** Case-sensitive lookup by name. @returns index or (size_t)-1. */
size_t reader_zip_find(ReaderZip* zip, const char* name);

/**
 * Open an entry for sequential reading: skips the local header, prepares stored or inflate.
 *
 * @returns NULL if the entry cannot be read, or if another stream of the same
 *          archive is still open.
 */
ReaderZipStream* reader_zip_entry_open(ReaderZip* zip, size_t index);

/** Close the stream. The DEFLATE window of the archive stays allocated. */
void reader_zip_stream_close(ReaderZipStream* stream);

/** @returns bytes read, 0 at end of entry or on error. */
size_t reader_zip_stream_read(ReaderZipStream* stream, void* buffer, size_t size);
