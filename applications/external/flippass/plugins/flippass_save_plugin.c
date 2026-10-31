#include "flippass_save_plugin.h"

/* Default flags — must precede conditional includes. */
#ifndef FLIPPASS_SAVE_WRITER_ENABLE_OUTER_CHACHA
#define FLIPPASS_SAVE_WRITER_ENABLE_OUTER_CHACHA 1
#endif

#ifndef FLIPPASS_SAVE_WRITER_ENABLE_GZIP
#define FLIPPASS_SAVE_WRITER_ENABLE_GZIP 1
#endif

#ifndef FLIPPASS_SAVE_WRITER_HOST_PAYLOAD
#define FLIPPASS_SAVE_WRITER_HOST_PAYLOAD 0
#endif

#if !FLIPPASS_SAVE_WRITER_HOST_PAYLOAD
#include "../kdbx/aes.h"
#endif
#include "../kdbx/kdbx_constants.h"
#include "../kdbx/memzero.h"

#include <storage/storage.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLIPPASS_SAVE_BLOCK_SIZE 512U
#if FLIPPASS_SAVE_WRITER_ENABLE_GZIP
#define FLIPPASS_SAVE_GZIP_PENDING_SIZE 256U
#define FLIPPASS_SAVE_GZIP_CRC_INIT     0xFFFFFFFFUL
#endif

typedef struct {
#if !FLIPPASS_SAVE_WRITER_HOST_PAYLOAD
    Storage* storage;
    File* file;
    FuriString* temp_path;
#endif
    FuriString* error_detail;
    const FlipPassSaveRequestV1* request;
    const FlipPassSaveHostApiV1* host_api;
#if !FLIPPASS_SAVE_WRITER_HOST_PAYLOAD
    uint8_t hmac_base[64];
    uint8_t block_buffer[FLIPPASS_SAVE_BLOCK_SIZE];
    size_t block_len;
    uint64_t block_index;
#endif
#if !FLIPPASS_SAVE_WRITER_HOST_PAYLOAD
#if FLIPPASS_SAVE_WRITER_ENABLE_OUTER_CHACHA
    bool use_aes;
    union {
        aes_encrypt_ctx aes_ctx;
        FlipPassCryptoProtectedState chacha_stream;
    } cipher;
#else
    aes_encrypt_ctx aes_ctx;
#endif
    uint8_t aes_iv[16];
    uint8_t aes_pending[16];
    size_t aes_pending_len;
#endif
#if FLIPPASS_SAVE_WRITER_ENABLE_GZIP
    uint8_t* gzip_pending;
    size_t gzip_pending_capacity;
    size_t gzip_pending_len;
    uint32_t gzip_crc32;
    uint32_t gzip_input_size;
#endif
    uint32_t payload_input_size;
    uint32_t progress_next_payload_size;
} FlipPassSavePluginContext;

static void flippass_save_progress(
    FlipPassSavePluginContext* ctx,
    const char* stage,
    const char* detail,
    uint8_t percent) {
    if(ctx != NULL && ctx->host_api != NULL && ctx->host_api->progress != NULL) {
        ctx->host_api->progress(ctx->host_api->context, stage, detail, percent);
    }
}

#if !FLIPPASS_SAVE_WRITER_HOST_PAYLOAD
static aes_encrypt_ctx* flippass_save_aes_context(FlipPassSavePluginContext* ctx) {
#if FLIPPASS_SAVE_WRITER_ENABLE_OUTER_CHACHA
    return &ctx->cipher.aes_ctx;
#else
    return &ctx->aes_ctx;
#endif
}

static bool flippass_save_uses_aes(const FlipPassSavePluginContext* ctx) {
#if FLIPPASS_SAVE_WRITER_ENABLE_OUTER_CHACHA
    return ctx->use_aes;
#else
    UNUSED(ctx);
    return true;
#endif
}
#endif

#if FLIPPASS_ENABLE_LOGS || FLIPPASS_ENABLE_PROBE_LOGS
static void flippass_save_log_heap_raw(const FlipPassSaveHostApiV1* host_api, const char* stage) {
    char message[112];
    if(host_api == NULL || host_api->log == NULL) return;
    snprintf(
        message,
        sizeof(message),
        "SAVE_PLUGIN stage=%s free=%lu max=%lu",
        stage != NULL ? stage : "unknown",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());
    host_api->log(host_api->context, message);
}

