/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "bootloader_sha.h"
#include <tiny_crypto/hash.h>
#include <stdlib.h>

/* IDF's application API owns an opaque handle until finish, including cancel. */
struct tc_boot_hash {
    unsigned bits;
    union {
        struct TC_SHA256_ctx sha256;
        struct TC_SHA384_ctx sha384;
        struct TC_SHA512_ctx sha512;
    } context;
};

static void tc_boot_hash_free(struct tc_boot_hash *hash)
{
    TC_secure_zero(hash, sizeof *hash);
    free(hash);
}

static void *tc_boot_hash_start(unsigned bits)
{
    struct tc_boot_hash *hash = malloc(sizeof *hash);
    TC_status status;
    if (!hash) return NULL;
    hash->bits = bits;
    switch (bits) {
        case 256: status = TC_SHA256_init(&hash->context.sha256); break;
        case 384: status = TC_SHA384_init(&hash->context.sha384); break;
        default: status = TC_SHA512_init(&hash->context.sha512); break;
    }
    if (status != TC_OK) {
        tc_boot_hash_free(hash);
        return NULL;
    }
    return hash;
}

static void tc_boot_hash_data(void *handle, const void *data, size_t length)
{
    struct tc_boot_hash *hash = handle;
    TC_status status;
    if (!hash) abort();
    switch (hash->bits) {
        case 256: status = TC_SHA256_update(&hash->context.sha256, data, length); break;
        case 384: status = TC_SHA384_update(&hash->context.sha384, data, length); break;
        case 512: status = TC_SHA512_update(&hash->context.sha512, data, length); break;
        default: status = TC_ERROR; break;
    }
    /* The IDF callback has no error return. Never continue image verification. */
    if (status != TC_OK) {
        tc_boot_hash_free(hash);
        abort();
    }
}

static void tc_boot_hash_finish(void *handle, uint8_t *digest)
{
    struct tc_boot_hash *hash = handle;
    TC_status status = TC_OK;
    if (!hash) abort();
    if (digest) {
        switch (hash->bits) {
            case 256: status = TC_SHA256_final(&hash->context.sha256, digest); break;
            case 384: status = TC_SHA384_final(&hash->context.sha384, digest); break;
            case 512: status = TC_SHA512_final(&hash->context.sha512, digest); break;
            default: status = TC_ERROR; break;
        }
    }
    tc_boot_hash_free(hash);
    if (status != TC_OK) abort();
}

bootloader_sha256_handle_t bootloader_sha256_start(void)
{ return tc_boot_hash_start(256); }
void bootloader_sha256_data(bootloader_sha256_handle_t handle, const void *data, size_t length)
{ tc_boot_hash_data(handle, data, length); }
void bootloader_sha256_finish(bootloader_sha256_handle_t handle, uint8_t *digest)
{ tc_boot_hash_finish(handle, digest); }

#if SOC_SHA_SUPPORT_SHA512
bootloader_sha_handle_t bootloader_sha512_start(bool is384)
{ return tc_boot_hash_start(is384 ? 384 : 512); }
void bootloader_sha512_data(bootloader_sha_handle_t handle, const void *data, size_t length)
{ tc_boot_hash_data(handle, data, length); }
void bootloader_sha512_finish(bootloader_sha_handle_t handle, uint8_t *digest)
{ tc_boot_hash_finish(handle, digest); }
#endif
