#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int placeholder;
} mbedtls_md_context_t;

typedef struct {
    int placeholder;
} mbedtls_md_info_t;

#define MBEDTLS_MD_SHA256 0

void mbedtls_md_init(mbedtls_md_context_t *ctx);
void mbedtls_md_free(mbedtls_md_context_t *ctx);
const mbedtls_md_info_t *mbedtls_md_info_from_type(int md_type);
int mbedtls_md_setup(mbedtls_md_context_t *ctx, const mbedtls_md_info_t *md_info, int hmac);
int mbedtls_md_hmac_starts(mbedtls_md_context_t *ctx, const unsigned char *key, size_t keylen);
int mbedtls_md_hmac_update(mbedtls_md_context_t *ctx, const unsigned char *input, size_t ilen);
int mbedtls_md_hmac_finish(mbedtls_md_context_t *ctx, unsigned char *output);
