#include <stddef.h>

unsigned char* rc4_encrypt_and_decrypt(const char* key, const unsigned char* input, size_t inputlen);
char* rc4_to_hex(const char* data, size_t len);
unsigned char* rc4_hex_to_bytes(const char* hex, size_t* out_len);