#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * ★ Last Upload summary. No furi. The scene owns the file.
 * Missing file is SrLastNone. Empty or corrupt text is SrLastBad.
 * A write or read failure must not change the upload itself.
 */

typedef struct {
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint32_t ok_n;
    uint32_t fail_n;
    bool wigle200;
} SrLastUpload;

typedef enum {
    SrLastNone = 0,
    SrLastOk,
    SrLastBad,
} SrLastParse;

size_t sr_last_upload_format(const SrLastUpload* u, char* dst, size_t cap);
SrLastParse sr_last_upload_parse(const char* text, size_t len, SrLastUpload* out);
size_t sr_last_upload_line(unsigned status, const SrLastUpload* u, char* dst, size_t cap);
size_t sr_last_upload_pending(bool known, uint32_t n, char* dst, size_t cap);
