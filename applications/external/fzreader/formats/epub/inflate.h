#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/** Streaming raw DEFLATE (RFC 1951) decompressor, without a zlib wrapper. */
typedef struct ReaderInflate ReaderInflate;

/** Pull the next chunk of compressed data into buffer. @returns bytes written, 0 when the stream ends. */
typedef size_t (*ReaderInflatePull)(void* context, uint8_t* buffer, size_t size);

ReaderInflate* reader_inflate_alloc(void);
void reader_inflate_free(ReaderInflate* inflate);

/** Start a new stream; with pull == NULL the decoder is parked and drops the stream state. */
void reader_inflate_begin(ReaderInflate* inflate, ReaderInflatePull pull, void* context);

/**
 * Produce decompressed bytes.
 *
 * @returns bytes written to out; 0 means the stream has ended or an error occurred,
 *          use reader_inflate_failed() to tell them apart.
 */
size_t reader_inflate_read(ReaderInflate* inflate, uint8_t* out, size_t out_size);

bool reader_inflate_failed(const ReaderInflate* inflate);
