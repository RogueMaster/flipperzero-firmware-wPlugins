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

#include "slice.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

Slice slice_trim_left(Slice slice) {
    while(slice.start < slice.end && isspace((unsigned char)*slice.start)) {
        slice.start++;
    }
    return slice;
}

Slice slice_trim_right(Slice slice) {
    while(slice.start < slice.end && isspace((unsigned char)*(slice.end - 1))) {
        slice.end--;
    }
    return slice;
}

Slice slice_trim(Slice slice) {
    return slice_trim_left(slice_trim_right(slice));
}

bool slice_equals_cstr(Slice slice, const char* str) {
    const char* p = slice.start;
    while(p < slice.end && *str != '\0' && *p == *str) {
        p++;
        str++;
    }
    return p == slice.end && *str == '\0';
}

bool slice_to_cstr(Slice slice, char* buffer, size_t size) {
    size_t len = slice_len(slice);
    if(len < size) {
        memcpy(buffer, slice.start, len);
        buffer[len] = '\0';
        return true;
    } else {
        return false;
    }
}

bool slice_parse_int32(Slice slice, int base, int32_t* result) {
    char temp[32];
    if(slice_to_cstr(slice, temp, sizeof(temp))) {
        char* endptr;
        int32_t value = strtol(temp, &endptr, base);
        if(*endptr == '\0') {
            *result = value;
            return true;
        }
    }
    return false;
}

bool slice_parse_uint32(Slice slice, int base, uint32_t* result) {
    char temp[32];
    if(slice_to_cstr(slice, temp, sizeof(temp))) {
        char* endptr;
        uint32_t value = strtoul(temp, &endptr, base);
        if(*endptr == '\0') {
            *result = value;
            return true;
        }
    }
    return false;
}

bool slice_parse_float(Slice slice, float* result) {
    char temp[32];
    if(slice_to_cstr(slice, temp, sizeof(temp))) {
        char* endptr;
        float value = strtof(temp, &endptr);
        if(*endptr == '\0') {
            *result = value;
            return true;
        }
    }
    return false;
}

bool slice_parse_double(Slice slice, double* result) {
    char temp[32];
    if(slice_to_cstr(slice, temp, sizeof(temp))) {
        char* endptr;
        double value = strtod(temp, &endptr);
        if(*endptr == '\0') {
            *result = value;
            return true;
        }
    }
    return false;
}

Slice tok_until_char(Slice* slice, char c) {
    const char* p = slice->start;
    while(p < slice->end && *p != c) {
        p++;
    }
    Slice result = {slice->start, p};
    slice->start = p;
    return result;
}

Slice tok_until_eol(Slice* slice) {
    Slice result = tok_until_char(slice, '\n');

    // Trim the trailing '\r' character
    if(result.start < result.end && *(result.end - 1) == '\r') {
        result.end--;
        slice->start--;
    }

    return result;
}

bool tok_skip_char(Slice* slice, char c) {
    if(slice->start < slice->end && *slice->start == c) {
        slice->start++;
        return true;
    } else {
        return false;
    }
}

bool tok_skip_eol(Slice* slice) {
    size_t len = slice->end - slice->start;
    const char* p = slice->start;

    if(len > 0 && *p == '\n') {
        slice->start += 1;
        return true;
    } else if(len > 1 && *p == '\r' && *(p + 1) == '\n') {
        slice->start += 2;
        return true;
    } else {
        return false;
    }
}
