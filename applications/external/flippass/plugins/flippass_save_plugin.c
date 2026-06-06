#include "flippass_save_plugin.h"

#include "../kdbx/aes.h"
#include "../kdbx/hmac.h"
#include "../kdbx/kdbx_constants.h"
#include "../kdbx/kdbx_protected.h"
#include "../kdbx/memzero.h"
#include "../kdbx/sha2.h"

#include <datetime.h>
#include <storage/storage.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLIPPASS_SAVE_BLOCK_SIZE          512U
#define FLIPPASS_SAVE_GZIP_PENDING_SIZE   256U
#define FLIPPASS_SAVE_GZIP_CRC_INIT       0xFFFFFFFFUL
#define FLIPPASS_KDBX_EPOCH_DELTA_SECONDS 62135596800ULL

typedef struct {
    Storage* storage;
    File* file;
    FuriString* temp_path;
    FuriString* error_detail;
    const FlipPassSaveRequestV1* request;
    const FlipPassSaveHostApiV1* host_api;
    uint8_t hmac_base[64];
    uint8_t block_buffer[FLIPPASS_SAVE_BLOCK_SIZE];
    size_t block_len;
    uint64_t block_index;
    KDBXProtectedStream protected_stream;
    char now_base64[16];
    FuriString* uuid;
    bool use_aes;
    union {
        aes_encrypt_ctx aes_ctx;
        KDBXProtectedStream chacha_stream;
    } cipher;
    uint8_t aes_iv[16];
    uint8_t aes_pending[16];
    size_t aes_pending_len;
    uint8_t* gzip_pending;
    size_t gzip_pending_capacity;
    size_t gzip_pending_len;
    uint32_t gzip_crc32;
    uint32_t gzip_input_size;
    uint32_t payload_input_size;
    uint32_t progress_next_payload_size;
} FlipPassSavePluginContext;

typedef struct {
    FlipPassSavePluginContext* ctx;
    FlipPassSaveChunkCallback callback;
    void* callback_context;
    FuriString* error;
    bool protected_value;
    uint8_t triplet[3];
    size_t triplet_len;
} FlipPassSaveValueStreamContext;

static void flippass_save_progress(
    FlipPassSavePluginContext* ctx,
    const char* stage,
    const char* detail,
    uint8_t percent) {
    if(ctx != NULL && ctx->host_api != NULL && ctx->host_api->progress != NULL) {
        ctx->host_api->progress(ctx->host_api->context, stage, detail, percent);
    }
}

#if FLIPPASS_ENABLE_LOGS
static void flippass_save_log_heap_raw(const FlipPassSaveHostApiV1* host_api, const char* stage) {
    char message[112];

    if(host_api == NULL || host_api->log == NULL) {
        return;
    }

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
    if(ctx == NULL) {
        return;
    }

    flippass_save_log_heap_raw(ctx->host_api, stage);
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

    if(ctx == NULL || bytes < ctx->progress_next_payload_size) {
        return;
    }

    ctx->progress_next_payload_size = (bytes > ((uint32_t)-1 - 4096U)) ? (uint32_t)-1 :
                                                                         (bytes + 4096U);
    percent = start + (uint8_t)(bytes / 4096U);
    if(percent > end) {
        percent = end;
    }
    snprintf(detail, sizeof(detail), "%lu KB streamed", (unsigned long)((bytes + 1023U) / 1024U));
    flippass_save_progress(ctx, stage, detail, percent);
}

static void flippass_save_write_u16_le(uint8_t* out, uint16_t value) {
    out[0] = (uint8_t)(value & 0xFFU);
    out[1] = (uint8_t)((value >> 8) & 0xFFU);
}

static void flippass_save_write_u32_le(uint8_t* out, uint32_t value) {
    out[0] = (uint8_t)(value & 0xFFU);
    out[1] = (uint8_t)((value >> 8) & 0xFFU);
    out[2] = (uint8_t)((value >> 16) & 0xFFU);
    out[3] = (uint8_t)((value >> 24) & 0xFFU);
}

static void flippass_save_write_u64_le(uint8_t* out, uint64_t value) {
    for(size_t index = 0U; index < 8U; index++) {
        out[index] = (uint8_t)((value >> (index * 8U)) & 0xFFU);
    }
}

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

static bool
    flippass_save_base64_encode(const uint8_t* data, size_t data_size, char* out, size_t out_size) {
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t out_index = 0U;

    furi_assert(data);
    furi_assert(out);

    for(size_t index = 0U; index < data_size; index += 3U) {
        const size_t remaining = data_size - index;
        const uint32_t triple = ((uint32_t)data[index] << 16) |
                                ((remaining > 1U ? data[index + 1U] : 0U) << 8) |
                                (remaining > 2U ? data[index + 2U] : 0U);

        if((out_index + 4U) >= out_size) {
            return false;
        }

        out[out_index++] = alphabet[(triple >> 18) & 0x3FU];
        out[out_index++] = alphabet[(triple >> 12) & 0x3FU];
        out[out_index++] = (remaining > 1U) ? alphabet[(triple >> 6) & 0x3FU] : '=';
        out[out_index++] = (remaining > 2U) ? alphabet[triple & 0x3FU] : '=';
    }

    if(out_index >= out_size) {
        return false;
    }

    out[out_index] = '\0';
    return true;
}

