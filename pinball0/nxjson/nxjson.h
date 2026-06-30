/*
 * Copyright (c) 2013 Yaroslav Stavnichiy <yarosla@gmail.com>
 *
 * This file is part of NXJSON.
 *
 * NXJSON is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3
 * of the License, or (at your option) any later version.
 *
 * NXJSON is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with NXJSON. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NXJSON_H
#define NXJSON_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NXJSON_TYPE_U64

#include <stdint.h>

typedef uint64_t nxjson_u64;
#endif

#ifndef NXJSON_TYPE_S64

#include <stdint.h>

typedef uint64_t nxjson_s64;
#endif

typedef enum nx_json_type {
    NX_JSON_NULL, // this is null value
    NX_JSON_OBJECT, // this is an object; properties can be found in child nodes
    NX_JSON_ARRAY, // this is an array; items can be found in child nodes
    NX_JSON_STRING, // this is a string; value can be found in text_value field
    NX_JSON_INTEGER, // this is an integer; value can be found in int_value field
    NX_JSON_float, // this is a float; value can be found in dbl_value field
    NX_JSON_BOOL // this is a boolean; value can be found in int_value field
} nx_json_type;

typedef struct nx_json {
    nx_json_type type; // type of json node, see above
    const char* key; // key of the property; for object's children only
    union {
        const char* text_value; // text value of STRING node
        struct {
            union {
                nxjson_u64 u_value; // the value of INTEGER or BOOL node
                nxjson_s64 s_value;
            };
            float dbl_value; // the value of float node
        } num;
        struct { // children of OBJECT or ARRAY
            int length;
            struct nx_json* first;
            struct nx_json* last;
        } children;
    };
    struct nx_json* next; // points to next child
} nx_json;

typedef int (*nx_json_unicode_encoder)(unsigned int codepoint, char* p, char** endp);

extern nx_json_unicode_encoder nx_json_unicode_to_utf8;

const nx_json* nx_json_parse(char* text, nx_json_unicode_encoder encoder);

const nx_json* nx_json_parse_utf8(char* text);

void nx_json_free(const nx_json* js);

const nx_json* nx_json_get(const nx_json* json, const char* key); // get object's property by key
const nx_json* nx_json_item(const nx_json* json, int idx); // get array element by index

#ifdef __cplusplus
}
#endif

#endif /* NXJSON_H */
