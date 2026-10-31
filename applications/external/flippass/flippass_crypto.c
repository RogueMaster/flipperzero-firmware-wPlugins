#include "flippass.h"
#include "flippass_crypto.h"
#include "kdbx/memzero.h"
#include "plugins/flippass_crypto_plugin.h"

static App* flippass_crypto_app = NULL;
static const FlipPassCryptoApiV1* flippass_crypto_scope = NULL;

static bool flippass_crypto_api_valid(const FlipPassCryptoApiV1* api) {
    return api != NULL && api->api_version == FLIPPASS_CRYPTO_API_VERSION &&
           api->sha256_chunks != NULL && api->sha512_chunks != NULL &&
           api->hmac_sha256_chunks != NULL && api->hmac_sha256_init != NULL &&
           api->hmac_sha256_update != NULL && api->hmac_sha256_final != NULL &&
           api->hmac_sha256_clear != NULL && api->chacha20_xor != NULL &&
           api->protected_stream_init != NULL && api->protected_stream_init_prederived != NULL &&
           api->chacha20_stream_init != NULL && api->protected_stream_apply != NULL &&
           api->protected_stream_clear != NULL;
}

bool flippass_crypto_begin_scope(const FlipPassCryptoApiV1* api) {
    if(flippass_crypto_app == NULL || flippass_crypto_scope != NULL ||
       !flippass_crypto_api_valid(api)) {
        return false;
    }
    flippass_crypto_scope = api;
    return true;
}

void flippass_crypto_end_scope(void) {
    flippass_crypto_scope = NULL;
}

void flippass_crypto_set_app(void* app) {
    flippass_crypto_app = app;
}

void flippass_crypto_clear_app(void* app) {
    if(flippass_crypto_app == app) {
        flippass_crypto_end_scope();
        flippass_crypto_app = NULL;
    }
}

static const FlipPassCryptoPluginV1* flippass_crypto_plugin_ensure(void) {
    if(flippass_crypto_app == NULL) {
        return NULL;
    }
    if(flippass_crypto_scope != NULL) return flippass_crypto_scope;

    const FlipperAppPluginDescriptor* descriptor = flippass_module_ensure(
        flippass_crypto_app,
        FlipPassModuleSlotCrypto,
        NULL,
        FLIPPASS_CRYPTO_PLUGIN_APP_ID,
        FLIPPASS_CRYPTO_PLUGIN_API_VERSION,
        NULL);
    if(descriptor == NULL || descriptor->entry_point == NULL) {
        return NULL;
    }

    const FlipPassCryptoPluginV1* plugin = descriptor->entry_point;
    if(!flippass_crypto_api_valid(plugin)) {
        flippass_module_unload(flippass_crypto_app, FlipPassModuleSlotCrypto);
        return NULL;
    }

    return plugin;
}

const FlipPassCryptoApiV1* flippass_crypto_get_api(void) {
    return flippass_crypto_plugin_ensure();
}

bool flippass_crypto_sha256_chunks(
    const FlipPassCryptoChunk* chunks,
    size_t chunk_count,
    uint8_t out[32]) {
    const FlipPassCryptoPluginV1* plugin = flippass_crypto_plugin_ensure();
    return plugin != NULL && plugin->sha256_chunks(chunks, chunk_count, out);
}

bool flippass_crypto_sha256_raw(const uint8_t* data, size_t data_size, uint8_t out[32]) {
    const FlipPassCryptoChunk chunk = {.data = data, .size = data_size};
    return flippass_crypto_sha256_chunks(&chunk, 1U, out);
}

bool flippass_crypto_sha512_chunks(
    const FlipPassCryptoChunk* chunks,
    size_t chunk_count,
    uint8_t out[64]) {
    const FlipPassCryptoPluginV1* plugin = flippass_crypto_plugin_ensure();
    return plugin != NULL && plugin->sha512_chunks(chunks, chunk_count, out);
}