static void flippass_save_log_heap(FlipPassSavePluginContext* ctx, const char* stage) {
    if(ctx != NULL) flippass_save_log_heap_raw(ctx->host_api, stage);
}
#else
static void flippass_save_log_heap_raw(const FlipPassSaveHostApiV1* host_api, const char* stage) {
    UNUSED(host_api);
    UNUSED(stage);
}

static void flippass_save_log_heap(FlipPassSavePluginContext* ctx, const char* stage) {
    UNUSED(ctx);
    UNUSED(stage);
}
#endif

static void flippass_save_progress_stream_bytes(
    FlipPassSavePluginContext* ctx,
    const char* stage,
    uint32_t bytes,
    uint8_t start,
    uint8_t end) {
    char detail[40];
    uint8_t percent = start;

    if(ctx == NULL || bytes < ctx->progress_next_payload_size) return;

    ctx->progress_next_payload_size = (bytes > ((uint32_t)-1 - 4096U)) ? (uint32_t)-1 :
                                                                         (bytes + 4096U);
    percent = start + (uint8_t)(bytes / 4096U);
    if(percent > end) percent = end;
    snprintf(detail, sizeof(detail), "%lu KB streamed", (unsigned long)((bytes + 1023U) / 1024U));
    flippass_save_progress(ctx, stage, detail, percent);
}

#if FLIPPASS_SAVE_WRITER_ENABLE_GZIP
static void flippass_save_write_u16_le(uint8_t* out, uint16_t value) {
    out[0] = (uint8_t)(value & 0xFFU);
    out[1] = (uint8_t)((value >> 8) & 0xFFU);
}
#endif

#if !FLIPPASS_SAVE_WRITER_HOST_PAYLOAD || FLIPPASS_SAVE_WRITER_ENABLE_GZIP
static void flippass_save_write_u32_le(uint8_t* out, uint32_t value) {
    out[0] = (uint8_t)(value & 0xFFU);
    out[1] = (uint8_t)((value >> 8) & 0xFFU);
    out[2] = (uint8_t)((value >> 16) & 0xFFU);
    out[3] = (uint8_t)((value >> 24) & 0xFFU);
}
#endif

static void flippass_save_set_error(
    FlipPassSavePluginContext* ctx,
    FuriString* error,
    const char* message) {
    furi_assert(ctx);
    furi_assert(error);
    furi_assert(message);
    furi_string_set_str(error, message);
    furi_string_set_str(ctx->error_detail, message);
}

/* ---- Payload crypto / file I/O ---- */

#if FLIPPASS_SAVE_WRITER_HOST_PAYLOAD
static bool flippass_save_open_payload_target(FlipPassSavePluginContext* ctx, FuriString* error) {
    if(ctx->host_api == NULL || ctx->host_api->begin_payload == NULL) {
        flippass_save_set_error(
            ctx, error, "FlipPass save writer is missing host payload support.");
        return false;
    }
    return ctx->host_api->begin_payload(ctx->host_api->context, ctx->request, error);
}

static bool
    flippass_save_prepare_payload_cipher(FlipPassSavePluginContext* ctx, FuriString* error) {
    UNUSED(ctx);
    UNUSED(error);
    return true;
}

static bool flippass_save_encrypt_plain_bytes(
    FlipPassSavePluginContext* ctx,
    const uint8_t* plain,
    size_t plain_size,
    FuriString* error) {
    if(ctx->host_api == NULL || ctx->host_api->write_payload_plain == NULL) {
        flippass_save_set_error(
            ctx, error, "FlipPass save writer is missing host payload support.");
        return false;
    }
    return ctx->host_api->write_payload_plain(ctx->host_api->context, plain, plain_size, error);
}

static bool flippass_save_finalize_payload(FlipPassSavePluginContext* ctx, FuriString* error) {
    if(ctx->host_api == NULL || ctx->host_api->finish_payload == NULL) {
        flippass_save_set_error(
            ctx, error, "FlipPass save writer is missing host payload support.");
        return false;
    }
    return ctx->host_api->finish_payload(ctx->host_api->context, error);
}
#else
static bool flippass_save_file_write(
    FlipPassSavePluginContext* ctx,
    const void* data,
    size_t data_size,
    FuriString* error) {
    if(data_size == 0U) return true;
    if(storage_file_write(ctx->file, data, data_size) != data_size) {
        flippass_save_set_error(ctx, error, "FlipPass could not write the target KDBX file.");
        return false;
    }
    return true;
}

