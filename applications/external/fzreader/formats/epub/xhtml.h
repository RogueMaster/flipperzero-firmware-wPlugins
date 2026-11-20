#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "../../text/../formats/reader_book.h"

typedef struct XhtmlParser XhtmlParser;

/** Source of decompressed markup: @returns bytes read, 0 at end. */
typedef size_t (*XhtmlPull)(void* context, char* buffer, size_t size);

XhtmlParser* xhtml_alloc(void);
void xhtml_free(XhtmlParser* parser);

void xhtml_begin(XhtmlParser* parser, XhtmlPull pull, void* context);

/**
 * Get the next run of text with uniform styling.
 *
 * @returns false at the end of the document. The text is valid until the next call.
 */
bool xhtml_next_span(XhtmlParser* parser, ReaderSpan* span);
