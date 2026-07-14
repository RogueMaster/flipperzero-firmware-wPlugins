#pragma once

#include <furi.h>
#include <furi_hal_usb_hid.h>

#ifdef __cplusplus
extern "C" {
#endif

void bunnyconnect_send_key_press(uint16_t key);
void bunnyconnect_send_key_release(uint16_t key);
void bunnyconnect_send_string(const char* string);
void bunnyconnect_send_enter(void);
void bunnyconnect_send_backspace(void);
uint16_t bunnyconnect_char_to_hid_key(char c);

/**
 * @brief Find string length with maximum limit
 * 
 * Custom implementation of strnlen to avoid dependency issues
 * 
 * @param str string to measure
 * @param maxlen maximum length to check
 * @return size_t length of string or maxlen if no null terminator found
 */
static inline size_t safe_strlen(const char* str, size_t maxlen) {
    if(str == NULL) return 0;

    size_t len = 0;
    while(len < maxlen && str[len] != '\0') {
        len++;
    }
    return len;
}

/**
 * @brief Safe string concatenation using memcpy
 * 
 * @param dest destination string buffer
 * @param src source string to append
 * @param dest_size total size of destination buffer
 * @return size_t number of characters appended
 */
static inline size_t string_cat_safe(char* dest, const char* src, size_t dest_size) {
    if(dest == NULL || src == NULL || dest_size == 0) return 0;

    size_t dest_len = strlen(dest);
    if(dest_len >= dest_size - 1) return 0; // No space left

    size_t available = dest_size - dest_len - 1;
    size_t src_len = strlen(src);
    size_t to_copy = available < src_len ? available : src_len;

    if(to_copy > 0) {
        memcpy(dest + dest_len, src, to_copy);
        dest[dest_len + to_copy] = '\0';
    }

    return to_copy;
}

/**
 * @brief Safe string append with bounds checking
 * 
 * Replaces strcat with a safer alternative using memcpy
 * 
 * @param dest destination buffer  
 * @param src source string
 * @param dest_size size of destination buffer
 * @return bool true if successful, false if buffer would overflow
 */
static inline bool safe_append_string(char* dest, const char* src, size_t dest_size) {
    if(!dest || !src || dest_size == 0) return false;

    size_t dest_len = strlen(dest);
    size_t src_len = strlen(src);

    if(dest_len + src_len + 1 > dest_size) {
        return false; // Would overflow
    }

    memcpy(dest + dest_len, src, src_len + 1);
    return true;
}

#ifdef __cplusplus
}
#endif
