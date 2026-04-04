#include "kdbx_protected.h"
#include <stdlib.h>

#define KDBX_PROTECTED_CHACHA_BLOCK_SIZE 64U
#define KDBX_PROTECTED_SALSA_BLOCK_SIZE  64U

static inline uint32_t kdbx_read_u32_le(const uint8_t* data) {
    return ((uint32_t)data[0]) | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static inline void kdbx_write_u32_le(uint8_t* data, uint32_t value) {
    data[0] = (uint8_t)(value & 0xFFU);
    data[1] = (uint8_t)((value >> 8) & 0xFFU);
    data[2] = (uint8_t)((value >> 16) & 0xFFU);
    data[3] = (uint8_t)((value >> 24) & 0xFFU);
}

static inline uint32_t kdbx_rotl32(uint32_t value, uint8_t shift) {
    return (value << shift) | (value >> (32U - shift));
}

static bool kdbx_chacha20_init_state(
    uint32_t state[16],
    const uint8_t* key,
    size_t key_size,
    const uint8_t* nonce,
    size_t nonce_size,
    uint32_t counter) {
    static const uint32_t sigma[4] = {0x61707865U, 0x3320646EU, 0x79622D32U, 0x6B206574U};

    if(key == NULL || nonce == NULL || key_size != 32U || nonce_size != 12U) {
        return false;
    }

    state[0] = sigma[0];
    state[1] = sigma[1];
    state[2] = sigma[2];
    state[3] = sigma[3];
    for(size_t i = 0; i < 8U; i++) {
        state[4U + i] = kdbx_read_u32_le(&key[i * 4U]);
    }
    state[12] = counter;
    state[13] = kdbx_read_u32_le(&nonce[0]);
    state[14] = kdbx_read_u32_le(&nonce[4]);
    state[15] = kdbx_read_u32_le(&nonce[8]);
    return true;
}

static inline uint8_t kdbx_base64_value(char c) {
    if(c >= 'A' && c <= 'Z') return (uint8_t)(c - 'A');
    if(c >= 'a' && c <= 'z') return (uint8_t)(c - 'a' + 26);
    if(c >= '0' && c <= '9') return (uint8_t)(c - '0' + 52);
    if(c == '+') return 62;
    if(c == '/') return 63;
    return 0xFFU;
}

static bool kdbx_base64_decoded_size(const char* encoded, size_t* out_size) {
    size_t encoded_size;
    size_t padding = 0U;
    size_t alloc_size;

    furi_assert(encoded);
    furi_assert(out_size);

    encoded_size = strlen(encoded);
    if(encoded_size == 0U) {
        *out_size = 0U;
        return true;
    }

    if((encoded_size % 4U) != 0U) {
        return false;
    }

    if(encoded_size >= 1U && encoded[encoded_size - 1U] == '=') padding++;
    if(encoded_size >= 2U && encoded[encoded_size - 2U] == '=') padding++;

    alloc_size = (encoded_size / 4U) * 3U;
    if(padding > alloc_size) {
        return false;
    }
    alloc_size -= padding;
    *out_size = alloc_size;
    return true;
}

static bool kdbx_base64_decode_into(const char* encoded, uint8_t* decoded, size_t decoded_size) {
    const size_t encoded_size = strlen(encoded);
    size_t encoded_index = 0U;
    size_t decoded_index = 0U;

    furi_assert(encoded);
    furi_assert(decoded);

    while(encoded_index < encoded_size) {
        uint8_t quartet[4];
        uint32_t triple;

        for(size_t i = 0; i < 4U; i++) {
            const char c = encoded[encoded_index++];
            if(c == '=') {
                quartet[i] = 0;
            } else {
                quartet[i] = kdbx_base64_value(c);
                if(quartet[i] == 0xFFU) {
                    return false;
                }
            }
        }

        triple = ((uint32_t)quartet[0] << 18) | ((uint32_t)quartet[1] << 12) |
                 ((uint32_t)quartet[2] << 6) | (uint32_t)quartet[3];

        if(decoded_index < decoded_size) decoded[decoded_index++] = (uint8_t)(triple >> 16);
        if(decoded_index < decoded_size) decoded[decoded_index++] = (uint8_t)(triple >> 8);
        if(decoded_index < decoded_size) decoded[decoded_index++] = (uint8_t)triple;
    }

    return decoded_index == decoded_size;
}

bool kdbx_protected_value_decode_reuse(
    KDBXProtectedStream* stream,
    const char* encoded,
    char** decoded_value,
    size_t* decoded_size,
    uint8_t** buffer,
    size_t* buffer_capacity) {
    size_t required_size = 0U;

    furi_assert(stream);
    furi_assert(encoded);
    furi_assert(decoded_value);
    furi_assert(decoded_size);
    furi_assert(buffer);
    furi_assert(buffer_capacity);

    if(!kdbx_base64_decoded_size(encoded, &required_size)) {
        return false;
    }

    if(*buffer_capacity < (required_size + 1U)) {
        size_t next_capacity = (*buffer_capacity > 0U) ? *buffer_capacity : 32U;
        while(next_capacity < (required_size + 1U)) {
            if(next_capacity > (SIZE_MAX / 2U)) {
                next_capacity = required_size + 1U;
                break;
            }
            next_capacity *= 2U;
        }

        uint8_t* next_buffer = malloc(next_capacity);
        if(next_buffer == NULL) {
            return false;
        }

        if(*buffer != NULL) {
            memzero(*buffer, *buffer_capacity);
            free(*buffer);
        }

        *buffer = next_buffer;
        *buffer_capacity = next_capacity;
    }

    if(!kdbx_base64_decode_into(encoded, *buffer, required_size)) {
        memzero(*buffer, *buffer_capacity);
        return false;
    }

    if(!kdbx_protected_stream_apply(stream, *buffer, required_size)) {
        memzero(*buffer, *buffer_capacity);
        return false;
    }

    (*buffer)[required_size] = '\0';
    *decoded_value = (char*)*buffer;
    *decoded_size = required_size;
    return true;
}

bool kdbx_protected_value_discard(KDBXProtectedStream* stream, const char* encoded) {
    uint8_t decoded_chunk[192];
    const size_t encoded_size = strlen(encoded);
    size_t encoded_index = 0U;
    size_t decoded_index = 0U;

    furi_assert(stream);
    furi_assert(encoded);

    if(encoded_size == 0U) {
        return true;
    }

    if((encoded_size % 4U) != 0U) {
        return false;
    }

    while(encoded_index < encoded_size) {
        uint8_t quartet[4];
        uint32_t triple;
        size_t bytes_to_emit = 3U;

        for(size_t i = 0U; i < 4U; i++) {
            const char c = encoded[encoded_index++];
            if(c == '=') {
                quartet[i] = 0U;
                if(i < 2U) {
                    return false;
                }
                bytes_to_emit--;
            } else {
                quartet[i] = kdbx_base64_value(c);
                if(quartet[i] == 0xFFU) {
                    return false;
                }
            }
        }

        triple = ((uint32_t)quartet[0] << 18) | ((uint32_t)quartet[1] << 12) |
                 ((uint32_t)quartet[2] << 6) | (uint32_t)quartet[3];

        if(bytes_to_emit >= 1U) {
            decoded_chunk[decoded_index++] = (uint8_t)(triple >> 16);
        }
        if(bytes_to_emit >= 2U) {
            decoded_chunk[decoded_index++] = (uint8_t)(triple >> 8);
        }
        if(bytes_to_emit >= 3U) {
            decoded_chunk[decoded_index++] = (uint8_t)triple;
        }

        if(decoded_index >= sizeof(decoded_chunk) - 3U || encoded_index == encoded_size) {
            if(decoded_index > 0U) {
                if(!kdbx_protected_stream_apply(stream, decoded_chunk, decoded_index)) {
                    memzero(decoded_chunk, sizeof(decoded_chunk));
                    return false;
                }
                memzero(decoded_chunk, decoded_index);
                decoded_index = 0U;
            }
        }
    }

    memzero(decoded_chunk, sizeof(decoded_chunk));
    return true;
}

void kdbx_protected_discard_state_init(KDBXProtectedDiscardState* state) {
    if(state == NULL) {
        return;
    }

    memzero(state, sizeof(*state));
}

static bool kdbx_protected_discard_state_flush(
    KDBXProtectedStream* stream,
    KDBXProtectedDiscardState* state,
    KDBXProtectedChunkCallback callback,
    void* context) {
    if(state->chunk_len == 0U) {
        return true;
    }

    if(!kdbx_protected_stream_apply(stream, state->chunk, state->chunk_len)) {
        memzero(state->chunk, sizeof(state->chunk));
        state->chunk_len = 0U;
        return false;
    }

    if(callback != NULL && !callback(state->chunk, state->chunk_len, context)) {
        memzero(state->chunk, sizeof(state->chunk));
        state->chunk_len = 0U;
        return false;
    }

    memzero(state->chunk, state->chunk_len);
    state->chunk_len = 0U;
    return true;
}

static bool kdbx_protected_discard_state_emit_quartet(
    KDBXProtectedStream* stream,
    KDBXProtectedDiscardState* state,
    KDBXProtectedChunkCallback callback,
    void* context) {
    uint8_t quartet[4];
    uint32_t triple;
    size_t bytes_to_emit = 3U;

    for(size_t i = 0U; i < 4U; i++) {
        const char c = state->quartet[i];
        if(c == '=') {
            quartet[i] = 0U;
            if(i < 2U) {
                return false;
            }
            bytes_to_emit--;
        } else {
            quartet[i] = kdbx_base64_value(c);
            if(quartet[i] == 0xFFU) {
                return false;
            }
        }
    }

    triple = ((uint32_t)quartet[0] << 18) | ((uint32_t)quartet[1] << 12) |
             ((uint32_t)quartet[2] << 6) | (uint32_t)quartet[3];

    if(bytes_to_emit >= 1U) {
        state->chunk[state->chunk_len++] = (uint8_t)(triple >> 16);
    }
    if(bytes_to_emit >= 2U) {
        state->chunk[state->chunk_len++] = (uint8_t)(triple >> 8);
    }
    if(bytes_to_emit >= 3U) {
        state->chunk[state->chunk_len++] = (uint8_t)triple;
    }
    state->quartet_len = 0U;

    if(state->chunk_len >= sizeof(state->chunk) - 3U) {
        return kdbx_protected_discard_state_flush(stream, state, callback, context);
    }

    return true;
}

bool kdbx_protected_decode_state_update(
    KDBXProtectedStream* stream,
    KDBXProtectedDiscardState* state,
    const char* encoded,
    size_t encoded_size,
    KDBXProtectedChunkCallback callback,
    void* context) {
    furi_assert(stream);
    furi_assert(state);
    furi_assert(encoded);

    for(size_t i = 0U; i < encoded_size; i++) {
        const char c = encoded[i];
        if(c == '\r' || c == '\n' || c == '\t' || c == ' ') {
            continue;
        }

        state->quartet[state->quartet_len++] = c;
        if(state->quartet_len == 4U) {
            if(!kdbx_protected_discard_state_emit_quartet(stream, state, callback, context)) {
                return false;
            }
        }
    }

    return true;
}

bool kdbx_protected_decode_state_finalize(
    KDBXProtectedStream* stream,
    KDBXProtectedDiscardState* state,
    KDBXProtectedChunkCallback callback,
    void* context) {
    furi_assert(stream);
    furi_assert(state);

    if(state->quartet_len != 0U) {
        return false;
    }

    if(!kdbx_protected_discard_state_flush(stream, state, callback, context)) {
        return false;
    }

    kdbx_protected_discard_state_init(state);
    return true;
}

bool kdbx_protected_discard_state_update(
    KDBXProtectedStream* stream,
    KDBXProtectedDiscardState* state,
    const char* encoded,
    size_t encoded_size) {
    return kdbx_protected_decode_state_update(
        stream, state, encoded, encoded_size, NULL, NULL);
}

bool kdbx_protected_discard_state_finalize(
    KDBXProtectedStream* stream,
    KDBXProtectedDiscardState* state) {
    return kdbx_protected_decode_state_finalize(stream, state, NULL, NULL);
}

static void kdbx_chacha20_generate_block(KDBXProtectedStream* stream) {
    static const uint32_t sigma[4] = {0x61707865U, 0x3320646EU, 0x79622D32U, 0x6B206574U};
    uint32_t working[16];

    memcpy(working, stream->state.chacha20.state, sizeof(working));
    working[0] = sigma[0];
    working[1] = sigma[1];
    working[2] = sigma[2];
    working[3] = sigma[3];

#define KDBX_CHACHA_QR(a, b, c, d)                   \
    do {                                             \
        working[(a)] += working[(b)];                \
        working[(d)] = kdbx_rotl32(                  \
            working[(d)] ^ working[(a)], 16);        \
        working[(c)] += working[(d)];                \
        working[(b)] = kdbx_rotl32(                  \
            working[(b)] ^ working[(c)], 12);        \
        working[(a)] += working[(b)];                \
        working[(d)] = kdbx_rotl32(                  \
            working[(d)] ^ working[(a)], 8);         \
        working[(c)] += working[(d)];                \
        working[(b)] = kdbx_rotl32(                  \
            working[(b)] ^ working[(c)], 7);         \
    } while(false)

    for(size_t round = 0; round < 10U; round++) {
        KDBX_CHACHA_QR(0, 4, 8, 12);
        KDBX_CHACHA_QR(1, 5, 9, 13);
        KDBX_CHACHA_QR(2, 6, 10, 14);
        KDBX_CHACHA_QR(3, 7, 11, 15);

        KDBX_CHACHA_QR(0, 5, 10, 15);
        KDBX_CHACHA_QR(1, 6, 11, 12);
        KDBX_CHACHA_QR(2, 7, 8, 13);
        KDBX_CHACHA_QR(3, 4, 9, 14);
    }

#undef KDBX_CHACHA_QR

    for(size_t i = 0; i < 16U; i++) {
        working[i] += stream->state.chacha20.state[i];
        kdbx_write_u32_le(&stream->block[i * 4U], working[i]);
    }

    stream->state.chacha20.state[12]++;
    if(stream->state.chacha20.state[12] == 0U) {
        stream->state.chacha20.state[13]++;
    }
    stream->block_offset = 0U;
    memzero(working, sizeof(working));
}

static void kdbx_salsa20_generate_block(KDBXProtectedStream* stream) {
    uint32_t working[16];

    memcpy(working, stream->state.salsa20.state, sizeof(working));

#define KDBX_SALSA_QR(a, b, c, d)                         \
    do {                                                  \
        working[(b)] ^= kdbx_rotl32(                      \
            working[(a)] + working[(d)], 7);              \
        working[(c)] ^= kdbx_rotl32(                      \
            working[(b)] + working[(a)], 9);              \
        working[(d)] ^= kdbx_rotl32(                      \
            working[(c)] + working[(b)], 13);             \
        working[(a)] ^= kdbx_rotl32(                      \
            working[(d)] + working[(c)], 18);             \
    } while(false)

    for(size_t round = 0; round < 10U; round++) {
        KDBX_SALSA_QR(0, 4, 8, 12);
        KDBX_SALSA_QR(5, 9, 13, 1);
        KDBX_SALSA_QR(10, 14, 2, 6);
        KDBX_SALSA_QR(15, 3, 7, 11);

        KDBX_SALSA_QR(0, 1, 2, 3);
        KDBX_SALSA_QR(5, 6, 7, 4);
        KDBX_SALSA_QR(10, 11, 8, 9);
        KDBX_SALSA_QR(15, 12, 13, 14);
    }

#undef KDBX_SALSA_QR

    for(size_t i = 0; i < 16U; i++) {
        working[i] += stream->state.salsa20.state[i];
        kdbx_write_u32_le(&stream->block[i * 4U], working[i]);
    }

    stream->state.salsa20.state[8]++;
    if(stream->state.salsa20.state[8] == 0U) {
        stream->state.salsa20.state[9]++;
    }
    stream->block_offset = 0U;
    memzero(working, sizeof(working));
}

void kdbx_protected_stream_reset(KDBXProtectedStream* stream) {
    if(stream == NULL) {
        return;
    }

    memzero(stream, sizeof(KDBXProtectedStream));
}

bool kdbx_protected_stream_init(
    KDBXProtectedStream* stream,
    KDBXProtectedStreamAlgorithm algorithm,
    const uint8_t* key,
    size_t key_size) {
    uint8_t hash[SHA512_DIGEST_LENGTH];
    uint8_t hash32[SHA256_DIGEST_LENGTH];

    furi_assert(stream);
    furi_assert(key);

    kdbx_protected_stream_reset(stream);

    if(algorithm == KDBXProtectedStreamNone) {
        stream->ready = false;
        return true;
    }
    if(algorithm == KDBXProtectedStreamArcFourVariant) {
        return false;
    }

    if(algorithm == KDBXProtectedStreamChaCha20) {
        sha512_Raw(key, key_size, hash);
        stream->algorithm = algorithm;
        stream->ready = kdbx_chacha20_init_state(
            stream->state.chacha20.state, hash, 32U, &hash[32], 12U, 0U);
        stream->block_offset = sizeof(stream->block);
        memzero(hash, sizeof(hash));
        return stream->ready;
    }

    if(algorithm == KDBXProtectedStreamSalsa20) {
        static const uint32_t sigma[4] = {0x61707865U, 0x3320646EU, 0x79622D32U, 0x6B206574U};
        static const uint8_t nonce[8] = {0xE8U, 0x30U, 0x09U, 0x4BU, 0x97U, 0x20U, 0x5DU, 0x2AU};
        sha256_Raw(key, key_size, hash32);
        stream->algorithm = algorithm;
        stream->ready = true;
        stream->block_offset = sizeof(stream->block);
        stream->state.salsa20.state[0] = sigma[0];
        stream->state.salsa20.state[1] = kdbx_read_u32_le(&hash32[0]);
        stream->state.salsa20.state[2] = kdbx_read_u32_le(&hash32[4]);
        stream->state.salsa20.state[3] = kdbx_read_u32_le(&hash32[8]);
        stream->state.salsa20.state[4] = kdbx_read_u32_le(&hash32[12]);
        stream->state.salsa20.state[5] = sigma[1];
        stream->state.salsa20.state[6] = kdbx_read_u32_le(&nonce[0]);
        stream->state.salsa20.state[7] = kdbx_read_u32_le(&nonce[4]);
        stream->state.salsa20.state[8] = 0U;
        stream->state.salsa20.state[9] = 0U;
        stream->state.salsa20.state[10] = sigma[2];
        stream->state.salsa20.state[11] = kdbx_read_u32_le(&hash32[16]);
        stream->state.salsa20.state[12] = kdbx_read_u32_le(&hash32[20]);
        stream->state.salsa20.state[13] = kdbx_read_u32_le(&hash32[24]);
        stream->state.salsa20.state[14] = kdbx_read_u32_le(&hash32[28]);
        stream->state.salsa20.state[15] = sigma[3];
        memzero(hash32, sizeof(hash32));
        return true;
    }

    return false;
}

bool kdbx_protected_stream_apply(
    KDBXProtectedStream* stream,
    uint8_t* data,
    size_t data_size) {
    furi_assert(stream);
    furi_assert(data);

    if(!stream->ready) {
        return false;
    }

    for(size_t i = 0; i < data_size; i++) {
        if(stream->block_offset >= sizeof(stream->block)) {
            if(stream->algorithm == KDBXProtectedStreamChaCha20) {
                kdbx_chacha20_generate_block(stream);
            } else if(stream->algorithm == KDBXProtectedStreamSalsa20) {
                kdbx_salsa20_generate_block(stream);
            } else {
                return false;
            }
        }

        data[i] ^= stream->block[stream->block_offset++];
    }

    return true;
}

bool kdbx_protected_value_decode(
    KDBXProtectedStream* stream,
    const char* encoded,
    char** decoded_value,
    size_t* decoded_size) {
    uint8_t* decoded = NULL;
    size_t size = 0U;
    size_t capacity = 0U;

    furi_assert(stream);
    furi_assert(encoded);
    furi_assert(decoded_value);
    furi_assert(decoded_size);

    if(!kdbx_protected_value_decode_reuse(
           stream, encoded, decoded_value, &size, &decoded, &capacity)) {
        return false;
    }

    *decoded_size = size;
    return true;
}

bool kdbx_chacha20_stream_init(
    KDBXProtectedStream* stream,
    const uint8_t* key,
    size_t key_size,
    const uint8_t* nonce,
    size_t nonce_size,
    uint32_t counter) {
    furi_assert(stream);
    furi_assert(key);
    furi_assert(nonce);

    kdbx_protected_stream_reset(stream);
    stream->algorithm = KDBXProtectedStreamChaCha20;
    stream->ready =
        kdbx_chacha20_init_state(stream->state.chacha20.state, key, key_size, nonce, nonce_size, counter);
    stream->block_offset = sizeof(stream->block);
    return stream->ready;
}

bool kdbx_chacha20_xor(
    uint8_t* data,
    size_t data_size,
    const uint8_t* key,
    size_t key_size,
    const uint8_t* nonce,
    size_t nonce_size,
    uint32_t counter) {
    KDBXProtectedStream stream;

    furi_assert(data);
    furi_assert(key);
    furi_assert(nonce);

    kdbx_protected_stream_reset(&stream);
    stream.algorithm = KDBXProtectedStreamChaCha20;
    stream.ready = kdbx_chacha20_init_state(
        stream.state.chacha20.state, key, key_size, nonce, nonce_size, counter);
    stream.block_offset = sizeof(stream.block);
    if(!stream.ready) {
        return false;
    }

    return kdbx_protected_stream_apply(&stream, data, data_size);
}
