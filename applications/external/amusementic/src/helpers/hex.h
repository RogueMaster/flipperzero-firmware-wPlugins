#pragma once

#include <furi.h>

// https://stackoverflow.com/a/57723618
bool to_hex(char* dest, size_t dest_len, const uint8_t* values, size_t val_len) {
    static const char hex_table[] = "0123456789ABCDEF";
    if(dest_len < (val_len * 3)) /* check that dest is large enough */
        return false;
    while(val_len--) {
        /* shift down the top nibble and pick a char from the hex_table */
        *dest++ = hex_table[*values >> 4];
        /* extract the bottom nibble and pick a char from the hex_table */
        *dest++ = hex_table[*values++ & 0xF];
        if(val_len) *dest++ = ':';
    }
    *dest = 0;
    return true;
}

char* to_hex_alloc(const uint8_t* values, size_t val_len) {
    size_t dest_len = val_len * 3;
    char* dest = malloc(dest_len);
    to_hex(dest, dest_len, values, val_len);
    return dest;
}

void furi_string_cat_hex(
    FuriString* string,
    const uint8_t* data,
    size_t data_size,
    const char* separator,
    size_t interval) {
    for(size_t i = 0; i < data_size; i++) {
        if(separator && interval && i > 0 && i % interval == 0) {
            furi_string_cat_str(string, separator);
        }
        furi_string_cat_printf(string, "%02X", data[i]);
    }
}
