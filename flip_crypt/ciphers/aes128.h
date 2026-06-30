#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint8_t state_t[4][4];

void KeyExpansion(const uint8_t* key, uint8_t* roundKeys);
void AddRoundKey(state_t* state, const uint8_t* roundKey);
void SubBytes(state_t* state);
void ShiftRows(state_t* state);
void MixColumns(state_t* state);
void AES_encrypt_block(uint8_t* input, const uint8_t* key, uint8_t* output);
void pad_input(const char* input, uint8_t* output);
void print_hex(uint8_t* data, size_t len);
void to_hex_string(const uint8_t* data, size_t len, char* output);

void AES_decrypt_block(uint8_t* input, const uint8_t* key, uint8_t* output);