static bool
    flippass_save_flush_block(FlipPassSavePluginContext* ctx, bool force_empty, FuriString* error) {
    uint8_t block_hmac[32];
    uint8_t size_bytes[4];
    uint8_t index_bytes[8];
    uint8_t input[72];
    uint8_t block_key[64];
    FlipPassCryptoHmacSha256State hmac_ctx;
    const FlipPassCryptoApiV1* crypto = ctx->host_api != NULL ? ctx->host_api->crypto : NULL;

    if(!force_empty && ctx->block_len == 0U) return true;
    if(crypto == NULL || crypto->sha512_chunks == NULL || crypto->hmac_sha256_init == NULL ||
       crypto->hmac_sha256_update == NULL || crypto->hmac_sha256_final == NULL ||
       crypto->hmac_sha256_clear == NULL) {
        flippass_save_set_error(ctx, error, "FlipPass crypto is unavailable for save.");
        return false;
    }

    for(size_t i = 0U; i < sizeof(index_bytes); i++) {
        index_bytes[i] = (uint8_t)((ctx->block_index >> (8U * i)) & 0xFFU);
    }
    flippass_save_write_u32_le(size_bytes, (uint32_t)ctx->block_len);
    memcpy(input, index_bytes, sizeof(index_bytes));
    memcpy(input + sizeof(index_bytes), ctx->hmac_base, sizeof(ctx->hmac_base));
    const FlipPassCryptoChunk block_key_input = {.data = input, .size = sizeof(input)};
    if(!crypto->sha512_chunks(&block_key_input, 1U, block_key) ||
       !crypto->hmac_sha256_init(&hmac_ctx, block_key, sizeof(block_key)) ||
       !crypto->hmac_sha256_update(&hmac_ctx, index_bytes, sizeof(index_bytes)) ||
       !crypto->hmac_sha256_update(&hmac_ctx, size_bytes, sizeof(size_bytes)) ||
       (ctx->block_len > 0U &&
        !crypto->hmac_sha256_update(&hmac_ctx, ctx->block_buffer, ctx->block_len)) ||
       !crypto->hmac_sha256_final(&hmac_ctx, block_hmac)) {
        crypto->hmac_sha256_clear(&hmac_ctx);
        memzero(input, sizeof(input));
        memzero(block_key, sizeof(block_key));
        memzero(block_hmac, sizeof(block_hmac));
        flippass_save_set_error(ctx, error, "FlipPass could not derive a KDBX block HMAC.");
        return false;
    }
    memzero(input, sizeof(input));
    memzero(block_key, sizeof(block_key));

    const bool ok = flippass_save_file_write(ctx, block_hmac, sizeof(block_hmac), error) &&
                    flippass_save_file_write(ctx, size_bytes, sizeof(size_bytes), error) &&
                    flippass_save_file_write(ctx, ctx->block_buffer, ctx->block_len, error);

    memzero(block_hmac, sizeof(block_hmac));
    if(ok) {
        ctx->block_len = 0U;
        ctx->block_index++;
    }
    return ok;
}

static bool flippass_save_emit_ciphertext(
    FlipPassSavePluginContext* ctx,
    const uint8_t* data,
    size_t data_size,
    FuriString* error) {
    size_t offset = 0U;
    while(offset < data_size) {
        const size_t available = sizeof(ctx->block_buffer) - ctx->block_len;
        const size_t chunk = ((data_size - offset) > available) ? available : (data_size - offset);
        memcpy(ctx->block_buffer + ctx->block_len, data + offset, chunk);
        ctx->block_len += chunk;
        offset += chunk;
        if(ctx->block_len == sizeof(ctx->block_buffer) &&
           !flippass_save_flush_block(ctx, false, error)) {
            return false;
        }
    }
    return true;
}

