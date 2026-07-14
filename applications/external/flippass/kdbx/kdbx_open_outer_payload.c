#include "kdbx_open_stream.h"

#include "kdbx_protected.h"
#include "memzero.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KDBX_OPEN_HEADER_VERIFICATION_BYTES 64U
#define KDBX_OPEN_STREAM_IO_CHUNK_SIZE      256U
#define KDBX_OPEN_MAX_STREAM_OUTPUT_SIZE    (2U * 1024U * 1024U)

typedef struct {
    aes_decrypt_ctx aes_ctx;
    uint8_t iv[16];
    uint8_t partial_block[16];
    size_t partial_len;
    uint8_t pending_block[16];
    bool has_pending_block;
} KDBXOpenOuterAesState;

static void kdbx_open_outer_set_error(char* error, size_t error_size, const char* format, ...) {
    va_list args;

    if(error == NULL || error_size == 0U || format == NULL) {
        return;
    }

    va_start(args, format);
    vsnprintf(error, error_size, format, args);
    va_end(args);
}

static void* kdbx_open_outer_alloc_aligned(size_t size, size_t alignment, void** raw_out) {
    uint8_t* raw = NULL;
    uintptr_t aligned = 0U;

    if(size == 0U || raw_out == NULL || alignment == 0U || (alignment & (alignment - 1U)) != 0U) {
        return NULL;
    }

    raw = malloc(size + alignment - 1U);
    if(raw == NULL) {
        return NULL;
    }

    aligned = ((uintptr_t)raw + alignment - 1U) & ~((uintptr_t)alignment - 1U);
    *raw_out = raw;
    memset((void*)aligned, 0, size);
    return (void*)aligned;
}

static bool kdbx_open_outer_emit(
    KDBXOpenStreamCallback callback,
    void* context,
    size_t* emitted_bytes,
    char* error,
    size_t error_size,
    const uint8_t* data,
    size_t data_size) {
    furi_assert(callback);
    furi_assert(emitted_bytes);

    if(data_size == 0U) {
        return true;
    }
    if(*emitted_bytes > (KDBX_OPEN_MAX_STREAM_OUTPUT_SIZE - data_size)) {
        kdbx_open_outer_set_error(
            error, error_size, "This database exceeds FlipPass's streaming output limit.");
        return false;
    }
    if(!callback(data, data_size, context)) {
        if(error != NULL && error[0] == '\0') {
            kdbx_open_outer_set_error(
                error, error_size, "The payload consumer rejected the database stream.");
        }
        return false;
    }

    *emitted_bytes += data_size;
    return true;
}

static void kdbx_open_outer_compute_block_key(
    const uint8_t* hmac_key,
    size_t hmac_key_size,
    uint64_t block_index,
    uint8_t block_key[64]) {
    uint8_t block_index_bytes[8];
    uint8_t block_key_input[72];

    for(size_t i = 0U; i < sizeof(block_index_bytes); ++i) {
        block_index_bytes[i] = (uint8_t)((block_index >> (i * 8U)) & 0xFFU);
    }

    memcpy(block_key_input, block_index_bytes, sizeof(block_index_bytes));
    memcpy(block_key_input + sizeof(block_index_bytes), hmac_key, hmac_key_size);
    sha512_Raw(block_key_input, sizeof(block_key_input), block_key);
    memzero(block_key_input, sizeof(block_key_input));
}

