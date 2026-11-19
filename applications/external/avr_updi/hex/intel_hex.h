#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define INTEL_HEX_MAX_PAGE_SIZE 512u

/** Called with one finished page. Data holes are 0xFF. Return false to stop. */
typedef bool (*IntelHexPageCallback)(
    uint32_t address,
    const uint8_t* data,
    uint16_t page_size,
    void* context);

typedef struct IntelHexParser IntelHexParser;

IntelHexParser* intel_hex_parser_alloc(
    uint32_t max_address,
    uint16_t page_size,
    IntelHexPageCallback on_page,
    void* context);

void intel_hex_parser_free(IntelHexParser* parser);

/** Feed one HEX record line, without the trailing newline. */
bool intel_hex_parser_feed(
    IntelHexParser* parser,
    const char* line,
    char* error,
    size_t error_size);

/** Emit the last open page. A missing EOF record is accepted. */
bool intel_hex_parser_finish(IntelHexParser* parser, char* error, size_t error_size);

typedef bool (*IntelHexWriteFn)(const char* line, void* context);

typedef struct IntelHexWriter IntelHexWriter;

IntelHexWriter* intel_hex_writer_alloc(IntelHexWriteFn write, void* context);

void intel_hex_writer_free(IntelHexWriter* writer);

/** Append bytes at an absolute byte address. Addresses must not go backwards. */
bool intel_hex_writer_feed(
    IntelHexWriter* writer,
    uint32_t address,
    const uint8_t* data,
    size_t length,
    char* error,
    size_t error_size);

bool intel_hex_writer_finish(IntelHexWriter* writer, char* error, size_t error_size);