static bool flippass_save_payload_emit(
    FlipPassSaveChunkCallback callback,
    void* callback_context,
    const void* data,
    size_t data_size) {
    if(data_size == 0U) {
        return true;
    }

    return callback(data, data_size, callback_context);
}

static bool flippass_save_payload_emit_cstr(
    FlipPassSaveChunkCallback callback,
    void* callback_context,
    const char* text) {
    furi_assert(text);
    return flippass_save_payload_emit(callback, callback_context, text, strlen(text));
}

static bool flippass_save_payload_emit_xml_escaped(
    FlipPassSaveChunkCallback callback,
    void* callback_context,
    const char* text) {
    const char* value = (text != NULL) ? text : "";

    while(*value != '\0') {
        switch(*value) {
        case '&':
            if(!flippass_save_payload_emit_cstr(callback, callback_context, "&amp;")) {
                return false;
            }
            break;
        case '<':
            if(!flippass_save_payload_emit_cstr(callback, callback_context, "&lt;")) {
                return false;
            }
            break;
        case '>':
            if(!flippass_save_payload_emit_cstr(callback, callback_context, "&gt;")) {
                return false;
            }
            break;
        case '"':
            if(!flippass_save_payload_emit_cstr(callback, callback_context, "&quot;")) {
                return false;
            }
            break;
        case '\'':
            if(!flippass_save_payload_emit_cstr(callback, callback_context, "&apos;")) {
                return false;
            }
            break;
        default:
            if(!flippass_save_payload_emit(callback, callback_context, value, 1U)) {
                return false;
            }
            break;
        }
        value++;
    }

    return true;
}

static bool flippass_save_payload_emit_xml_escaped_bytes(
    FlipPassSaveChunkCallback callback,
    void* callback_context,
    const uint8_t* data,
    size_t data_size) {
    for(size_t index = 0U; index < data_size; index++) {
        switch((char)data[index]) {
        case '&':
            if(!flippass_save_payload_emit_cstr(callback, callback_context, "&amp;")) {
                return false;
            }
            break;
        case '<':
            if(!flippass_save_payload_emit_cstr(callback, callback_context, "&lt;")) {
                return false;
            }
            break;
        case '>':
            if(!flippass_save_payload_emit_cstr(callback, callback_context, "&gt;")) {
                return false;
            }
            break;
        case '"':
            if(!flippass_save_payload_emit_cstr(callback, callback_context, "&quot;")) {
                return false;
            }
            break;
        case '\'':
            if(!flippass_save_payload_emit_cstr(callback, callback_context, "&apos;")) {
                return false;
            }
            break;
        default:
            if(!flippass_save_payload_emit(callback, callback_context, &data[index], 1U)) {
                return false;
            }
            break;
        }
    }

    return true;
}

static bool flippass_save_payload_emit_protected_triplet(FlipPassSaveValueStreamContext* stream) {
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char quartet[4];

    furi_assert(stream);
    furi_assert(stream->triplet_len == 3U);

    const uint32_t triple = ((uint32_t)stream->triplet[0] << 16) |
                            ((uint32_t)stream->triplet[1] << 8) | (uint32_t)stream->triplet[2];
    quartet[0] = alphabet[(triple >> 18) & 0x3FU];
    quartet[1] = alphabet[(triple >> 12) & 0x3FU];
    quartet[2] = alphabet[(triple >> 6) & 0x3FU];
    quartet[3] = alphabet[triple & 0x3FU];
    stream->triplet_len = 0U;
    memzero(stream->triplet, sizeof(stream->triplet));
    return flippass_save_payload_emit(
        stream->callback, stream->callback_context, quartet, sizeof(quartet));
}

static bool
    flippass_save_payload_emit_value_chunk(const uint8_t* data, size_t data_size, void* context) {
    FlipPassSaveValueStreamContext* stream = context;

    if(stream == NULL || stream->ctx == NULL || stream->callback == NULL) {
        return false;
    }

    if(data_size == 0U) {
        return true;
    }

    if(!stream->protected_value) {
        const bool ok = flippass_save_payload_emit_xml_escaped_bytes(
            stream->callback, stream->callback_context, data, data_size);
        if(!ok) {
            flippass_save_set_error(
                stream->ctx, stream->error, "FlipPass could not serialize a KDBX field.");
        }
        return ok;
    }

    for(size_t index = 0U; index < data_size; index++) {
        uint8_t byte = data[index];
        if(!kdbx_protected_stream_apply(&stream->ctx->protected_stream, &byte, 1U)) {
            flippass_save_set_error(
                stream->ctx, stream->error, "FlipPass could not protect a KDBX field.");
            return false;
        }

        stream->triplet[stream->triplet_len++] = byte;
        if(stream->triplet_len == 3U && !flippass_save_payload_emit_protected_triplet(stream)) {
            flippass_save_set_error(
                stream->ctx, stream->error, "FlipPass could not serialize a protected field.");
            return false;
        }
    }

    return true;
}

