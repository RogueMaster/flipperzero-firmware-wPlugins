#pragma once
#include "../app.h"

//Function Declarations
void delete_and_update_entry(void* context, uint32_t KeyToDelete);

void update_dictionary_keys(void* context);

uint8_t hex_char_to_int(char c);

void hex_string_to_bytes(const char* hex_string, uint8_t* byte_array, size_t* byte_array_len);

char* uint16_to_hex_string(uint16_t value);

void hex_string_to_uint16(const char* hex_string, uint16_t* uint16_array, size_t* uint16_array_len);

char* combineArrays(const char* array1, const char* array2);

uint32_t bytes_to_uint32(uint8_t* bytes, size_t length);