static bool kdbx_open_outer_verify_block_hmac(
    Stream* stream,
    const uint8_t* hmac_key,
    size_t hmac_key_size,
    uint64_t block_index,
    uint32_t block_size,
    const uint8_t expected_hmac[32],
    uint8_t* io_buffer,
    size_t io_buffer_size,
    char* error,
    size_t error_size) {
    uint8_t block_key[64];
    uint8_t block_hmac[32];
    uint8_t block_index_bytes[8];
    size_t block_remaining = block_size;
    HMAC_SHA256_CTX hmac_ctx;

    furi_assert(stream);
    furi_assert(hmac_key);
    furi_assert(expected_hmac);
    furi_assert(io_buffer);

    for(size_t i = 0U; i < sizeof(block_index_bytes); ++i) {
        block_index_bytes[i] = (uint8_t)((block_index >> (i * 8U)) & 0xFFU);
    }

    kdbx_open_outer_compute_block_key(hmac_key, hmac_key_size, block_index, block_key);
    hmac_sha256_Init(&hmac_ctx, block_key, sizeof(block_key));
    hmac_sha256_Update(&hmac_ctx, block_index_bytes, sizeof(block_index_bytes));
    hmac_sha256_Update(&hmac_ctx, (const uint8_t*)&block_size, sizeof(block_size));
    while(block_remaining > 0U) {
        const size_t chunk_size = (block_remaining > io_buffer_size) ? io_buffer_size :
                                                                       block_remaining;
        if(stream_read(stream, io_buffer, chunk_size) != chunk_size) {
            kdbx_open_outer_set_error(
                error, error_size, "Unable to read the encrypted database payload.");
            memzero(block_key, sizeof(block_key));
            memzero(block_hmac, sizeof(block_hmac));
            memzero(block_index_bytes, sizeof(block_index_bytes));
            return false;
        }
        hmac_sha256_Update(&hmac_ctx, io_buffer, chunk_size);
        block_remaining -= chunk_size;
    }
    hmac_sha256_Final(&hmac_ctx, block_hmac);
    if(memcmp(block_hmac, expected_hmac, sizeof(block_hmac)) != 0) {
        kdbx_open_outer_set_error(
            error, error_size, "Wrong password or damaged database payload.");
        memzero(block_key, sizeof(block_key));
        memzero(block_hmac, sizeof(block_hmac));
        memzero(block_index_bytes, sizeof(block_index_bytes));
        return false;
    }

    if(!stream_seek(stream, -(int32_t)block_size, StreamOffsetFromCurrent)) {
        kdbx_open_outer_set_error(
            error, error_size, "Unable to rewind the encrypted database payload.");
        memzero(block_key, sizeof(block_key));
        memzero(block_hmac, sizeof(block_hmac));
        memzero(block_index_bytes, sizeof(block_index_bytes));
        return false;
    }

    memzero(block_key, sizeof(block_key));
    memzero(block_hmac, sizeof(block_hmac));
    memzero(block_index_bytes, sizeof(block_index_bytes));
    return true;
}

static void kdbx_open_outer_aes_state_init(
    KDBXOpenOuterAesState* state,
    const uint8_t* key,
    const uint8_t iv[16]) {
    furi_assert(state);
    furi_assert(key);
    furi_assert(iv);

    memset(state, 0, sizeof(*state));
    aes_decrypt_key256(key, &state->aes_ctx);
    memcpy(state->iv, iv, sizeof(state->iv));
}

static bool kdbx_open_outer_aes_emit_decrypted_block(
    KDBXOpenOuterAesState* state,
    KDBXOpenStreamCallback callback,
    void* context,
    size_t* emitted_bytes,
    char* error,
    size_t error_size,
    const uint8_t ciphertext_block[16]) {
    uint8_t plaintext[16];

    furi_assert(state);
    furi_assert(ciphertext_block);

    aes_cbc_decrypt(ciphertext_block, plaintext, sizeof(plaintext), state->iv, &state->aes_ctx);
    memcpy(state->iv, ciphertext_block, sizeof(state->iv));

    if(state->has_pending_block && !kdbx_open_outer_emit(
                                       callback,
                                       context,
                                       emitted_bytes,
                                       error,
                                       error_size,
                                       state->pending_block,
                                       sizeof(state->pending_block))) {
        memzero(plaintext, sizeof(plaintext));
        return false;
    }

    memcpy(state->pending_block, plaintext, sizeof(state->pending_block));
    state->has_pending_block = true;
    memzero(plaintext, sizeof(plaintext));
    return true;
}

static bool kdbx_open_outer_aes_state_feed(
    KDBXOpenOuterAesState* state,
    KDBXOpenStreamCallback callback,
    void* context,
    size_t* emitted_bytes,
    char* error,
    size_t error_size,
    const uint8_t* ciphertext,
    size_t ciphertext_size) {
    size_t offset = 0U;

    furi_assert(state);
    furi_assert(ciphertext);

    if(state->partial_len > 0U) {
        const size_t needed = sizeof(state->partial_block) - state->partial_len;
        const size_t take = (ciphertext_size < needed) ? ciphertext_size : needed;
        memcpy(state->partial_block + state->partial_len, ciphertext, take);
        state->partial_len += take;
        offset += take;
        if(state->partial_len < sizeof(state->partial_block)) {
            return true;
        }
        if(!kdbx_open_outer_aes_emit_decrypted_block(
               state, callback, context, emitted_bytes, error, error_size, state->partial_block)) {
            return false;
        }
        state->partial_len = 0U;
    }

    while((ciphertext_size - offset) >= sizeof(state->partial_block)) {
        if(!kdbx_open_outer_aes_emit_decrypted_block(
               state, callback, context, emitted_bytes, error, error_size, ciphertext + offset)) {
            return false;
        }
        offset += sizeof(state->partial_block);
    }

    if(offset < ciphertext_size) {
        state->partial_len = ciphertext_size - offset;
        memcpy(state->partial_block, ciphertext + offset, state->partial_len);
    }

    return true;
}

