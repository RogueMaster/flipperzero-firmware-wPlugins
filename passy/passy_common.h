#pragma once

#include <mbedtls/des.h>
#include <furi.h>
#include <toolbox/bit_buffer.h>

typedef enum {
    PassyReadNone = 0,
    PassyReadCOM = 0x011E,
    PassyReadDG1 = 0x0101,
    PassyReadDG2 = 0x0102,
    PassyReadDG3 = 0x0103,
    PassyReadDG4 = 0x0104,
    PassyReadDG5 = 0x0105,
    PassyReadDG6 = 0x0106,
    PassyReadDG7 = 0x0107,
    PassyReadDG8 = 0x0108,
    PassyReadDG9 = 0x0109,
    PassyReadDG10 = 0x010A,
    PassyReadDG11 = 0x010B,
    PassyReadDG12 = 0x010C,
    PassyReadDG13 = 0x010D,
    PassyReadDG14 = 0x010E,
    PassyReadDG15 = 0x010F,
} PassyReadType;

void passy_log_bitbuffer(char* tag, char* prefix, BitBuffer* buffer);
void passy_log_buffer(char* tag, char* prefix, const uint8_t* buffer, size_t buffer_len);
void passy_mac(uint8_t* key, uint8_t* data, size_t data_length, uint8_t* mac, bool prepadded);
char passy_checksum(char* str);
int passy_print_struct_callback(const void* buffer, size_t size, void* app_key);

size_t passy_furi_string_filename_safe(FuriString* string);