static bool flippass_save_encrypt_plain_bytes(
    FlipPassSavePluginContext* ctx,
    const uint8_t* plain,
    size_t plain_size,
    FuriString* error) {
    if(flippass_save_uses_aes(ctx)) {
        for(size_t index = 0U; index < plain_size; index++) {
            ctx->aes_pending[ctx->aes_pending_len++] = plain[index];
            if(ctx->aes_pending_len == sizeof(ctx->aes_pending)) {
                if(aes_cbc_encrypt(
                       ctx->aes_pending,
                       ctx->aes_pending,
                       sizeof(ctx->aes_pending),
                       ctx->aes_iv,
                       flippass_save_aes_context(ctx)) != EXIT_SUCCESS ||
                   !flippass_save_emit_ciphertext(
                       ctx, ctx->aes_pending, sizeof(ctx->aes_pending), error)) {
                    flippass_save_set_error(
                        ctx, error, "FlipPass could not encrypt the KDBX payload.");
                    return false;
                }
                ctx->aes_pending_len = 0U;
            }
        }
        return true;
    }

#if FLIPPASS_SAVE_WRITER_ENABLE_OUTER_CHACHA
    uint8_t chunk[128];
    size_t offset = 0U;
    while(offset < plain_size) {
        const size_t copy_size = ((plain_size - offset) > sizeof(chunk)) ? sizeof(chunk) :
                                                                           (plain_size - offset);
        memcpy(chunk, plain + offset, copy_size);
        const FlipPassCryptoApiV1* crypto = ctx->host_api != NULL ? ctx->host_api->crypto : NULL;
        if(crypto == NULL || crypto->protected_stream_apply == NULL ||
           !crypto->protected_stream_apply(&ctx->cipher.chacha_stream, chunk, copy_size) ||
           !flippass_save_emit_ciphertext(ctx, chunk, copy_size, error)) {
            memzero(chunk, sizeof(chunk));
            flippass_save_set_error(ctx, error, "FlipPass could not encrypt the KDBX payload.");
            return false;
        }
        offset += copy_size;
    }
    memzero(chunk, sizeof(chunk));
    return true;
#else
    flippass_save_set_error(ctx, error, "This save writer does not support ChaCha20 payloads.");
    return false;
#endif
}

static bool flippass_save_finalize_payload(FlipPassSavePluginContext* ctx, FuriString* error) {
    if(flippass_save_uses_aes(ctx)) {
        const uint8_t pad = (uint8_t)(sizeof(ctx->aes_pending) - ctx->aes_pending_len);
        while(ctx->aes_pending_len < sizeof(ctx->aes_pending)) {
            ctx->aes_pending[ctx->aes_pending_len++] = pad;
        }
        if(aes_cbc_encrypt(
               ctx->aes_pending,
               ctx->aes_pending,
               sizeof(ctx->aes_pending),
               ctx->aes_iv,
               flippass_save_aes_context(ctx)) != EXIT_SUCCESS ||
           !flippass_save_emit_ciphertext(ctx, ctx->aes_pending, sizeof(ctx->aes_pending), error)) {
            flippass_save_set_error(ctx, error, "FlipPass could not finalize AES padding.");
            return false;
        }
        memzero(ctx->aes_pending, sizeof(ctx->aes_pending));
        ctx->aes_pending_len = 0U;
    }
    return flippass_save_flush_block(ctx, false, error) &&
           flippass_save_flush_block(ctx, true, error);
}
#endif

#if FLIPPASS_SAVE_WRITER_ENABLE_GZIP
static uint32_t flippass_save_crc32_update(uint32_t crc, const uint8_t* data, size_t data_size) {
    for(size_t index = 0U; index < data_size; index++) {
        crc ^= data[index];
        for(uint8_t bit = 0U; bit < 8U; bit++) {
            crc = (crc >> 1U) ^ ((crc & 1U) ? 0xEDB88320UL : 0UL);
        }
    }
    return crc;
}

static bool flippass_save_gzip_emit(
    FlipPassSavePluginContext* ctx,
    const uint8_t* data,
    size_t data_size,
    FuriString* error) {
    return flippass_save_encrypt_plain_bytes(ctx, data, data_size, error);
}

static void flippass_save_release_gzip_buffer(FlipPassSavePluginContext* ctx) {
    if(ctx == NULL || ctx->gzip_pending == NULL) return;
    memzero(ctx->gzip_pending, ctx->gzip_pending_capacity);
    free(ctx->gzip_pending);
    ctx->gzip_pending = NULL;
    ctx->gzip_pending_capacity = 0U;
    ctx->gzip_pending_len = 0U;
}