static bool kdbx_open_outer_aes_state_finish(
    KDBXOpenOuterAesState* state,
    KDBXOpenStreamCallback callback,
    void* context,
    size_t* emitted_bytes,
    char* error,
    size_t error_size) {
    uint8_t plaintext[16];

    furi_assert(state);

    if(state->partial_len != 0U || !state->has_pending_block) {
        kdbx_open_outer_set_error(error, error_size, "The AES payload blocks are malformed.");
        return false;
    }

    memcpy(plaintext, state->pending_block, sizeof(plaintext));
    const uint8_t padding = plaintext[15];
    if(padding == 0U || padding > sizeof(plaintext)) {
        memzero(plaintext, sizeof(plaintext));
        kdbx_open_outer_set_error(error, error_size, "The AES payload padding is invalid.");
        return false;
    }
    for(size_t i = sizeof(plaintext) - padding; i < sizeof(plaintext); ++i) {
        if(plaintext[i] != padding) {
            memzero(plaintext, sizeof(plaintext));
            kdbx_open_outer_set_error(error, error_size, "The AES payload padding is invalid.");
            return false;
        }
    }

    const size_t plain_size = sizeof(plaintext) - padding;
    if(!kdbx_open_outer_emit(
           callback, context, emitted_bytes, error, error_size, plaintext, plain_size)) {
        memzero(plaintext, sizeof(plaintext));
        return false;
    }

    memzero(plaintext, sizeof(plaintext));
    memzero(state, sizeof(*state));
    return true;
}

