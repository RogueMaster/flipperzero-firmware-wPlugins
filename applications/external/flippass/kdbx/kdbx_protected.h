#pragma once

#include "kdbx_includes.h"

typedef enum {
    KDBXProtectedStreamNone = 0,
    KDBXProtectedStreamArcFourVariant = 1,
    KDBXProtectedStreamSalsa20 = 2,
    KDBXProtectedStreamChaCha20 = 3,
} KDBXProtectedStreamAlgorithm;

typedef struct {
    bool ready;
    KDBXProtectedStreamAlgorithm algorithm;
    uint8_t block[64];
    size_t block_offset;
    union {
        struct {
            uint32_t state[16];
        } chacha20;
        struct {
            uint32_t state[16];
        } salsa20;
    } state;
} KDBXProtectedStream;

typedef struct {
    char quartet[4];
    size_t quartet_len;
    uint8_t chunk[192];
    size_t chunk_len;
} KDBXProtectedDiscardState;

typedef bool (*KDBXProtectedChunkCallback)(const uint8_t* data, size_t data_size, void* context);

void kdbx_protected_stream_reset(KDBXProtectedStream* stream);
bool kdbx_protected_stream_init(
    KDBXProtectedStream* stream,
    KDBXProtectedStreamAlgorithm algorithm,
    const uint8_t* key,
    size_t key_size);
bool kdbx_protected_stream_apply(
    KDBXProtectedStream* stream,
    uint8_t* data,
    size_t data_size);
bool kdbx_chacha20_stream_init(
    KDBXProtectedStream* stream,
    const uint8_t* key,
    size_t key_size,
    const uint8_t* nonce,
    size_t nonce_size,
    uint32_t counter);
bool kdbx_protected_value_decode(
    KDBXProtectedStream* stream,
    const char* encoded,
    char** decoded_value,
    size_t* decoded_size);
bool kdbx_protected_value_decode_reuse(
    KDBXProtectedStream* stream,
    const char* encoded,
    char** decoded_value,
    size_t* decoded_size,
    uint8_t** buffer,
    size_t* buffer_capacity);
bool kdbx_protected_value_discard(
    KDBXProtectedStream* stream,
    const char* encoded);
void kdbx_protected_discard_state_init(KDBXProtectedDiscardState* state);
bool kdbx_protected_decode_state_update(
    KDBXProtectedStream* stream,
    KDBXProtectedDiscardState* state,
    const char* encoded,
    size_t encoded_size,
    KDBXProtectedChunkCallback callback,
    void* context);
bool kdbx_protected_decode_state_finalize(
    KDBXProtectedStream* stream,
    KDBXProtectedDiscardState* state,
    KDBXProtectedChunkCallback callback,
    void* context);
bool kdbx_protected_discard_state_update(
    KDBXProtectedStream* stream,
    KDBXProtectedDiscardState* state,
    const char* encoded,
    size_t encoded_size);
bool kdbx_protected_discard_state_finalize(
    KDBXProtectedStream* stream,
    KDBXProtectedDiscardState* state);
bool kdbx_chacha20_xor(
    uint8_t* data,
    size_t data_size,
    const uint8_t* key,
    size_t key_size,
    const uint8_t* nonce,
    size_t nonce_size,
    uint32_t counter);