static bool flippass_save_gzip_flush_block(
    FlipPassSavePluginContext* ctx,
    bool final_block,
    FuriString* error) {
    uint8_t header[5];
    const uint16_t len = (uint16_t)ctx->gzip_pending_len;
    const uint16_t nlen = (uint16_t)~len;

    header[0] = final_block ? 0x01U : 0x00U;
    flippass_save_write_u16_le(header + 1U, len);
    flippass_save_write_u16_le(header + 3U, nlen);
    if(!flippass_save_gzip_emit(ctx, header, sizeof(header), error)) return false;
    if(ctx->gzip_pending_len > 0U &&
       !flippass_save_gzip_emit(ctx, ctx->gzip_pending, ctx->gzip_pending_len, error)) {
        return false;
    }
    ctx->gzip_pending_len = 0U;
    return true;
}

static bool flippass_save_gzip_begin(FlipPassSavePluginContext* ctx, FuriString* error) {
    static const uint8_t gzip_header[10] = {
        0x1FU, 0x8BU, 0x08U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0xFFU};

    if(ctx->gzip_pending == NULL) {
        ctx->gzip_pending = malloc(FLIPPASS_SAVE_GZIP_PENDING_SIZE);
        if(ctx->gzip_pending == NULL) {
            flippass_save_set_error(ctx, error, "Not enough RAM is available for GZip save.");
            return false;
        }
        ctx->gzip_pending_capacity = FLIPPASS_SAVE_GZIP_PENDING_SIZE;
    }
    ctx->gzip_pending_len = 0U;
    ctx->gzip_crc32 = FLIPPASS_SAVE_GZIP_CRC_INIT;
    ctx->gzip_input_size = 0U;
    return flippass_save_gzip_emit(ctx, gzip_header, sizeof(gzip_header), error);
}

static bool flippass_save_gzip_payload_sink(const uint8_t* data, size_t data_size, void* context) {
    FlipPassSavePluginContext* ctx = context;
    size_t offset = 0U;

    if(data_size == 0U) return true;

    ctx->gzip_crc32 = flippass_save_crc32_update(ctx->gzip_crc32, data, data_size);
    ctx->gzip_input_size = (data_size > ((uint32_t)-1 - ctx->gzip_input_size)) ?
                               (uint32_t)-1 :
                               (ctx->gzip_input_size + (uint32_t)data_size);
    flippass_save_progress_stream_bytes(ctx, "Compressing XML", ctx->gzip_input_size, 44U, 66U);
    while(offset < data_size) {
        const size_t available = ctx->gzip_pending_capacity - ctx->gzip_pending_len;
        const size_t chunk = ((data_size - offset) > available) ? available : (data_size - offset);
        memcpy(ctx->gzip_pending + ctx->gzip_pending_len, data + offset, chunk);
        ctx->gzip_pending_len += chunk;
        offset += chunk;
        if(ctx->gzip_pending_len == ctx->gzip_pending_capacity &&
           !flippass_save_gzip_flush_block(ctx, false, ctx->error_detail)) {
            return false;
        }
    }
    return true;
}

static bool flippass_save_gzip_finish(FlipPassSavePluginContext* ctx, FuriString* error) {
    uint8_t trailer[8];
    if(!flippass_save_gzip_flush_block(ctx, true, error)) return false;
    flippass_save_write_u32_le(trailer, ctx->gzip_crc32 ^ FLIPPASS_SAVE_GZIP_CRC_INIT);
    flippass_save_write_u32_le(trailer + 4U, ctx->gzip_input_size);
    return flippass_save_gzip_emit(ctx, trailer, sizeof(trailer), error);
}
#else
static void flippass_save_release_gzip_buffer(FlipPassSavePluginContext* ctx) {
    UNUSED(ctx);
}
#endif

static bool flippass_save_payload_sink(const uint8_t* data, size_t data_size, void* context) {
    FlipPassSavePluginContext* ctx = context;
    if(data_size == 0U) return true;
    ctx->payload_input_size = (data_size > ((uint32_t)-1 - ctx->payload_input_size)) ?
                                  (uint32_t)-1 :
                                  (ctx->payload_input_size + (uint32_t)data_size);
    flippass_save_progress_stream_bytes(ctx, "Encrypting XML", ctx->payload_input_size, 44U, 70U);
    return flippass_save_encrypt_plain_bytes(ctx, data, data_size, ctx->error_detail);
}

