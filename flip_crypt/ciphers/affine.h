#include <stdint.h>

char* encode_affine(const char* text, uint8_t a, uint8_t b);
char* decode_affine(const char* text, uint8_t a, uint8_t b);