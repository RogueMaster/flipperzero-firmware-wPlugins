#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "storage/storage.h"

typedef struct FlipperFormat FlipperFormat;
typedef struct FuriString FuriString;

FlipperFormat *flipper_format_file_alloc(Storage *storage);
void flipper_format_free(FlipperFormat *flipper_format);

bool flipper_format_file_open_always(FlipperFormat *flipper_format, const char *path);
bool flipper_format_file_open_existing(FlipperFormat *flipper_format, const char *path);
bool flipper_format_file_close(FlipperFormat *flipper_format);

bool flipper_format_write_header_cstr(FlipperFormat *flipper_format, const char *type,
                                      uint32_t version);
bool flipper_format_write_uint32(FlipperFormat *flipper_format, const char *key,
                                 const uint32_t *value, size_t count);
bool flipper_format_write_hex(FlipperFormat *flipper_format, const char *key, const uint8_t *value,
                              size_t size);

bool flipper_format_read_header(FlipperFormat *flipper_format, FuriString *type, uint32_t *version);
bool flipper_format_read_uint32(FlipperFormat *flipper_format, const char *key, uint32_t *value,
                                size_t count);
bool flipper_format_read_hex(FlipperFormat *flipper_format, const char *key, uint8_t *value,
                             size_t size);

FuriString *furi_string_alloc(void);
void furi_string_free(FuriString *string);
const char *furi_string_get_cstr(const FuriString *string);