#if !FLIPPASS_SAVE_WRITER_HOST_PAYLOAD
static bool flippass_save_open_payload_target(FlipPassSavePluginContext* ctx, FuriString* error) {
    uint64_t file_size = 0U;
    furi_string_printf(ctx->temp_path, "%s.tmp", ctx->request->file_path);
    ctx->file = storage_file_alloc(ctx->storage);
    if(ctx->file == NULL ||
       !storage_file_open(
           ctx->file, furi_string_get_cstr(ctx->temp_path), FSAM_WRITE, FSOM_OPEN_EXISTING)) {
        flippass_save_set_error(ctx, error, "FlipPass could not open the temporary KDBX file.");
        return false;
    }
    file_size = storage_file_size(ctx->file);
    if(file_size > (uint64_t)((uint32_t)-1) || !storage_file_seek(ctx->file, file_size, true)) {
        flippass_save_set_error(
            ctx, error, "FlipPass could not append to the temporary KDBX file.");
        return false;
    }
    return true;
}

static bool
    flippass_save_prepare_payload_cipher(FlipPassSavePluginContext* ctx, FuriString* error) {
    const FlipPassSaveRequestV1* request = ctx->request;
    memcpy(ctx->hmac_base, request->hmac_base, sizeof(ctx->hmac_base));
    if(flippass_save_uses_aes(ctx)) {
        if(aes_encrypt_key256(request->cipher_key, flippass_save_aes_context(ctx)) !=
           EXIT_SUCCESS) {
            flippass_save_set_error(ctx, error, "FlipPass could not initialize AES-256 for save.");
            return false;
        }
        memcpy(ctx->aes_iv, request->iv, sizeof(ctx->aes_iv));
    }
#if FLIPPASS_SAVE_WRITER_ENABLE_OUTER_CHACHA
    else if(
        ctx->host_api == NULL || ctx->host_api->crypto == NULL ||
        ctx->host_api->crypto->chacha20_stream_init == NULL ||
        !ctx->host_api->crypto->chacha20_stream_init(
            &ctx->cipher.chacha_stream,
            request->cipher_key,
            request->cipher_key_size,
            request->iv,
            request->iv_size,
            0U)) {
        flippass_save_set_error(ctx, error, "FlipPass could not initialize ChaCha20 for save.");
        return false;
    }
#endif
    return true;
}
#endif

#if FLIPPASS_SAVE_WRITER_HOST_PAYLOAD
static bool flippass_save_commit_target(FlipPassSavePluginContext* ctx, FuriString* error) {
    UNUSED(ctx);
    UNUSED(error);
    return true;
}
#else
static bool flippass_save_commit_target(FlipPassSavePluginContext* ctx, FuriString* error) {
    storage_file_sync(ctx->file);
    storage_file_close(ctx->file);
    storage_file_free(ctx->file);
    ctx->file = NULL;

    storage_simply_remove(ctx->storage, ctx->request->file_path);
    if(storage_common_rename(
           ctx->storage, furi_string_get_cstr(ctx->temp_path), ctx->request->file_path) !=
       FSE_OK) {
        flippass_save_set_error(ctx, error, "FlipPass could not replace the target KDBX file.");
        storage_simply_remove(ctx->storage, furi_string_get_cstr(ctx->temp_path));
        return false;
    }
    return true;
}
#endif

