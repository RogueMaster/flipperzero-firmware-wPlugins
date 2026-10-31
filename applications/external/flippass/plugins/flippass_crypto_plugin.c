#include "flippass_crypto_plugin.h"

#include "../kdbx/hmac.h"
#include "../kdbx/kdbx_protected.h"
#include "../kdbx/memzero.h"
#include "../kdbx/sha2.h"

_Static_assert(
    sizeof(HMAC_SHA256_CTX) <= sizeof(FlipPassCryptoHmacSha256State),
    "FlipPassCryptoHmacSha256State is too small.");
_Static_assert(
    sizeof(KDBXProtectedStream) <= sizeof(FlipPassCryptoProtectedState),
    "FlipPassCryptoProtectedState is too small.");

static bool flippass_crypto_chunks_valid(const FlipPassCryptoChunk* chunks, size_t chunk_count) {
    if(chunks == NULL && chunk_count > 0U) {
        return false;
    }

    for(size_t index = 0U; index < chunk_count; index++) {
        if(chunks[index].data == NULL && chunks[index].size > 0U) {
            return false;
        }
    }

    return true;
}

static HMAC_SHA256_CTX*
    flippass_crypto_hmac_sha256_state_cast(FlipPassCryptoHmacSha256State* state) {
    return state != NULL ? (HMAC_SHA256_CTX*)state->storage : NULL;
}

static KDBXProtectedStream*
    flippass_crypto_protected_state_cast(FlipPassCryptoProtectedState* state) {
    return state != NULL ? (KDBXProtectedStream*)state->storage : NULL;
}

static bool flippass_crypto_sha256_chunks_impl(
    const FlipPassCryptoChunk* chunks,
    size_t chunk_count,
    uint8_t out[32]) {
    SHA256_CTX ctx;

    if(out == NULL || !flippass_crypto_chunks_valid(chunks, chunk_count)) {
        return false;
    }

    sha256_Init(&ctx);
    for(size_t index = 0U; index < chunk_count; index++) {
        if(chunks[index].size > 0U) {
            sha256_Update(&ctx, chunks[index].data, chunks[index].size);
        }
    }
    sha256_Final(&ctx, out);
    memzero(&ctx, sizeof(ctx));
    return true;
}

static bool flippass_crypto_sha512_chunks_impl(
    const FlipPassCryptoChunk* chunks,
    size_t chunk_count,
    uint8_t out[64]) {
    SHA512_CTX ctx;

    if(out == NULL || !flippass_crypto_chunks_valid(chunks, chunk_count)) {
        return false;
    }

    sha512_Init(&ctx);
    for(size_t index = 0U; index < chunk_count; index++) {
        if(chunks[index].size > 0U) {
            sha512_Update(&ctx, chunks[index].data, chunks[index].size);
        }
    }
    sha512_Final(&ctx, out);
    memzero(&ctx, sizeof(ctx));
    return true;
}

static bool flippass_crypto_hmac_sha256_chunks_impl(
    const uint8_t* key,
    size_t key_size,
    const FlipPassCryptoChunk* chunks,
    size_t chunk_count,
    uint8_t out[32]) {
    HMAC_SHA256_CTX ctx;

    if(key == NULL || key_size > UINT32_MAX || out == NULL ||
       !flippass_crypto_chunks_valid(chunks, chunk_count)) {
        return false;
    }

    hmac_sha256_Init(&ctx, key, (uint32_t)key_size);
    for(size_t index = 0U; index < chunk_count; index++) {
        if(chunks[index].size > UINT32_MAX) {
            memzero(&ctx, sizeof(ctx));
            return false;
        }
        if(chunks[index].size > 0U) {
            hmac_sha256_Update(&ctx, chunks[index].data, (uint32_t)chunks[index].size);
        }
    }
    hmac_sha256_Final(&ctx, out);
    memzero(&ctx, sizeof(ctx));
    return true;
}

static bool flippass_crypto_hmac_sha256_init_impl(
    FlipPassCryptoHmacSha256State* state,
    const uint8_t* key,
    size_t key_size) {
    HMAC_SHA256_CTX* ctx = flippass_crypto_hmac_sha256_state_cast(state);

    if(ctx == NULL || key == NULL || key_size > UINT32_MAX) {
        return false;
    }

    memset(state, 0, sizeof(*state));
    hmac_sha256_Init(ctx, key, (uint32_t)key_size);
    return true;
}

static bool flippass_crypto_hmac_sha256_update_impl(
    FlipPassCryptoHmacSha256State* state,
    const uint8_t* data,
    size_t data_size) {
    HMAC_SHA256_CTX* ctx = flippass_crypto_hmac_sha256_state_cast(state);

    if(ctx == NULL || (data == NULL && data_size > 0U) || data_size > UINT32_MAX) {
        return false;
    }
    if(data_size > 0U) {
        hmac_sha256_Update(ctx, data, (uint32_t)data_size);
    }
    return true;
}

