#ifndef MD5_H
#define MD5_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    uint32_t h[4];
    uint8_t buffer[64];
    uint64_t length;
} MD5Context;

void md5_init(MD5Context* ctx);
void md5_update(MD5Context* ctx, const uint8_t* data, size_t len);
void md5_finalize(MD5Context* ctx, uint8_t hash[16]);
void md5_to_hex(const uint8_t hash[16], char* output);

#endif