static bool flippass_save_payload_finish_value_stream(FlipPassSaveValueStreamContext* stream) {
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char quartet[4];

    if(stream == NULL || !stream->protected_value || stream->triplet_len == 0U) {
        return true;
    }

    const uint32_t triple = ((uint32_t)stream->triplet[0] << 16) |
                            ((stream->triplet_len > 1U ? stream->triplet[1] : 0U) << 8) |
                            (stream->triplet_len > 2U ? stream->triplet[2] : 0U);
    quartet[0] = alphabet[(triple >> 18) & 0x3FU];
    quartet[1] = alphabet[(triple >> 12) & 0x3FU];
    quartet[2] = (stream->triplet_len > 1U) ? alphabet[(triple >> 6) & 0x3FU] : '=';
    quartet[3] = (stream->triplet_len > 2U) ? alphabet[triple & 0x3FU] : '=';
    stream->triplet_len = 0U;
    memzero(stream->triplet, sizeof(stream->triplet));
    return flippass_save_payload_emit(
        stream->callback, stream->callback_context, quartet, sizeof(quartet));
}

static bool flippass_save_payload_emit_ref_value(
    FlipPassSavePluginContext* ctx,
    FlipPassSaveChunkCallback callback,
    void* callback_context,
    const KDBXFieldRef* ref,
    bool protected_value,
    FuriString* error) {
    FlipPassSaveValueStreamContext stream = {
        .ctx = ctx,
        .callback = callback,
        .callback_context = callback_context,
        .error = error,
        .protected_value = protected_value,
    };

    if(ref == NULL) {
        flippass_save_set_error(ctx, error, "FlipPass could not locate a deferred field.");
        return false;
    }

    if(!ctx->host_api->stream_ref(
           ctx->host_api->context, ref, flippass_save_payload_emit_value_chunk, &stream, error)) {
        if(furi_string_empty(error)) {
            flippass_save_set_error(ctx, error, "FlipPass could not read a deferred field.");
        }
        memzero(&stream, sizeof(stream));
        return false;
    }

    const bool ok = flippass_save_payload_finish_value_stream(&stream);
    if(!ok && furi_string_empty(error)) {
        flippass_save_set_error(ctx, error, "FlipPass could not finish a deferred field.");
    }
    memzero(&stream, sizeof(stream));
    return ok;
}

static bool flippass_save_build_time_base64(char out[16]) {
    DateTime now;
    uint8_t raw[8];
    uint64_t seconds;

    furi_hal_rtc_get_datetime(&now);
    seconds = (uint64_t)datetime_datetime_to_timestamp(&now) + FLIPPASS_KDBX_EPOCH_DELTA_SECONDS;

    for(size_t index = 0U; index < sizeof(raw); index++) {
        raw[index] = (uint8_t)((seconds >> (index * 8U)) & 0xFFU);
    }

    return flippass_save_base64_encode(raw, sizeof(raw), out, 16U);
}

static bool flippass_save_payload_emit_times(
    FlipPassSavePluginContext* ctx,
    FlipPassSaveChunkCallback callback,
    void* callback_context) {
    return flippass_save_payload_emit_cstr(
               callback, callback_context, "<Times><LastModificationTime>") &&
           flippass_save_payload_emit_cstr(callback, callback_context, ctx->now_base64) &&
           flippass_save_payload_emit_cstr(
               callback, callback_context, "</LastModificationTime><CreationTime>") &&
           flippass_save_payload_emit_cstr(callback, callback_context, ctx->now_base64) &&
           flippass_save_payload_emit_cstr(
               callback, callback_context, "</CreationTime><LastAccessTime>") &&
           flippass_save_payload_emit_cstr(callback, callback_context, ctx->now_base64) &&
           flippass_save_payload_emit_cstr(
               callback, callback_context, "</LastAccessTime><ExpiryTime>") &&
           flippass_save_payload_emit_cstr(callback, callback_context, ctx->now_base64) &&
           flippass_save_payload_emit_cstr(
               callback,
               callback_context,
               "</ExpiryTime><Expires>False</Expires><UsageCount>0</UsageCount><LocationChanged>") &&
           flippass_save_payload_emit_cstr(callback, callback_context, ctx->now_base64) &&
           flippass_save_payload_emit_cstr(
               callback, callback_context, "</LocationChanged></Times>");
}

