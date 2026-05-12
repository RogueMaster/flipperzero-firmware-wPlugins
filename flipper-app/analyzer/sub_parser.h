#pragma once

#include "types.h"
#include <storage/storage.h>

typedef enum {
    SubParseOk = 0,
    SubParseTruncated, /* parsed, but caps were hit */
    SubParseErrorOpen,
    SubParseErrorMissingField,
    SubParseErrorNoData,
    SubParseErrorMalformedHex,
} SubParseStatus;

void sub_file_init(SubFile* sub);
void sub_file_reset(SubFile* sub);

/* Parse a .sub Flipper SubGhz Key File. Reads the file via the storage API,
 * converts each Bit_RAW/Data_RAW pair into one segment of MSB-first bits.
 *
 * On any error, an explanatory string is appended to *err_out (caller may pass
 * NULL). Bits beyond BITRAW_MAX_BITS_PER_SEG, segments beyond
 * BITRAW_MAX_SEGMENTS, or total bits beyond BITRAW_MAX_TOTAL_BITS cause
 * truncation and `sub->truncated = true` (not a hard error).
 */
SubParseStatus sub_parser_parse(
    Storage* storage,
    const char* path,
    SubFile* sub,
    FuriString* err_out);
