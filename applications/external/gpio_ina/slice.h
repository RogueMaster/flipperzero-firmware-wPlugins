/* 
 * This file is part of the INA Meter application for Flipper Zero (https://github.com/cepetr/flipper-tina).
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char* start;
    const char* end;
} Slice;

// Returns an empty slice
static inline Slice slice_empty(void) {
    Slice slice = {
        .start = NULL,
        .end = NULL,
    };
    return slice;
}

// Returns `true` if the slice is empty
static inline bool slice_is_empty(Slice slice) {
    return slice.start >= slice.end;
}

// Returns the length of the slice
static inline size_t slice_len(Slice slice) {
    return slice.end - slice.start;
}

// Trims the whitespace from the beginning of the slice.
Slice slice_trim_left(Slice slice);

// Trims the whitespace from the end of the slice.
Slice slice_trim_right(Slice slice);

// Trims the whitespace from both ends of the slice.
Slice slice_trim(Slice slice);

// Compares the slice with a C-string.
bool slice_equals_cstr(Slice slice, const char* str);

// Copies the slice into a C-string buffer and null-terminates it.
// Returns `true` if the slice fits within the buffer; otherwise, returns `false`.
// If the slice is too long, the buffer remains unchanged.
bool slice_to_cstr(Slice slice, char* buffer, size_t size);

// Parses the slice as a signed 32-bit integer.
// Returns `true` if the slice represents a valid integer.
bool slice_parse_int32(Slice slice, int base, int32_t* result);

// Parses the slice as an unsigned 32-bit integer.
// Returns `true` if the slice represents a valid integer.
bool slice_parse_uint32(Slice slice, int base, uint32_t* result);

// Parses the slice as a float.
// Returns `true` if the slice represents a valid float.
bool slice_parse_float(Slice slice, float* result);

// Parses the slice as a double.
// Returns `true` if the slice represents a valid double.
bool slice_parse_double(Slice slice, double* result);

// Returns a slice up to (but excluding) the specified character.
// Modifies the input slice to point to the remaining part.
Slice tok_until_char(Slice* slice, char c);

// Returns a slice up to (but excluding) the end of the line.
// Modifies the input slice to point to the remaining part.
Slice tok_until_eol(Slice* slice);

// Skips the specified character at the beginning of the slice.
// Returns `true` if the character was found; otherwise, returns `false`
// and leaves the slice unchanged.
bool tok_skip_char(Slice* slice, char c);

// Skips the end of the line in the slice.
// Returns `true` if the end of the line was found; otherwise, returns `false`
// and leaves the slice unchanged.
bool tok_skip_eol(Slice* slice);