static bool flippass_save_payload_emit_meta(
    FlipPassSavePluginContext* ctx,
    FlipPassSaveChunkCallback callback,
    void* callback_context,
    const char* database_name) {
    return flippass_save_payload_emit_cstr(
               callback,
               callback_context,
               "<KeePassFile><Meta><Generator>FlipPass</Generator><DatabaseName>") &&
           flippass_save_payload_emit_xml_escaped(callback, callback_context, database_name) &&
           flippass_save_payload_emit_cstr(
               callback, callback_context, "</DatabaseName><DatabaseNameChanged>") &&
           flippass_save_payload_emit_cstr(callback, callback_context, ctx->now_base64) &&
           flippass_save_payload_emit_cstr(
               callback,
               callback_context,
               "</DatabaseNameChanged><DatabaseDescription/><DatabaseDescriptionChanged>") &&
           flippass_save_payload_emit_cstr(callback, callback_context, ctx->now_base64) &&
           flippass_save_payload_emit_cstr(
               callback,
               callback_context,
               "</DatabaseDescriptionChanged><DefaultUserName/><DefaultUserNameChanged>") &&
           flippass_save_payload_emit_cstr(callback, callback_context, ctx->now_base64) &&
           flippass_save_payload_emit_cstr(
               callback,
               callback_context,
               "</DefaultUserNameChanged><MaintenanceHistoryDays>365</MaintenanceHistoryDays><Color/><MasterKeyChanged>") &&
           flippass_save_payload_emit_cstr(callback, callback_context, ctx->now_base64) &&
           flippass_save_payload_emit_cstr(
               callback,
               callback_context,
               "</MasterKeyChanged><MasterKeyChangeRec>-1</MasterKeyChangeRec><MasterKeyChangeForce>-1</MasterKeyChangeForce><MemoryProtection><ProtectTitle>False</ProtectTitle><ProtectUserName>False</ProtectUserName><ProtectPassword>True</ProtectPassword><ProtectURL>False</ProtectURL><ProtectNotes>False</ProtectNotes></MemoryProtection><CustomIcons/><RecycleBinEnabled>False</RecycleBinEnabled><RecycleBinUUID>AAAAAAAAAAAAAAAAAAAAAA==</RecycleBinUUID><RecycleBinChanged>") &&
           flippass_save_payload_emit_cstr(callback, callback_context, ctx->now_base64) &&
           flippass_save_payload_emit_cstr(
               callback,
               callback_context,
               "</RecycleBinChanged><EntryTemplatesGroup>AAAAAAAAAAAAAAAAAAAAAA==</EntryTemplatesGroup><EntryTemplatesGroupChanged>") &&
           flippass_save_payload_emit_cstr(callback, callback_context, ctx->now_base64) &&
           flippass_save_payload_emit_cstr(
               callback,
               callback_context,
               "</EntryTemplatesGroupChanged><LastSelectedGroup>AAAAAAAAAAAAAAAAAAAAAA==</LastSelectedGroup><LastTopVisibleGroup>AAAAAAAAAAAAAAAAAAAAAA==</LastTopVisibleGroup><HistoryMaxItems>10</HistoryMaxItems><HistoryMaxSize>6291456</HistoryMaxSize><SettingsChanged>") &&
           flippass_save_payload_emit_cstr(callback, callback_context, ctx->now_base64) &&
           flippass_save_payload_emit_cstr(
               callback, callback_context, "</SettingsChanged></Meta><Root>");
}

static bool flippass_save_payload_emit_entry_string(
    FlipPassSavePluginContext* ctx,
    FlipPassSaveChunkCallback callback,
    void* callback_context,
    const char* key,
    const char* value,
    bool protected_value) {
    if(!flippass_save_payload_emit_cstr(callback, callback_context, "<String><Key>") ||
       !flippass_save_payload_emit_xml_escaped(callback, callback_context, key) ||
       !flippass_save_payload_emit_cstr(
           callback,
           callback_context,
           protected_value ? "</Key><Value Protected=\"True\">" : "</Key><Value>")) {
        return false;
    }

    if(protected_value) {
        FlipPassSaveValueStreamContext stream = {
            .ctx = ctx,
            .callback = callback,
            .callback_context = callback_context,
            .error = ctx->error_detail,
            .protected_value = true,
        };
        const uint8_t* bytes = (const uint8_t*)(value != NULL ? value : "");
        const size_t bytes_size = value != NULL ? strlen(value) : 0U;
        const bool ok = flippass_save_payload_emit_value_chunk(bytes, bytes_size, &stream) &&
                        flippass_save_payload_finish_value_stream(&stream);
        memzero(&stream, sizeof(stream));
        if(!ok) {
            return false;
        }
    } else if(!flippass_save_payload_emit_xml_escaped(callback, callback_context, value)) {
        return false;
    }

    return flippass_save_payload_emit_cstr(callback, callback_context, "</Value></String>");
}

static bool flippass_save_payload_emit_entry_ref_string(
    FlipPassSavePluginContext* ctx,
    FlipPassSaveChunkCallback callback,
    void* callback_context,
    const char* key,
    const KDBXFieldRef* ref,
    bool protected_value,
    FuriString* error) {
    if(!flippass_save_payload_emit_cstr(callback, callback_context, "<String><Key>") ||
       !flippass_save_payload_emit_xml_escaped(callback, callback_context, key) ||
       !flippass_save_payload_emit_cstr(
           callback,
           callback_context,
           protected_value ? "</Key><Value Protected=\"True\">" : "</Key><Value>")) {
        return false;
    }

    if(!flippass_save_payload_emit_ref_value(
           ctx, callback, callback_context, ref, protected_value, error)) {
        return false;
    }

    return flippass_save_payload_emit_cstr(callback, callback_context, "</Value></String>");
}

static bool flippass_save_file_write(
    FlipPassSavePluginContext* ctx,
    const void* data,
    size_t data_size,
    FuriString* error) {
    if(data_size == 0U) {
        return true;
    }

    if(storage_file_write(ctx->file, data, data_size) != data_size) {
        flippass_save_set_error(ctx, error, "FlipPass could not write the target KDBX file.");
        return false;
    }

    return true;
}