bool kdbx_open_stream_outer_payload(
    const char* file_path,
    const KDBXOpenProfile* profile,
    KDBXOpenStreamCallback callback,
    void* context,
    char* error,
    size_t error_size) {
    Storage* storage = NULL;
    Stream* stream = NULL;
    uint8_t* io_buffer = NULL;
    KDBXOpenOuterAesState* aes_state = NULL;
    void* aes_state_raw = NULL;
    KDBXProtectedStream* chacha_stream = NULL;
    const bool use_aes =
        profile != NULL &&
        memcmp(profile->encryption_algorithm_uuid, KDBX_UUID_AES256, sizeof(KDBX_UUID_AES256)) ==
            0;
    const bool use_chacha20 = profile != NULL && memcmp(
                                                     profile->encryption_algorithm_uuid,
                                                     KDBX_UUID_CHACHA20,
                                                     sizeof(KDBX_UUID_CHACHA20)) == 0;
    size_t emitted_bytes = 0U;
    uint64_t block_index = 0U;
    bool ok = false;

    if(file_path == NULL || file_path[0] == '\0' || profile == NULL || callback == NULL) {
        kdbx_open_outer_set_error(error, error_size, "Open stream input is invalid.");
        return false;
    }

    if(!kdbx_open_profile_validate_for_stream(profile, error, error_size)) {
        return false;
    }
    if(!use_aes && !use_chacha20) {
        kdbx_open_outer_set_error(
            error, error_size, "This database uses an unsupported payload cipher.");
        return false;
    }

    storage = furi_record_open(RECORD_STORAGE);
    stream = file_stream_alloc(storage);
    if(stream == NULL) {
        kdbx_open_outer_set_error(error, error_size, "Unable to allocate the payload stream.");
        goto cleanup;
    }
    if(!file_stream_open(stream, file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        kdbx_open_outer_set_error(error, error_size, "Failed to open the database file.");
        goto cleanup;
    }
    if(!stream_seek(stream, profile->payload_data_offset, StreamOffsetFromStart)) {
        kdbx_open_outer_set_error(error, error_size, "Unable to seek to the encrypted payload.");
        goto cleanup;
    }

    const size_t stream_remaining = stream_size(stream) - stream_tell(stream);
    if(stream_remaining < KDBX_OPEN_HEADER_VERIFICATION_BYTES) {
        kdbx_open_outer_set_error(
            error, error_size, "The KDBX4 header verification bytes are truncated.");
        goto cleanup;
    }
    if(!stream_seek(stream, KDBX_OPEN_HEADER_VERIFICATION_BYTES, StreamOffsetFromCurrent)) {
        kdbx_open_outer_set_error(
            error, error_size, "Unable to skip the KDBX4 header verification bytes.");
        goto cleanup;
    }

    io_buffer = malloc(KDBX_OPEN_STREAM_IO_CHUNK_SIZE);
    if(io_buffer == NULL) {
        kdbx_open_outer_set_error(
            error, error_size, "Not enough RAM is available for the payload stream.");
        goto cleanup;
    }

    if(use_aes) {
        aes_state = kdbx_open_outer_alloc_aligned(sizeof(*aes_state), 16U, &aes_state_raw);
        if(aes_state == NULL) {
            kdbx_open_outer_set_error(
                error, error_size, "Not enough RAM is available for AES payload state.");
            goto cleanup;
        }
        kdbx_open_outer_aes_state_init(aes_state, profile->cipher_key, profile->encryption_iv);
    } else {
        chacha_stream = malloc(sizeof(*chacha_stream));
        if(chacha_stream == NULL) {
            kdbx_open_outer_set_error(
                error, error_size, "Not enough RAM is available for ChaCha20 payload state.");
            goto cleanup;
        }
        memset(chacha_stream, 0, sizeof(*chacha_stream));
        kdbx_protected_stream_reset(chacha_stream);
        if(!kdbx_chacha20_stream_init(
               chacha_stream,
               profile->cipher_key,
               sizeof(profile->cipher_key),
               profile->encryption_iv,
               profile->encryption_iv_size,
               0U)) {
            kdbx_open_outer_set_error(
                error, error_size, "Unable to initialize the ChaCha20 payload stream.");
            goto cleanup;
        }
    }

    while(true) {
        uint8_t hmac[32];
        uint32_t block_size = 0U;

        if(stream_read(stream, hmac, sizeof(hmac)) != sizeof(hmac)) {
            kdbx_open_outer_set_error(
                error, error_size, "Unable to read the encrypted database payload.");
            goto cleanup;
        }
        if(stream_read(stream, (uint8_t*)&block_size, sizeof(block_size)) != sizeof(block_size)) {
            kdbx_open_outer_set_error(
                error, error_size, "Unable to read the encrypted database payload.");
            goto cleanup;
        }
        if(block_size > KDBX_OPEN_MAX_STREAM_OUTPUT_SIZE) {
            kdbx_open_outer_set_error(
                error, error_size, "A database payload block exceeds FlipPass's stream limit.");
            goto cleanup;
        }

        const size_t remaining_after_size = stream_size(stream) - stream_tell(stream);
        if(block_size > remaining_after_size) {
            kdbx_open_outer_set_error(
                error, error_size, "The encrypted database payload is truncated.");
            goto cleanup;
        }
        if(!kdbx_open_outer_verify_block_hmac(
               stream,
               profile->hmac_key,
               sizeof(profile->hmac_key),
               block_index,
               block_size,
               hmac,
               io_buffer,
               KDBX_OPEN_STREAM_IO_CHUNK_SIZE,
               error,
               error_size)) {
            goto cleanup;
        }

        if(block_size == 0U) {
            break;
        }

        size_t block_remaining = block_size;
        while(block_remaining > 0U) {
            const size_t chunk_size = (block_remaining > KDBX_OPEN_STREAM_IO_CHUNK_SIZE) ?
                                          KDBX_OPEN_STREAM_IO_CHUNK_SIZE :
                                          block_remaining;
            if(stream_read(stream, io_buffer, chunk_size) != chunk_size) {
                kdbx_open_outer_set_error(
                    error, error_size, "Unable to read the encrypted database payload.");
                goto cleanup;
            }

            if(use_aes) {
                if(!kdbx_open_outer_aes_state_feed(
                       aes_state,
                       callback,
                       context,
                       &emitted_bytes,
                       error,
                       error_size,
                       io_buffer,
                       chunk_size)) {
                    goto cleanup;
                }
            } else {
                if(!kdbx_protected_stream_apply(chacha_stream, io_buffer, chunk_size)) {
                    kdbx_open_outer_set_error(
                        error, error_size, "Unable to decrypt the database payload.");
                    goto cleanup;
                }
                if(!kdbx_open_outer_emit(
                       callback,
                       context,
                       &emitted_bytes,
                       error,
                       error_size,
                       io_buffer,
                       chunk_size)) {
                    goto cleanup;
                }
            }

            block_remaining -= chunk_size;
        }

        block_index++;
    }

    if(use_aes && aes_state != NULL &&
       !kdbx_open_outer_aes_state_finish(
           aes_state, callback, context, &emitted_bytes, error, error_size)) {
        goto cleanup;
    }

    ok = true;

cleanup:
    if(io_buffer != NULL) {
        memzero(io_buffer, KDBX_OPEN_STREAM_IO_CHUNK_SIZE);
        free(io_buffer);
    }
    if(stream != NULL) {
        file_stream_close(stream);
        stream_free(stream);
    }
    if(storage != NULL) {
        furi_record_close(RECORD_STORAGE);
    }
    if(chacha_stream != NULL) {
        kdbx_protected_stream_reset(chacha_stream);
        memzero(chacha_stream, sizeof(*chacha_stream));
        free(chacha_stream);
    }
    if(aes_state != NULL) {
        memzero(aes_state, sizeof(*aes_state));
        free(aes_state_raw);
    }
    return ok;
}
