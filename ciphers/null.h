#pragma once

#include <stddef.h>
#include <stdint.h>

char* null_encrypt(const char* input, int32_t n);
char* null_decrypt(const char* input, int32_t n);