static void flippass_save_compute_block_key(
    const uint8_t* hmac_base,
    uint64_t block_index,
    uint8_t out_key[64]) {
    uint8_t input[72];

    flippass_save_write_u64_le(input, block_index);
    memcpy(input + 8U, hmac_base, 64U);
    sha512_Raw(input, sizeof(input), out_key);
    memzero(input, sizeof(input));
}

static bool
    flippass_save_flush_block(FlipPassSavePluginContext* ctx, bool force_empty, FuriString* error) {
    uint8_t block_key[64];
    uint8_t block_hmac[32];
    uint8_t index_bytes[8];
    uint8_t size_bytes[4];
    HMAC_SHA256_CTX hmac_ctx;

    if(!force_empty && ctx->block_len == 0U) {
        return true;
    }

    flippass_save_compute_block_key(ctx->hmac_base, ctx->block_index, block_key);
    flippass_save_write_u64_le(index_bytes, ctx->block_index);
    flippass_save_write_u32_le(size_bytes, (uint32_t)ctx->block_len);
    hmac_sha256_Init(&hmac_ctx, block_key, sizeof(block_key));
    hmac_sha256_Update(&hmac_ctx, index_bytes, sizeof(index_bytes));
    hmac_sha256_Update(&hmac_ctx, size_bytes, sizeof(size_bytes));
    if(ctx->block_len > 0U) {
        hmac_sha256_Update(&hmac_ctx, ctx->block_buffer, (uint32_t)ctx->block_len);
    }
    hmac_sha256_Final(&hmac_ctx, block_hmac);

    const bool ok = flippass_save_file_write(ctx, block_hmac, sizeof(block_hmac), error) &&
                    flippass_save_file_write(ctx, size_bytes, sizeof(size_bytes), error) &&
                    flippass_save_file_write(ctx, ctx->block_buffer, ctx->block_len, error);

    memzero(block_key, sizeof(block_key));
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
    if(ctx->use_aes) {
        for(size_t index = 0U; index < plain_size; index++) {
            ctx->aes_pending[ctx->aes_pending_len++] = plain[index];
            if(ctx->aes_pending_len == sizeof(ctx->aes_pending)) {
                if(aes_cbc_encrypt(
                       ctx->aes_pending,
                       ctx->aes_pending,
                       sizeof(ctx->aes_pending),
                       ctx->aes_iv,
                       &ctx->cipher.aes_ctx) != EXIT_SUCCESS ||
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

    uint8_t chunk[128];
    size_t offset = 0U;
    while(offset < plain_size) {
        const size_t copy_size = ((plain_size - offset) > sizeof(chunk)) ? sizeof(chunk) :
                                                                           (plain_size - offset);
        memcpy(chunk, plain + offset, copy_size);
        if(!kdbx_protected_stream_apply(&ctx->cipher.chacha_stream, chunk, copy_size) ||
           !flippass_save_emit_ciphertext(ctx, chunk, copy_size, error)) {
            memzero(chunk, sizeof(chunk));
            flippass_save_set_error(ctx, error, "FlipPass could not encrypt the KDBX payload.");
            return false;
        }
        offset += copy_size;
    }

    memzero(chunk, sizeof(chunk));
    return true;
}

static bool flippass_save_finalize_payload(FlipPassSavePluginContext* ctx, FuriString* error) {
    if(ctx->use_aes) {
        const uint8_t pad = (uint8_t)(sizeof(ctx->aes_pending) - ctx->aes_pending_len);
        while(ctx->aes_pending_len < sizeof(ctx->aes_pending)) {
            ctx->aes_pending[ctx->aes_pending_len++] = pad;
        }

        if(aes_cbc_encrypt(
               ctx->aes_pending,
               ctx->aes_pending,
               sizeof(ctx->aes_pending),
               ctx->aes_iv,
               &ctx->cipher.aes_ctx) != EXIT_SUCCESS ||
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
    if(ctx == NULL || ctx->gzip_pending == NULL) {
        return;
    }

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
    if(!flippass_save_gzip_emit(ctx, header, sizeof(header), error)) {
        return false;
    }
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

    if(data_size == 0U) {
        return true;
    }

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

    if(!flippass_save_gzip_flush_block(ctx, true, error)) {
        return false;
    }

    flippass_save_write_u32_le(trailer, ctx->gzip_crc32 ^ FLIPPASS_SAVE_GZIP_CRC_INIT);
    flippass_save_write_u32_le(trailer + 4U, ctx->gzip_input_size);
    return flippass_save_gzip_emit(ctx, trailer, sizeof(trailer), error);
}

static bool flippass_save_payload_sink(const uint8_t* data, size_t data_size, void* context) {
    FlipPassSavePluginContext* ctx = context;

    if(data_size == 0U) {
        return true;
    }

    ctx->payload_input_size = (data_size > ((uint32_t)-1 - ctx->payload_input_size)) ?
                                  (uint32_t)-1 :
                                  (ctx->payload_input_size + (uint32_t)data_size);
    flippass_save_progress_stream_bytes(ctx, "Encrypting XML", ctx->payload_input_size, 44U, 70U);
    return flippass_save_encrypt_plain_bytes(ctx, data, data_size, ctx->error_detail);
}

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
    if(ctx->use_aes) {
        if(aes_encrypt_key256(request->cipher_key, &ctx->cipher.aes_ctx) != EXIT_SUCCESS) {
            flippass_save_set_error(ctx, error, "FlipPass could not initialize AES-256 for save.");
            return false;
        }
        memcpy(ctx->aes_iv, request->iv, sizeof(ctx->aes_iv));
    } else if(!kdbx_chacha20_stream_init(
                  &ctx->cipher.chacha_stream,
                  request->cipher_key,
                  request->cipher_key_size,
                  request->iv,
                  request->iv_size,
                  0U)) {
        flippass_save_set_error(ctx, error, "FlipPass could not initialize ChaCha20 for save.");
        return false;
    }

    return true;
}

static const KDBXFieldRef*
    flippass_save_entry_field_ref(const KDBXEntry* entry, uint32_t field_mask) {
    if(entry == NULL) {
        return NULL;
    }

    switch(field_mask) {
    case KDBXEntryFieldUsername:
        return &entry->username_ref;
    case KDBXEntryFieldPassword:
        return &entry->password_ref;
    case KDBXEntryFieldUrl:
        return &entry->url_ref;
    case KDBXEntryFieldNotes:
        return &entry->notes_ref;
    case KDBXEntryFieldAutotype:
        return &entry->autotype_ref;
    default:
        return NULL;
    }
}

static bool flippass_save_payload_emit_entry(
    FlipPassSavePluginContext* ctx,
    FlipPassSaveChunkCallback callback,
    void* callback_context,
    KDBXEntry* entry,
    FuriString* error) {
    if(!ctx->host_api->copy_entry_uuid(ctx->host_api->context, entry, ctx->uuid, error)) {
        return false;
    }

    if(!flippass_save_payload_emit_cstr(callback, callback_context, "<Entry><UUID>") ||
       !flippass_save_payload_emit_xml_escaped(
           callback, callback_context, furi_string_get_cstr(ctx->uuid)) ||
       !flippass_save_payload_emit_cstr(callback, callback_context, "</UUID>") ||
       !flippass_save_payload_emit_times(ctx, callback, callback_context) ||
       !flippass_save_payload_emit_entry_string(
           ctx, callback, callback_context, "Title", entry->title, false)) {
        return false;
    }

    if(ctx->host_api->entry_has_field(ctx->host_api->context, entry, KDBXEntryFieldUsername) &&
       !flippass_save_payload_emit_entry_ref_string(
           ctx,
           callback,
           callback_context,
           "UserName",
           flippass_save_entry_field_ref(entry, KDBXEntryFieldUsername),
           false,
           error)) {
        return false;
    }

    if(ctx->host_api->entry_has_field(ctx->host_api->context, entry, KDBXEntryFieldPassword) &&
       !flippass_save_payload_emit_entry_ref_string(
           ctx,
           callback,
           callback_context,
           "Password",
           flippass_save_entry_field_ref(entry, KDBXEntryFieldPassword),
           true,
           error)) {
        return false;
    }

    if(ctx->host_api->entry_has_field(ctx->host_api->context, entry, KDBXEntryFieldUrl) &&
       !flippass_save_payload_emit_entry_ref_string(
           ctx,
           callback,
           callback_context,
           "URL",
           flippass_save_entry_field_ref(entry, KDBXEntryFieldUrl),
           false,
           error)) {
        return false;
    }

    if(ctx->host_api->entry_has_field(ctx->host_api->context, entry, KDBXEntryFieldNotes) &&
       !flippass_save_payload_emit_entry_ref_string(
           ctx,
           callback,
           callback_context,
           "Notes",
           flippass_save_entry_field_ref(entry, KDBXEntryFieldNotes),
           false,
           error)) {
        return false;
    }

    for(KDBXCustomField* field = entry->custom_fields; field != NULL; field = field->next) {
        if(!flippass_save_payload_emit_entry_ref_string(
               ctx,
               callback,
               callback_context,
               field->key,
               &field->value_ref,
               field->protected_value,
               error)) {
            return false;
        }
    }

    if(ctx->host_api->entry_has_field(ctx->host_api->context, entry, KDBXEntryFieldAutotype)) {
        if(!flippass_save_payload_emit_cstr(
               callback,
               callback_context,
               "<AutoType><Enabled>True</Enabled><DataTransferObfuscation>0</DataTransferObfuscation><DefaultSequence>") ||
           !flippass_save_payload_emit_ref_value(
               ctx,
               callback,
               callback_context,
               flippass_save_entry_field_ref(entry, KDBXEntryFieldAutotype),
               false,
               error) ||
           !flippass_save_payload_emit_cstr(
               callback,
               callback_context,
               "</DefaultSequence><Association><Window/><KeystrokeSequence/></Association></AutoType>")) {
            return false;
        }
    }

    return flippass_save_payload_emit_cstr(callback, callback_context, "</Entry>");
}

static bool flippass_save_payload_emit_group(
    FlipPassSavePluginContext* ctx,
    FlipPassSaveChunkCallback callback,
    void* callback_context,
    KDBXGroup* group,
    bool root_group,
    FuriString* error) {
    if(!ctx->host_api->copy_group_uuid(ctx->host_api->context, group, ctx->uuid, error) ||
       !flippass_save_payload_emit_cstr(callback, callback_context, "<Group><UUID>") ||
       !flippass_save_payload_emit_xml_escaped(
           callback, callback_context, furi_string_get_cstr(ctx->uuid)) ||
       !flippass_save_payload_emit_cstr(callback, callback_context, "</UUID>")) {
        return false;
    }

    if(root_group) {
        if(!flippass_save_payload_emit_cstr(
               callback, callback_context, "<Notes/><IconID>48</IconID>") ||
           !flippass_save_payload_emit_times(ctx, callback, callback_context) ||
           !flippass_save_payload_emit_cstr(
               callback,
               callback_context,
               "<IsExpanded>True</IsExpanded><DefaultAutoTypeSequence/><EnableAutoType>null</EnableAutoType><EnableSearching>null</EnableSearching><LastTopVisibleEntry>AAAAAAAAAAAAAAAAAAAAAA==</LastTopVisibleEntry><Name>") ||
           !flippass_save_payload_emit_xml_escaped(
               callback, callback_context, group->name != NULL ? group->name : "Root") ||
           !flippass_save_payload_emit_cstr(callback, callback_context, "</Name>")) {
            return false;
        }
    } else if(
        !flippass_save_payload_emit_times(ctx, callback, callback_context) ||
        !flippass_save_payload_emit_cstr(callback, callback_context, "<Name>") ||
        !flippass_save_payload_emit_xml_escaped(
            callback, callback_context, group->name != NULL ? group->name : "") ||
        !flippass_save_payload_emit_cstr(callback, callback_context, "</Name>")) {
        return false;
    }

    for(KDBXGroup* child = group->children; child != NULL; child = child->next) {
        if(!flippass_save_payload_emit_group(
               ctx, callback, callback_context, child, false, error)) {
            return false;
        }
    }

    for(KDBXEntry* entry = group->entries; entry != NULL; entry = entry->next) {
        if(!flippass_save_payload_emit_entry(ctx, callback, callback_context, entry, error)) {
            return false;
        }
    }

    return flippass_save_payload_emit_cstr(callback, callback_context, "</Group>");
}

static bool flippass_save_stream_payload(
    FlipPassSavePluginContext* ctx,
    FlipPassSaveChunkCallback callback,
    void* callback_context,
    FuriString* error) {
    uint8_t inner_key[64];
    uint8_t inner_material[KDBX_PROTECTED_STREAM_MATERIAL_MAX];
    uint8_t inner_header_alg[4] = {3U, 0U, 0U, 0U};
    uint8_t inner_field_header[5];
    const char* database_name =
        (ctx->request->database_name != NULL && ctx->request->database_name[0] != '\0') ?
            ctx->request->database_name :
            "Database";

    if(ctx->request->root_group == NULL) {
        flippass_save_set_error(ctx, error, "FlipPass has no database model to save.");
        return false;
    }

    kdbx_protected_stream_reset(&ctx->protected_stream);
    if(!flippass_save_build_time_base64(ctx->now_base64)) {
        flippass_save_set_error(
            ctx, error, "FlipPass could not encode the KDBX timestamp fields.");
        return false;
    }

    furi_hal_random_fill_buf(inner_key, sizeof(inner_key));
    sha512_Raw(inner_key, sizeof(inner_key), inner_material);
    if(!kdbx_protected_stream_init_prederived(
           &ctx->protected_stream,
           KDBXProtectedStreamChaCha20,
           inner_material,
           KDBX_PROTECTED_STREAM_CHACHA20_MATERIAL_SIZE)) {
        memzero(inner_key, sizeof(inner_key));
        memzero(inner_material, sizeof(inner_material));
        flippass_save_set_error(
            ctx, error, "FlipPass could not initialize the inner protected stream.");
        return false;
    }
    memzero(inner_material, sizeof(inner_material));

    inner_field_header[0] = 1U;
    inner_field_header[1] = 4U;
    inner_field_header[2] = 0U;
    inner_field_header[3] = 0U;
    inner_field_header[4] = 0U;
    if(!flippass_save_payload_emit(
           callback, callback_context, inner_field_header, sizeof(inner_field_header)) ||
       !flippass_save_payload_emit(
           callback, callback_context, inner_header_alg, sizeof(inner_header_alg))) {
        memzero(inner_key, sizeof(inner_key));
        return false;
    }

    inner_field_header[0] = 2U;
    inner_field_header[1] = sizeof(inner_key);
    inner_field_header[2] = 0U;
    inner_field_header[3] = 0U;
    inner_field_header[4] = 0U;
    if(!flippass_save_payload_emit(
           callback, callback_context, inner_field_header, sizeof(inner_field_header)) ||
       !flippass_save_payload_emit(callback, callback_context, inner_key, sizeof(inner_key))) {
        memzero(inner_key, sizeof(inner_key));
        return false;
    }

    inner_field_header[0] = 0U;
    inner_field_header[1] = 0U;
    inner_field_header[2] = 0U;
    inner_field_header[3] = 0U;
    inner_field_header[4] = 0U;
    if(!flippass_save_payload_emit(
           callback, callback_context, inner_field_header, sizeof(inner_field_header))) {
        memzero(inner_key, sizeof(inner_key));
        return false;
    }
    memzero(inner_key, sizeof(inner_key));

    if(!flippass_save_payload_emit_meta(ctx, callback, callback_context, database_name) ||
       !flippass_save_payload_emit_group(
           ctx, callback, callback_context, ctx->request->root_group, true, error) ||
       !flippass_save_payload_emit_cstr(
           callback, callback_context, "<DeletedObjects/></Root></KeePassFile>")) {
        if(furi_string_empty(error)) {
            flippass_save_set_error(
                ctx, error, "FlipPass could not serialize the editable database.");
        }
        return false;
    }

    return true;
}

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

static bool flippass_save_run_internal(FlipPassSavePluginContext* ctx, FuriString* error) {
    bool ok = false;

    flippass_save_log_heap(ctx, "run_internal_enter");
    flippass_save_progress(ctx, "Preparing Payload", "Opening temp file", 38U);

    if(!flippass_save_open_payload_target(ctx, error)) {
        goto cleanup;
    }
    flippass_save_log_heap(ctx, "payload_target_open_ok");

    flippass_save_progress(ctx, "Preparing Payload", "Initializing cipher", 39U);

    if(!flippass_save_prepare_payload_cipher(ctx, error)) {
        goto cleanup;
    }
    flippass_save_log_heap(ctx, "payload_cipher_ok");

    flippass_save_progress(ctx, "Building XML", "Streaming model", 40U);
    ctx->payload_input_size = 0U;
    ctx->progress_next_payload_size = 0U;

    if(ctx->request->compression == KDBX_COMPRESSION_GZIP) {
        flippass_save_progress(ctx, "Preparing GZip", "Stored blocks; no dictionary", 42U);
        ctx->progress_next_payload_size = 0U;
        if(!flippass_save_gzip_begin(ctx, error) ||
           !flippass_save_stream_payload(ctx, flippass_save_gzip_payload_sink, ctx, error) ||
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
    } else if(!flippass_save_stream_payload(ctx, flippass_save_payload_sink, ctx, error)) {
        if(furi_string_empty(error)) {
            flippass_save_set_error(ctx, error, "FlipPass could not stream the KDBX payload.");
        }
        goto cleanup;
    }
    flippass_save_log_heap(ctx, "payload_ok");

    flippass_save_progress(ctx, "Encrypting Payload", "Final blocks", 78U);

    if(!flippass_save_finalize_payload(ctx, error)) {
        goto cleanup;
    }
    flippass_save_log_heap(ctx, "final_blocks_ok");

    flippass_save_progress(ctx, "Saving File", "Replacing target", 92U);

    if(!flippass_save_commit_target(ctx, error)) {
        goto cleanup;
    }
    flippass_save_log_heap(ctx, "commit_ok");

    flippass_save_progress(ctx, "Done", "Saved", 100U);
    ok = true;

cleanup:
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
    flippass_save_release_gzip_buffer(ctx);
    if(!ctx->use_aes) {
        kdbx_protected_stream_reset(&ctx->cipher.chacha_stream);
    }
    memzero(&ctx->cipher, sizeof(ctx->cipher));
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

    const size_t expected_iv_size = (request->cipher == FlipPassSaveCipherChaCha20) ? 12U : 16U;
    if(request->api_version != FLIPPASS_SAVE_PLUGIN_API_VERSION ||
       host_api->api_version != FLIPPASS_SAVE_HOST_API_VERSION || request->file_path == NULL ||
       request->cipher_key == NULL || request->cipher_key_size != 32U ||
       request->hmac_base == NULL || request->hmac_base_size != 64U || request->iv == NULL ||
       request->iv_size != expected_iv_size || request->root_group == NULL ||
       host_api->copy_group_uuid == NULL || host_api->copy_entry_uuid == NULL ||
       host_api->entry_has_field == NULL || host_api->stream_ref == NULL ||
       (request->compression != KDBX_COMPRESSION_NONE &&
        request->compression != KDBX_COMPRESSION_GZIP)) {
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

    ctx->storage = furi_record_open(RECORD_STORAGE);
    ctx->temp_path = furi_string_alloc();
    ctx->error_detail = furi_string_alloc();
    ctx->uuid = furi_string_alloc();
    ctx->request = request;
    ctx->host_api = host_api;
    ctx->use_aes = request->cipher != FlipPassSaveCipherChaCha20;
    memzero(&ctx->cipher, sizeof(ctx->cipher));
    flippass_save_log_heap(ctx, "context_ready");

    if(ctx->temp_path == NULL || ctx->error_detail == NULL || ctx->uuid == NULL) {
        furi_string_set_str(error, "Not enough RAM is available for save paths.");
        goto cleanup;
    }

    ok = flippass_save_run_internal(ctx, error);

cleanup:
    if(ctx->file != NULL) {
        storage_file_close(ctx->file);
        storage_file_free(ctx->file);
    }
    if(ctx->temp_path != NULL) {
        furi_string_free(ctx->temp_path);
    }
    if(ctx->error_detail != NULL) {
        furi_string_free(ctx->error_detail);
    }
    if(ctx->uuid != NULL) {
        furi_string_free(ctx->uuid);
    }
    kdbx_protected_stream_reset(&ctx->protected_stream);
    if(ctx->storage != NULL) {
        furi_record_close(RECORD_STORAGE);
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
