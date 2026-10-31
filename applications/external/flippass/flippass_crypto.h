#pragma once

#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const uint8_t* data;
    size_t size;
} FlipPassCryptoChunk;

#define FLIPPASS_CRYPTO_API_VERSION 2u

typedef struct {
    uint64_t storage[24];
} FlipPassCryptoHmacSha256State;

typedef struct {
    uint64_t storage[32];
} FlipPassCryptoProtectedState;

typedef struct {
    uint32_t api_version;
    bool (*sha256_chunks)(const FlipPassCryptoChunk* chunks, size_t chunk_count, uint8_t out[32]);
    bool (*sha512_chunks)(const FlipPassCryptoChunk* chunks, size_t chunk_count, uint8_t out[64]);
    bool (*hmac_sha256_chunks)(
        const uint8_t* key,
        size_t key_size,
        const FlipPassCryptoChunk* chunks,
        size_t chunk_count,
        uint8_t out[32]);
    bool (*hmac_sha256_init)(
        FlipPassCryptoHmacSha256State* state,
        const uint8_t* key,
        size_t key_size);
    bool (*hmac_sha256_update)(
        FlipPassCryptoHmacSha256State* state,
        const uint8_t* data,
        size_t data_size);
    bool (*hmac_sha256_final)(FlipPassCryptoHmacSha256State* state, uint8_t out[32]);
    void (*hmac_sha256_clear)(FlipPassCryptoHmacSha256State* state);
    bool (*chacha20_xor)(
        uint8_t* data,
        size_t data_size,
        const uint8_t* key,
        size_t key_size,
        const uint8_t* nonce,
        size_t nonce_size,
        uint32_t counter);
    bool (*protected_stream_init)(
        FlipPassCryptoProtectedState* state,
        uint32_t algorithm,
        const uint8_t* key,
        size_t key_size);
    bool (*protected_stream_init_prederived)(
        FlipPassCryptoProtectedState* state,
        uint32_t algorithm,
        const uint8_t* material,
        size_t material_size);
    bool (*chacha20_stream_init)(
        FlipPassCryptoProtectedState* state,
        const uint8_t* key,
        size_t key_size,
        const uint8_t* nonce,
        size_t nonce_size,
        uint32_t counter);
    bool (*protected_stream_apply)(
        FlipPassCryptoProtectedState* state,
        uint8_t* data,
        size_t data_size);
    void (*protected_stream_clear)(FlipPassCryptoProtectedState* state);
} FlipPassCryptoApiV1;

void flippass_crypto_set_app(void* app);
void flippass_crypto_clear_app(void* app);

const FlipPassCryptoApiV1* flippass_crypto_get_api(void);

/* The caller pins the provider module for the entire scope and ends the
 * scope before unpinning/unmapping it. Binding never allocates or evicts. */
bool flippass_crypto_begin_scope(const FlipPassCryptoApiV1* api);
void flippass_crypto_end_scope(void);

bool flippass_crypto_sha256_chunks(
    const FlipPassCryptoChunk* chunks,
    size_t chunk_count,
    uint8_t out[32]);
bool flippass_crypto_sha256_raw(const uint8_t* data, size_t data_size, uint8_t out[32]);
bool flippass_crypto_sha512_chunks(
    const FlipPassCryptoChunk* chunks,
    size_t chunk_count,
    uint8_t out[64]);
bool flippass_crypto_sha512_raw(const uint8_t* data, size_t data_size, uint8_t out[64]);
bool flippass_crypto_hmac_sha256_chunks(
    const uint8_t* key,
    size_t key_size,
    const FlipPassCryptoChunk* chunks,
    size_t chunk_count,
    uint8_t out[32]);
bool flippass_crypto_chacha20_xor(
    uint8_t* data,
    size_t data_size,
    const uint8_t* key,
    size_t key_size,
    const uint8_t* nonce,
    size_t nonce_size,
    uint32_t counter);
bool flippass_crypto_hmac_sha256_init(
    FlipPassCryptoHmacSha256State* state,
    const uint8_t* key,
    size_t key_size);
bool flippass_crypto_hmac_sha256_update(
    FlipPassCryptoHmacSha256State* state,
    const uint8_t* data,
    size_t data_size);
bool flippass_crypto_hmac_sha256_final(FlipPassCryptoHmacSha256State* state, uint8_t out[32]);
void flippass_crypto_hmac_sha256_clear(FlipPassCryptoHmacSha256State* state);
bool flippass_crypto_protected_stream_init(
    FlipPassCryptoProtectedState* state,
    uint32_t algorithm,
    const uint8_t* key,
    size_t key_size);
bool flippass_crypto_protected_stream_init_prederived(
    FlipPassCryptoProtectedState* state,
    uint32_t algorithm,
    const uint8_t* material,
    size_t material_size);
bool flippass_crypto_chacha20_stream_init(
    FlipPassCryptoProtectedState* state,
    const uint8_t* key,
    size_t key_size,
    const uint8_t* nonce,
    size_t nonce_size,
    uint32_t counter);
bool flippass_crypto_protected_stream_apply(
    FlipPassCryptoProtectedState* state,
    uint8_t* data,
    size_t data_size);
void flippass_crypto_protected_stream_clear(FlipPassCryptoProtectedState* state);

#ifdef __cplusplus
}
#endif