bool flippass_crypto_sha512_raw(const uint8_t* data, size_t data_size, uint8_t out[64]) {
    const FlipPassCryptoChunk chunk = {.data = data, .size = data_size};
    return flippass_crypto_sha512_chunks(&chunk, 1U, out);
}

bool flippass_crypto_hmac_sha256_chunks(
    const uint8_t* key,
    size_t key_size,
    const FlipPassCryptoChunk* chunks,
    size_t chunk_count,
    uint8_t out[32]) {
    const FlipPassCryptoPluginV1* plugin = flippass_crypto_plugin_ensure();
    return plugin != NULL && plugin->hmac_sha256_chunks(key, key_size, chunks, chunk_count, out);
}

bool flippass_crypto_chacha20_xor(
    uint8_t* data,
    size_t data_size,
    const uint8_t* key,
    size_t key_size,
    const uint8_t* nonce,
    size_t nonce_size,
    uint32_t counter) {
    const FlipPassCryptoPluginV1* plugin = flippass_crypto_plugin_ensure();
    return plugin != NULL &&
           plugin->chacha20_xor(data, data_size, key, key_size, nonce, nonce_size, counter);
}

bool flippass_crypto_hmac_sha256_init(
    FlipPassCryptoHmacSha256State* state,
    const uint8_t* key,
    size_t key_size) {
    const FlipPassCryptoPluginV1* plugin = flippass_crypto_plugin_ensure();
    return plugin != NULL && plugin->hmac_sha256_init(state, key, key_size);
}

bool flippass_crypto_hmac_sha256_update(
    FlipPassCryptoHmacSha256State* state,
    const uint8_t* data,
    size_t data_size) {
    const FlipPassCryptoPluginV1* plugin = flippass_crypto_plugin_ensure();
    return plugin != NULL && plugin->hmac_sha256_update(state, data, data_size);
}

bool flippass_crypto_hmac_sha256_final(FlipPassCryptoHmacSha256State* state, uint8_t out[32]) {
    const FlipPassCryptoPluginV1* plugin = flippass_crypto_plugin_ensure();
    return plugin != NULL && plugin->hmac_sha256_final(state, out);
}

void flippass_crypto_hmac_sha256_clear(FlipPassCryptoHmacSha256State* state) {
    if(state != NULL) {
        memzero(state, sizeof(*state));
    }
}

bool flippass_crypto_protected_stream_init(
    FlipPassCryptoProtectedState* state,
    uint32_t algorithm,
    const uint8_t* key,
    size_t key_size) {
    const FlipPassCryptoPluginV1* plugin = flippass_crypto_plugin_ensure();
    return plugin != NULL && plugin->protected_stream_init(state, algorithm, key, key_size);
}

bool flippass_crypto_protected_stream_init_prederived(
    FlipPassCryptoProtectedState* state,
    uint32_t algorithm,
    const uint8_t* material,
    size_t material_size) {
    const FlipPassCryptoPluginV1* plugin = flippass_crypto_plugin_ensure();
    return plugin != NULL &&
           plugin->protected_stream_init_prederived(state, algorithm, material, material_size);
}

bool flippass_crypto_chacha20_stream_init(
    FlipPassCryptoProtectedState* state,
    const uint8_t* key,
    size_t key_size,
    const uint8_t* nonce,
    size_t nonce_size,
    uint32_t counter) {
    const FlipPassCryptoPluginV1* plugin = flippass_crypto_plugin_ensure();
    return plugin != NULL &&
           plugin->chacha20_stream_init(state, key, key_size, nonce, nonce_size, counter);
}

bool flippass_crypto_protected_stream_apply(
    FlipPassCryptoProtectedState* state,
    uint8_t* data,
    size_t data_size) {
    const FlipPassCryptoPluginV1* plugin = flippass_crypto_plugin_ensure();
    return plugin != NULL && plugin->protected_stream_apply(state, data, data_size);
}

void flippass_crypto_protected_stream_clear(FlipPassCryptoProtectedState* state) {
    if(state != NULL) {
        memzero(state, sizeof(*state));
    }
}
