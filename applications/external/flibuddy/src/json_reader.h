/* Minimal pull-style JSON reader over a caller-owned text, no allocation.
 *
 * Enough for the host -> Flipper messages of protocol/PROTOCOL.md: objects
 * with string, integer, boolean and array-of-string values plus a nested
 * object or two. Any other value can be skipped (json_skip validates it).
 *
 *   json_reader_init(&r, text, len);
 *   if(!json_object_begin(&r)) ...;
 *   while(json_object_next(&r, key, sizeof(key))) {
 *       if(strcmp(key, "n") == 0) json_read_int(&r, &n); else json_skip(&r);
 *   }
 *   if(!json_reader_finish(&r)) ...;  // malformed somewhere
 *
 * After json_object_next / json_array_next return true the caller consumes
 * exactly one value. The first failure (syntax error, wrong type, string
 * too long, nesting deeper than JSON_READER_MAX_DEPTH) puts the reader into
 * a sticky error state: every call returns false from then on.
 *
 * Strings come out ASCII-only, like everything the Flipper renders: each
 * control character and each non-ASCII code point (raw UTF-8 or a \u
 * escape, a surrogate pair counting as one) becomes a single '?'.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define JSON_READER_MAX_DEPTH 8

typedef enum {
    JsonTypeInvalid,
    JsonTypeObject,
    JsonTypeArray,
    JsonTypeString,
    JsonTypeNumber,
    JsonTypeBool,
    JsonTypeNull,
} JsonType;

typedef struct {
    const char* p;
    const char* end;
    bool ok;
    uint8_t depth;
    uint16_t first; /* bit d: the container at depth d has no element yet */
} JsonReader;

void json_reader_init(JsonReader* r, const char* text, size_t len);

/* Type of the next value, without consuming it; JsonTypeInvalid at the end
 * of the text or on an unexpected character. Never sets the error state. */
JsonType json_peek(JsonReader* r);

bool json_object_begin(JsonReader* r);
/* Reads the next key of the current object into `key` (NUL-terminated) and
 * the ':' after it. Returns false at the closing '}' (consumed) or on error.
 * A key that does not fit in `cap` reads as "". `key` may be NULL. */
bool json_object_next(JsonReader* r, char* key, size_t cap);

bool json_array_begin(JsonReader* r);
/* Returns true when another element follows, false at the closing ']'
 * (consumed) or on error. */
bool json_array_next(JsonReader* r);

/* Reads a string into `out` (NUL-terminated); a string longer than
 * `cap - 1` characters is an error. */
bool json_read_string(JsonReader* r, char* out, size_t cap);
/* Reads an integer (no fraction, no exponent) that fits in a long. */
bool json_read_int(JsonReader* r, long* out);
bool json_read_bool(JsonReader* r, bool* out);
/* Skips one value of any type, validating it. */
bool json_skip(JsonReader* r);

bool json_reader_ok(const JsonReader* r);
/* True when no error occurred and only whitespace is left. */
bool json_reader_finish(JsonReader* r);