static bool
    flippass_crypto_hmac_sha256_final_impl(FlipPassCryptoHmacSha256State* state, uint8_t out[32]) {
    HMAC_SHA256_CTX* ctx = flippass_crypto_hmac_sha256_state_cast(state);

    if(ctx == NULL || out == NULL) {
        return false;
    }

    hmac_sha256_Final(ctx, out);
    memzero(state, sizeof(*state));
    return true;
}

static void flippass_crypto_hmac_sha256_clear_impl(FlipPassCryptoHmacSha256State* state) {
    if(state != NULL) {
        memzero(state, sizeof(*state));
    }
}

static bool flippass_crypto_chacha20_xor_impl(
    uint8_t* data,
    size_t data_size,
    const uint8_t* key,
    size_t key_size,
    const uint8_t* nonce,
    size_t nonce_size,
    uint32_t counter) {
    if((data == NULL && data_size > 0U) || key == NULL || nonce == NULL) {
        return false;
    }

    return kdbx_chacha20_xor(data, data_size, key, key_size, nonce, nonce_size, counter);
}

static bool flippass_crypto_protected_stream_init_impl(
    FlipPassCryptoProtectedState* state,
    uint32_t algorithm,
    const uint8_t* key,
    size_t key_size) {
    KDBXProtectedStream* stream = flippass_crypto_protected_state_cast(state);

    if(stream == NULL || key == NULL) {
        return false;
    }

    memset(state, 0, sizeof(*state));
    return kdbx_protected_stream_init(
        stream, (KDBXProtectedStreamAlgorithm)algorithm, key, key_size);
}

static bool flippass_crypto_protected_stream_init_prederived_impl(
    FlipPassCryptoProtectedState* state,
    uint32_t algorithm,
    const uint8_t* material,
    size_t material_size) {
    KDBXProtectedStream* stream = flippass_crypto_protected_state_cast(state);

    if(stream == NULL || material == NULL) {
        return false;
    }

    memset(state, 0, sizeof(*state));
    return kdbx_protected_stream_init_prederived(
        stream, (KDBXProtectedStreamAlgorithm)algorithm, material, material_size);
}

static bool flippass_crypto_chacha20_stream_init_impl(
    FlipPassCryptoProtectedState* state,
    const uint8_t* key,
    size_t key_size,
    const uint8_t* nonce,
    size_t nonce_size,
    uint32_t counter) {
    KDBXProtectedStream* stream = flippass_crypto_protected_state_cast(state);

    if(stream == NULL || key == NULL || nonce == NULL) {
        return false;
    }

    memset(state, 0, sizeof(*state));
    return kdbx_chacha20_stream_init(stream, key, key_size, nonce, nonce_size, counter);
}

static bool flippass_crypto_protected_stream_apply_impl(
    FlipPassCryptoProtectedState* state,
    uint8_t* data,
    size_t data_size) {
    KDBXProtectedStream* stream = flippass_crypto_protected_state_cast(state);

    if(stream == NULL || (data == NULL && data_size > 0U)) {
        return false;
    }

    return kdbx_protected_stream_apply(stream, data, data_size);
}

static void flippass_crypto_protected_stream_clear_impl(FlipPassCryptoProtectedState* state) {
    KDBXProtectedStream* stream = flippass_crypto_protected_state_cast(state);

    if(stream != NULL) {
        kdbx_protected_stream_reset(stream);
    }
    if(state != NULL) {
        memzero(state, sizeof(*state));
    }
}

static const FlipPassCryptoPluginV1 flippass_crypto_plugin = {
    .api_version = FLIPPASS_CRYPTO_PLUGIN_API_VERSION,
    .sha256_chunks = flippass_crypto_sha256_chunks_impl,
    .sha512_chunks = flippass_crypto_sha512_chunks_impl,
    .hmac_sha256_chunks = flippass_crypto_hmac_sha256_chunks_impl,
    .hmac_sha256_init = flippass_crypto_hmac_sha256_init_impl,
    .hmac_sha256_update = flippass_crypto_hmac_sha256_update_impl,
    .hmac_sha256_final = flippass_crypto_hmac_sha256_final_impl,
    .hmac_sha256_clear = flippass_crypto_hmac_sha256_clear_impl,
    .chacha20_xor = flippass_crypto_chacha20_xor_impl,
    .protected_stream_init = flippass_crypto_protected_stream_init_impl,
    .protected_stream_init_prederived = flippass_crypto_protected_stream_init_prederived_impl,
    .chacha20_stream_init = flippass_crypto_chacha20_stream_init_impl,
    .protected_stream_apply = flippass_crypto_protected_stream_apply_impl,
    .protected_stream_clear = flippass_crypto_protected_stream_clear_impl,
};

static const FlipperAppPluginDescriptor flippass_crypto_descriptor = {
    .appid = FLIPPASS_CRYPTO_PLUGIN_APP_ID,
    .ep_api_version = FLIPPASS_CRYPTO_PLUGIN_API_VERSION,
    .entry_point = &flippass_crypto_plugin,
};

const FlipperAppPluginDescriptor* flippass_crypto_plugin_ep(void) {
    return &flippass_crypto_descriptor;
}

const FlipPassCryptoApiV1* flippass_crypto_plugin_api(void) {
    return &flippass_crypto_plugin;
}