static bool flippass_save_run_internal(FlipPassSavePluginContext* ctx, FuriString* error) {
    bool ok = false;

    flippass_save_log_heap(ctx, "run_internal_enter");
    flippass_save_progress(ctx, "Preparing Payload", "Opening temp file", 38U);

    if(!flippass_save_open_payload_target(ctx, error)) goto cleanup;
    flippass_save_log_heap(ctx, "payload_target_open_ok");

    flippass_save_progress(ctx, "Preparing Payload", "Initializing cipher", 39U);

    if(!flippass_save_prepare_payload_cipher(ctx, error)) goto cleanup;
    flippass_save_log_heap(ctx, "payload_cipher_ok");

    flippass_save_progress(ctx, "Writing XML", "Streaming from serial vault", 40U);
    ctx->payload_input_size = 0U;
    ctx->progress_next_payload_size = 0U;

#if FLIPPASS_SAVE_WRITER_ENABLE_GZIP
    if(ctx->request->compression == KDBX_COMPRESSION_GZIP) {
        flippass_save_progress(ctx, "Preparing GZip", "Stored blocks; no dictionary", 42U);
        ctx->progress_next_payload_size = 0U;
        if(!flippass_save_gzip_begin(ctx, error) ||
           !ctx->host_api->stream_xml(
               ctx->host_api->context, flippass_save_gzip_payload_sink, ctx, error) ||
           !flippass_save_gzip_finish(ctx, error)) {
            flippass_save_release_gzip_buffer(ctx);
            if(furi_string_empty(error)) {
                flippass_save_set_error(ctx, error, "FlipPass could not GZip the KDBX payload.");
            }
            goto cleanup;
        }
        flippass_save_release_gzip_buffer(ctx);
        flippass_save_progress(ctx, "Compressing XML", "GZip trailer", 68U);
        flippass_save_log_heap(ctx, "gzip_payload_ok");
    } else
#endif
        if(!ctx->host_api->stream_xml(
               ctx->host_api->context, flippass_save_payload_sink, ctx, error)) {
        if(furi_string_empty(error)) {
            flippass_save_set_error(ctx, error, "FlipPass could not stream the KDBX payload.");
        }
        goto cleanup;
    }
    flippass_save_log_heap(ctx, "payload_ok");

    flippass_save_progress(ctx, "Encrypting Payload", "Final blocks", 78U);
    if(!flippass_save_finalize_payload(ctx, error)) goto cleanup;
    flippass_save_log_heap(ctx, "final_blocks_ok");

    flippass_save_progress(ctx, "Saving File", "Replacing target", 92U);
    if(!flippass_save_commit_target(ctx, error)) goto cleanup;
    flippass_save_log_heap(ctx, "commit_ok");

    flippass_save_progress(ctx, "Done", "Saved", 100U);
    ok = true;

cleanup:
#if !FLIPPASS_SAVE_WRITER_HOST_PAYLOAD
    if(!ok) {
        if(ctx->file != NULL) {
            storage_file_close(ctx->file);
            storage_file_free(ctx->file);
            ctx->file = NULL;
        }
        if(ctx->temp_path != NULL && !furi_string_empty(ctx->temp_path)) {
            storage_simply_remove(ctx->storage, furi_string_get_cstr(ctx->temp_path));
        }
    }
    memzero(ctx->hmac_base, sizeof(ctx->hmac_base));
    memzero(ctx->block_buffer, sizeof(ctx->block_buffer));
    memzero(ctx->aes_iv, sizeof(ctx->aes_iv));
    memzero(ctx->aes_pending, sizeof(ctx->aes_pending));
#endif
    flippass_save_release_gzip_buffer(ctx);
#if !FLIPPASS_SAVE_WRITER_HOST_PAYLOAD
#if FLIPPASS_SAVE_WRITER_ENABLE_OUTER_CHACHA
    if(!ctx->use_aes) {
        if(ctx->host_api != NULL && ctx->host_api->crypto != NULL &&
           ctx->host_api->crypto->protected_stream_clear != NULL) {
            ctx->host_api->crypto->protected_stream_clear(&ctx->cipher.chacha_stream);
        }
    }
    memzero(&ctx->cipher, sizeof(ctx->cipher));
#else
    memzero(&ctx->aes_ctx, sizeof(ctx->aes_ctx));
#endif
#endif
    return ok;
}

static bool flippass_save_plugin_run(
    const FlipPassSaveRequestV1* request,
    const FlipPassSaveHostApiV1* host_api,
    FuriString* error) {
    FlipPassSavePluginContext* ctx = NULL;
    bool ok = false;

    if(request == NULL || host_api == NULL || error == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass received an invalid save-plugin request.");
        }
        return false;
    }

#if !FLIPPASS_SAVE_WRITER_HOST_PAYLOAD && FLIPPASS_SAVE_WRITER_ENABLE_OUTER_CHACHA
    const bool cipher_supported = request->cipher == FlipPassSaveCipherAes256 ||
                                  request->cipher == FlipPassSaveCipherChaCha20;
    const size_t expected_iv_size = (request->cipher == FlipPassSaveCipherChaCha20) ? 12U : 16U;
#else
    const bool cipher_supported = request->cipher == FlipPassSaveCipherAes256;
    const size_t expected_iv_size = 16U;
#endif
#if FLIPPASS_SAVE_WRITER_ENABLE_GZIP
    const bool compression_supported = request->compression == KDBX_COMPRESSION_NONE ||
                                       request->compression == KDBX_COMPRESSION_GZIP;
