/* Append-only compact JSON writer into a caller-owned fixed buffer.
 *
 * No allocation, no nesting stack: commas are inserted automatically before
 * every key and every array element. The buffer always stays NUL-terminated.
 * The first write that does not fit (text plus NUL) puts the writer into a
 * sticky error state; every call returns false from then on, and json_len()
 * returns 0. Callers may chain writes and check only the last result.
 *
 * Strings are escaped per RFC 8259. The protocol keeps the Flipper side
 * ASCII-only, so bytes >= 0x80 are written as '?'.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    char* buf;
    size_t cap;
    size_t len;
    bool ok;
    bool need_comma;
} JsonWriter;

void json_init(JsonWriter* w, char* buf, size_t cap);

bool json_object_open(JsonWriter* w);
bool json_object_close(JsonWriter* w);
bool json_array_open(JsonWriter* w);
bool json_array_close(JsonWriter* w);

/* Writes `"key":`; the value call that follows gets no comma. */
bool json_key(JsonWriter* w, const char* key);

bool json_string(JsonWriter* w, const char* s);
bool json_int(JsonWriter* w, long value);
bool json_bool(JsonWriter* w, bool value);
/* Writes `len` bytes as one upper-case hex string without separators. */
bool json_hex(JsonWriter* w, const uint8_t* bytes, size_t len);

bool json_ok(const JsonWriter* w);
/* Length of the text written so far, 0 after an error. */
size_t json_len(const JsonWriter* w);
