/**
 * @file sha1.h
 * @brief Minimal, self-contained SHA-1 implementation.
 *
 * Bundled on purpose: the Xiaomi filter password is derived from a SHA-1 digest
 * of the tag UID (see xiaomi_filter.h). Vendoring the hash keeps the derivation
 * free of any firmware/curated-API dependency, so the exact same object code runs
 * both on the Flipper and in the host unit tests.
 *
 * Derived from the public-domain implementation by Steve Reid <steve@edmweb.com>.
 * This file remains in the public domain.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHA1_DIGEST_SIZE (20U)
#define SHA1_BLOCK_SIZE  (64U)

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t buffer[SHA1_BLOCK_SIZE];
} Sha1Context;

/** @brief Initialise a SHA-1 context. */
void sha1_init(Sha1Context* context);

/** @brief Feed @p len bytes from @p data into the running digest. */
void sha1_update(Sha1Context* context, const uint8_t* data, size_t len);

/** @brief Finalise the digest, writing SHA1_DIGEST_SIZE bytes to @p digest. */
void sha1_final(Sha1Context* context, uint8_t digest[SHA1_DIGEST_SIZE]);

/**
 * @brief One-shot convenience helper.
 *
 * @param[in]  data   input buffer
 * @param[in]  len    input length in bytes
 * @param[out] digest destination for the 20-byte digest
 */
void sha1(const uint8_t* data, size_t len, uint8_t digest[SHA1_DIGEST_SIZE]);

#ifdef __cplusplus
}
#endif