#else
    const bool compression_supported = request->compression == KDBX_COMPRESSION_NONE;
#endif

    if(request->api_version != FLIPPASS_SAVE_PLUGIN_API_VERSION ||
       host_api->api_version != FLIPPASS_SAVE_HOST_API_VERSION || request->file_path == NULL ||
       request->cipher_key == NULL || request->cipher_key_size != 32U ||
       request->hmac_base == NULL || request->hmac_base_size != 64U || request->iv == NULL ||
       request->iv_size != expected_iv_size || host_api->stream_xml == NULL ||
#if !FLIPPASS_SAVE_WRITER_HOST_PAYLOAD
       host_api->crypto == NULL || host_api->crypto->api_version != FLIPPASS_CRYPTO_API_VERSION ||
       host_api->crypto->sha512_chunks == NULL || host_api->crypto->hmac_sha256_init == NULL ||
       host_api->crypto->hmac_sha256_update == NULL ||
       host_api->crypto->hmac_sha256_final == NULL ||
       host_api->crypto->hmac_sha256_clear == NULL ||
#if FLIPPASS_SAVE_WRITER_ENABLE_OUTER_CHACHA
       host_api->crypto->chacha20_stream_init == NULL ||
       host_api->crypto->protected_stream_apply == NULL ||
       host_api->crypto->protected_stream_clear == NULL ||
#endif
#endif
#if FLIPPASS_SAVE_WRITER_HOST_PAYLOAD
       host_api->begin_payload == NULL || host_api->write_payload_plain == NULL ||
       host_api->finish_payload == NULL ||
#endif
       !cipher_supported || !compression_supported) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass received an invalid save-plugin request.");
        }
        return false;
    }

    flippass_save_log_heap_raw(host_api, "run_enter");
    ctx = malloc(sizeof(FlipPassSavePluginContext));
    if(ctx == NULL) {
        furi_string_set_str(error, "Not enough RAM is available for the save writer.");
        return false;
    }
    memset(ctx, 0, sizeof(FlipPassSavePluginContext));

    ctx->error_detail = furi_string_alloc();
    ctx->request = request;
    ctx->host_api = host_api;
#if !FLIPPASS_SAVE_WRITER_HOST_PAYLOAD
    ctx->storage = furi_record_open(RECORD_STORAGE);
    ctx->temp_path = furi_string_alloc();
#if FLIPPASS_SAVE_WRITER_ENABLE_OUTER_CHACHA
    ctx->use_aes = request->cipher != FlipPassSaveCipherChaCha20;
    memzero(&ctx->cipher, sizeof(ctx->cipher));
#else
    memzero(&ctx->aes_ctx, sizeof(ctx->aes_ctx));
#endif
#endif
    flippass_save_log_heap(ctx, "context_ready");

    if(ctx->error_detail == NULL
#if !FLIPPASS_SAVE_WRITER_HOST_PAYLOAD
       || ctx->temp_path == NULL
#endif
    ) {
        furi_string_set_str(error, "Not enough RAM is available for save paths.");
        goto cleanup;
    }

    ok = flippass_save_run_internal(ctx, error);

cleanup:
#if !FLIPPASS_SAVE_WRITER_HOST_PAYLOAD
    if(ctx->file != NULL) {
        storage_file_close(ctx->file);
        storage_file_free(ctx->file);
    }
    if(ctx->temp_path != NULL) {
        furi_string_free(ctx->temp_path);
    }
    if(ctx->storage != NULL) {
        furi_record_close(RECORD_STORAGE);
    }
#endif
    if(ctx->error_detail != NULL) {
        furi_string_free(ctx->error_detail);
    }
    memzero(ctx, sizeof(FlipPassSavePluginContext));
    free(ctx);
    return ok;
}

static const FlipPassSavePluginV1 flippass_save_plugin = {
    .api_version = FLIPPASS_SAVE_PLUGIN_API_VERSION,
    .run = flippass_save_plugin_run,
};

static const FlipperAppPluginDescriptor flippass_save_plugin_descriptor = {
    .appid = FLIPPASS_SAVE_PLUGIN_APP_ID,
    .ep_api_version = FLIPPASS_SAVE_PLUGIN_API_VERSION,
    .entry_point = &flippass_save_plugin,
};

const FlipperAppPluginDescriptor* flippass_save_plugin_ep(void) {
    return &flippass_save_plugin_descriptor;
}
