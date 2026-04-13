#include "kdbx_parser.h"
#include "kdbx_gzip.h"
#include "kdbx_protected.h"
#include <furi.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if !FLIPPASS_ENABLE_PARSER_FURI_TRACE
#ifdef FURI_LOG_T
#undef FURI_LOG_T
#endif
#define FURI_LOG_T(...) \
    do {                \
    } while(0)
#endif

struct KDBXParser {
    Storage* storage;
    Stream* stream;
    KDBXHeader header;
    bool header_parsed;
    bool stream_open;
    char last_error[160];
    size_t decrypt_budget_free_heap;
    size_t decrypt_budget_max_free_block;
    KDBXParserKdfProgressCallback kdf_progress_callback;
    void* kdf_progress_context;
};

#define KDBX_HEADER_VERIFICATION_BYTES 64U
#define KDBX_MAX_PAYLOAD_SIZE          (256U * 1024U)
#define KDBX_MAX_BLOCK_SIZE            KDBX_MAX_PAYLOAD_SIZE
#define KDBX_STREAM_IO_CHUNK_SIZE      256U
#define KDBX_MAX_STREAM_OUTPUT_SIZE    (2U * 1024U * 1024U)
#define KDBX_PARSER_LOG_PATH           EXT_PATH("apps_data/flippass/flippass.log")
#define KDBX_PARSER_TRACE_TAG          "FlipPassParser"
#define KDBX_ARGON2_UNSUPPORTED_MESSAGE \
    "Argon2 KDF is not viable on this device. This database cannot be opened."

static bool kdbx_parser_read_header(KDBXParser* parser);
static void kdbx_parser_clear_header(KDBXParser* parser);
static void kdbx_parser_release_kdf_parameters(KDBXParser* parser);
static void kdbx_parser_clear_error(KDBXParser* parser);
static void kdbx_parser_set_error(KDBXParser* parser, const char* format, ...);
#if FLIPPASS_ENABLE_VERBOSE_UNLOCK_LOG
static void kdbx_parser_trace(const KDBXParser* parser, const char* format, ...);
#else
#define kdbx_parser_trace(...) \
    do {                       \
    } while(0)
#endif
static bool kdbx_parser_has_bytes(const uint8_t* p, const uint8_t* end, size_t needed);
static bool kdbx_parser_derive_transformed_key(
    const KDBXParser* parser,
    const char* password,
    uint8_t transformed_key[32]);
static bool kdbx_parser_stream_emit(
    KDBXParser* parser,
    KDBXParserOutputCallback callback,
    void* context,
    size_t* emitted_bytes,
    const uint8_t* data,
    size_t data_size);
static bool kdbx_parser_stream_payload_internal(
    KDBXParser* parser,
    const uint8_t* cipher_key,
    size_t cipher_key_size,
    const uint8_t* hmac_key,
    size_t hmac_key_size,
    KDBXParserOutputCallback callback,
    void* context,
    bool inflate_gzip);
static void kdbx_parser_trace_fail(KDBXParser* parser, const char* stage);
static void kdbx_parser_set_gzip_error(KDBXParser* parser, const KDBXGzipTelemetry* telemetry);
static inline uint32_t read_uint32_le(const uint8_t* p);
static inline uint64_t read_uint64_le(const uint8_t* p);
static bool
    kdbx_parser_variant_name_equals(const char* name, uint32_t name_size, const char* expected);
static bool kdbx_parser_variant_read_uint(
    uint8_t value_type,
    const uint8_t* value,
    uint32_t value_size,
    uint64_t* out_value);

typedef enum {
    KDBXParserKdfTypeInvalid = 0,
    KDBXParserKdfTypeAes,
    KDBXParserKdfTypeArgon2Unsupported,
} KDBXParserKdfType;

typedef struct {
    KDBXParserKdfType type;
    const uint8_t* salt;
    size_t salt_size;
    uint64_t rounds;
} KDBXParserKdfParameters;

static bool
    kdbx_parser_parse_kdf_parameters(const KDBXParser* parser, KDBXParserKdfParameters* params);
static bool kdbx_parser_derive_transformed_key_aes(
    const KDBXParser* parser,
    const char* password,
    const KDBXParserKdfParameters* params,
    uint8_t transformed_key[32]);

typedef struct {
    uint8_t* data;
    size_t size;
    size_t capacity;
} KDBXParserHeapBuffer;

typedef struct {
    aes_decrypt_ctx aes_ctx;
    uint8_t iv[16];
    uint8_t partial_block[16];
    size_t partial_len;
    uint8_t pending_block[16];
    bool has_pending_block;
} KDBXParserAesState;

typedef struct {
    uint8_t* data;
    size_t size;
    size_t capacity;
    bool failed;
} KDBXParserCollectState;

static bool kdbx_parser_heap_buffer_reserve(KDBXParserHeapBuffer* buffer, size_t needed_capacity) {
    uint8_t* next = NULL;

    furi_assert(buffer);

    if(needed_capacity <= buffer->capacity) {
        return true;
    }

    const size_t new_capacity = needed_capacity;
    if(new_capacity >= 1024U) {
        FURI_LOG_T(
            KDBX_PARSER_TRACE_TAG,
            "gzip member reserve need=%lu old=%lu free=%lu max=%lu",
            (unsigned long)new_capacity,
            (unsigned long)buffer->capacity,
            (unsigned long)memmgr_get_free_heap(),
            (unsigned long)memmgr_heap_get_max_free_block());
    }

    next = malloc(new_capacity);
    if(next == NULL) {
        FURI_LOG_T(
            KDBX_PARSER_TRACE_TAG,
            "gzip member reserve failed need=%lu old=%lu free=%lu max=%lu",
            (unsigned long)new_capacity,
            (unsigned long)buffer->capacity,
            (unsigned long)memmgr_get_free_heap(),
            (unsigned long)memmgr_heap_get_max_free_block());
        return false;
    }

    if(buffer->data != NULL) {
        if(buffer->size > 0U) {
            memcpy(next, buffer->data, buffer->size);
        }
        memzero(buffer->data, buffer->capacity);
        free(buffer->data);
    }

    buffer->data = next;
    buffer->capacity = new_capacity;
    if(new_capacity >= 1024U) {
        FURI_LOG_T(
            KDBX_PARSER_TRACE_TAG,
            "gzip member reserve ok new=%lu size=%lu",
            (unsigned long)buffer->capacity,
            (unsigned long)buffer->size);
    }
    return true;
}

static bool kdbx_parser_heap_buffer_append(
    KDBXParserHeapBuffer* buffer,
    const uint8_t* data,
    size_t data_size) {
    furi_assert(buffer);

    if(data_size == 0U) {
        return true;
    }

    if(data == NULL || data_size > (SIZE_MAX - buffer->size)) {
        return false;
    }

    if(!kdbx_parser_heap_buffer_reserve(buffer, buffer->size + data_size)) {
        return false;
    }

    memcpy(buffer->data + buffer->size, data, data_size);
    buffer->size += data_size;
    return true;
}

static void kdbx_parser_heap_buffer_free(KDBXParserHeapBuffer* buffer) {
    if(buffer == NULL) {
        return;
    }

    if(buffer->data != NULL) {
        memzero(buffer->data, buffer->capacity);
        free(buffer->data);
    }

    memset(buffer, 0, sizeof(*buffer));
}

static bool kdbx_parser_collect_callback(const uint8_t* data, size_t data_size, void* context) {
    KDBXParserCollectState* collect = context;
    furi_assert(collect);

    KDBXParserHeapBuffer buffer = {
        .data = collect->data,
        .size = collect->size,
        .capacity = collect->capacity,
    };

    if(!kdbx_parser_heap_buffer_append(&buffer, data, data_size)) {
        collect->failed = true;
        return false;
    }

    collect->data = buffer.data;
    collect->size = buffer.size;
    collect->capacity = buffer.capacity;
    return true;
}

static void kdbx_parser_compute_block_key(
    const uint8_t* hmac_key,
    size_t hmac_key_size,
    uint64_t block_index,
    uint8_t block_key[64]) {
    uint8_t block_index_bytes[8];
    uint8_t block_key_input[72];

    for(size_t i = 0; i < sizeof(block_index_bytes); ++i) {
        block_index_bytes[i] = (uint8_t)((block_index >> (i * 8U)) & 0xFFU);
    }

    memcpy(block_key_input, block_index_bytes, sizeof(block_index_bytes));
    memcpy(block_key_input + sizeof(block_index_bytes), hmac_key, hmac_key_size);
    sha512_Raw(block_key_input, sizeof(block_key_input), block_key);
    memzero(block_key_input, sizeof(block_key_input));
}

static bool kdbx_parser_verify_block_hmac(
    KDBXParser* parser,
    const uint8_t* hmac_key,
    size_t hmac_key_size,
    uint64_t block_index,
    uint32_t block_size,
    const uint8_t expected_hmac[32],
    uint8_t* io_buffer,
    size_t io_buffer_size) {
    HMAC_SHA256_CTX hmac_ctx;
    uint8_t block_key[64];
    uint8_t block_hmac[32];
    uint8_t block_index_bytes[8];
    const size_t payload_offset = stream_tell(parser->stream);
    size_t remaining = block_size;

    if(block_size == 0U) {
        FURI_LOG_T(
            KDBX_PARSER_TRACE_TAG,
            "block zero verify begin index=%llu offset=%lu",
            (unsigned long long)block_index,
            (unsigned long)payload_offset);
    }

    for(size_t i = 0; i < sizeof(block_index_bytes); ++i) {
        block_index_bytes[i] = (uint8_t)((block_index >> (i * 8U)) & 0xFFU);
    }

    kdbx_parser_compute_block_key(hmac_key, hmac_key_size, block_index, block_key);
    hmac_sha256_Init(&hmac_ctx, block_key, sizeof(block_key));
    hmac_sha256_Update(&hmac_ctx, block_index_bytes, sizeof(block_index_bytes));
    hmac_sha256_Update(&hmac_ctx, (const uint8_t*)&block_size, sizeof(block_size));

    while(remaining > 0U) {
        const size_t chunk_size = (remaining > io_buffer_size) ? io_buffer_size : remaining;
        if(stream_read(parser->stream, io_buffer, chunk_size) != chunk_size) {
            memzero(block_key, sizeof(block_key));
            return false;
        }
        hmac_sha256_Update(&hmac_ctx, io_buffer, chunk_size);
        remaining -= chunk_size;
    }

    hmac_sha256_Final(&hmac_ctx, block_hmac);
    if(block_size == 0U) {
        FURI_LOG_T(
            KDBX_PARSER_TRACE_TAG,
            "block zero verify hash index=%llu",
            (unsigned long long)block_index);
    }
    memzero(block_key, sizeof(block_key));

    if(!stream_seek(parser->stream, payload_offset, StreamOffsetFromStart)) {
        return false;
    }
    if(block_size == 0U) {
        FURI_LOG_T(
            KDBX_PARSER_TRACE_TAG,
            "block zero verify seek index=%llu",
            (unsigned long long)block_index);
    }

    return memcmp(expected_hmac, block_hmac, sizeof(block_hmac)) == 0;
}

static void kdbx_parser_aes_state_init(
    KDBXParserAesState* state,
    const uint8_t* cipher_key,
    const uint8_t iv[16]) {
    furi_assert(state);
    furi_assert(cipher_key);
    furi_assert(iv);

    memset(state, 0, sizeof(*state));
    aes_decrypt_key256(cipher_key, &state->aes_ctx);
    memcpy(state->iv, iv, 16U);
}

static bool kdbx_parser_plain_sink(
    KDBXParser* parser,
    size_t* emitted_bytes,
    KDBXParserOutputCallback callback,
    void* context,
    KDBXParserHeapBuffer* gzip_buffer,
    const uint8_t* data,
    size_t data_size) {
    if(data_size == 0U) {
        return true;
    }

    if(gzip_buffer != NULL) {
        if(gzip_buffer->size > (KDBX_MAX_STREAM_OUTPUT_SIZE - data_size) ||
           !kdbx_parser_heap_buffer_append(gzip_buffer, data, data_size)) {
            kdbx_parser_set_error(
                parser, "The compressed database exceeds FlipPass's stream buffer limit.");
            return false;
        }
        return true;
    }

    return kdbx_parser_stream_emit(parser, callback, context, emitted_bytes, data, data_size);
}

static bool kdbx_parser_aes_emit_decrypted_block(
    KDBXParserAesState* state,
    KDBXParser* parser,
    size_t* emitted_bytes,
    KDBXParserOutputCallback callback,
    void* context,
    KDBXParserHeapBuffer* gzip_buffer,
    const uint8_t ciphertext_block[16]) {
    uint8_t plaintext[16];

    if(aes_decrypt(state->pending_block, plaintext, &state->aes_ctx) != EXIT_SUCCESS) {
        kdbx_parser_set_error(parser, "Unable to decrypt the database payload.");
        memzero(plaintext, sizeof(plaintext));
        return false;
    }

    for(size_t i = 0; i < sizeof(plaintext); ++i) {
        plaintext[i] ^= state->iv[i];
    }

    memcpy(state->iv, state->pending_block, sizeof(state->iv));
    memcpy(state->pending_block, ciphertext_block, sizeof(state->pending_block));

    const bool ok = kdbx_parser_plain_sink(
        parser, emitted_bytes, callback, context, gzip_buffer, plaintext, sizeof(plaintext));
    memzero(plaintext, sizeof(plaintext));
    return ok;
}

static bool kdbx_parser_aes_state_feed(
    KDBXParserAesState* state,
    KDBXParser* parser,
    size_t* emitted_bytes,
    KDBXParserOutputCallback callback,
    void* context,
    KDBXParserHeapBuffer* gzip_buffer,
    const uint8_t* ciphertext,
    size_t ciphertext_size) {
    furi_assert(state);

    while(ciphertext_size > 0U) {
        const size_t needed = 16U - state->partial_len;
        const size_t take = (ciphertext_size < needed) ? ciphertext_size : needed;

        memcpy(state->partial_block + state->partial_len, ciphertext, take);
        state->partial_len += take;
        ciphertext += take;
        ciphertext_size -= take;

        if(state->partial_len < 16U) {
            continue;
        }

        if(!state->has_pending_block) {
            memcpy(state->pending_block, state->partial_block, sizeof(state->pending_block));
            state->has_pending_block = true;
        } else if(!kdbx_parser_aes_emit_decrypted_block(
                      state,
                      parser,
                      emitted_bytes,
                      callback,
                      context,
                      gzip_buffer,
                      state->partial_block)) {
            return false;
        }

        memzero(state->partial_block, sizeof(state->partial_block));
        state->partial_len = 0U;
    }

    return true;
}

static bool kdbx_parser_aes_state_finish(
    KDBXParserAesState* state,
    KDBXParser* parser,
    size_t* emitted_bytes,
    KDBXParserOutputCallback callback,
    void* context,
    KDBXParserHeapBuffer* gzip_buffer) {
    uint8_t plaintext[16];

    FURI_LOG_T(
        KDBX_PARSER_TRACE_TAG,
        "aes finish enter partial=%lu pending=%u emitted=%lu gzip_size=%lu gzip_cap=%lu free=%lu max=%lu",
        (unsigned long)state->partial_len,
        state->has_pending_block ? 1U : 0U,
        (unsigned long)(emitted_bytes != NULL ? *emitted_bytes : 0U),
        (unsigned long)(gzip_buffer != NULL ? gzip_buffer->size : 0U),
        (unsigned long)(gzip_buffer != NULL ? gzip_buffer->capacity : 0U),
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());

    if(state->partial_len != 0U || !state->has_pending_block) {
        kdbx_parser_set_error(parser, "The AES payload is not aligned to a full block.");
        FURI_LOG_T(KDBX_PARSER_TRACE_TAG, "aes finish invalid alignment");
        return false;
    }

    if(aes_decrypt(state->pending_block, plaintext, &state->aes_ctx) != EXIT_SUCCESS) {
        kdbx_parser_set_error(parser, "Unable to decrypt the database payload.");
        FURI_LOG_T(KDBX_PARSER_TRACE_TAG, "aes finish decrypt failed");
        memzero(plaintext, sizeof(plaintext));
        return false;
    }

    for(size_t i = 0; i < sizeof(plaintext); ++i) {
        plaintext[i] ^= state->iv[i];
    }

    const uint8_t padding = plaintext[15];
    if(padding == 0U || padding > 16U) {
        kdbx_parser_set_error(parser, "The AES payload padding is invalid.");
        FURI_LOG_T(KDBX_PARSER_TRACE_TAG, "aes finish invalid padding value=%u", padding);
        memzero(plaintext, sizeof(plaintext));
        return false;
    }

    for(size_t i = 0; i < padding; ++i) {
        if(plaintext[15U - i] != padding) {
            kdbx_parser_set_error(parser, "The AES payload padding is invalid.");
            FURI_LOG_T(
                KDBX_PARSER_TRACE_TAG,
                "aes finish invalid padding tail index=%lu value=%u expected=%u",
                (unsigned long)i,
                plaintext[15U - i],
                padding);
            memzero(plaintext, sizeof(plaintext));
            return false;
        }
    }

    const size_t plain_len = 16U - padding;
    FURI_LOG_T(
        KDBX_PARSER_TRACE_TAG,
        "aes finish sink begin plain=%lu gzip_size=%lu gzip_cap=%lu",
        (unsigned long)plain_len,
        (unsigned long)(gzip_buffer != NULL ? gzip_buffer->size : 0U),
        (unsigned long)(gzip_buffer != NULL ? gzip_buffer->capacity : 0U));
    const bool ok = kdbx_parser_plain_sink(
        parser, emitted_bytes, callback, context, gzip_buffer, plaintext, plain_len);
    FURI_LOG_T(
        KDBX_PARSER_TRACE_TAG,
        "aes finish sink end ok=%u emitted=%lu gzip_size=%lu gzip_cap=%lu free=%lu max=%lu",
        ok ? 1U : 0U,
        (unsigned long)(emitted_bytes != NULL ? *emitted_bytes : 0U),
        (unsigned long)(gzip_buffer != NULL ? gzip_buffer->size : 0U),
        (unsigned long)(gzip_buffer != NULL ? gzip_buffer->capacity : 0U),
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());
    memzero(plaintext, sizeof(plaintext));
    memzero(state, sizeof(*state));
    return ok;
}

static void kdbx_parser_trace_fail(KDBXParser* parser, const char* stage) {
    furi_assert(parser);
    furi_assert(stage);

    kdbx_parser_trace(
        parser,
        "PAYLOAD_STREAM_FAIL stage=%s reason=%s",
        stage,
        parser->last_error[0] != '\0' ? parser->last_error : "unknown");
}

static void kdbx_parser_set_gzip_error(KDBXParser* parser, const KDBXGzipTelemetry* telemetry) {
    furi_assert(parser);
    furi_assert(telemetry);

    kdbx_parser_trace(
        parser,
        "PAYLOAD_GZIP_FAIL status=%u expected=%lu actual=%lu consumed=%lu max_free=%lu",
        (unsigned)telemetry->status,
        (unsigned long)telemetry->expected_output_size,
        (unsigned long)telemetry->actual_output_size,
        (unsigned long)telemetry->consumed_input_size,
        (unsigned long)telemetry->max_free_block);

    switch(telemetry->status) {
    case KDBXGzipStatusInvalidHeader:
        kdbx_parser_set_error(parser, "The database payload is not valid GZip data.");
        break;
    case KDBXGzipStatusReservedFlags:
        kdbx_parser_set_error(parser, "The database GZip header uses unsupported flags.");
        break;
    case KDBXGzipStatusInvalidExtraField:
        kdbx_parser_set_error(parser, "The database GZip extra field is truncated.");
        break;
    case KDBXGzipStatusInvalidNameField:
        kdbx_parser_set_error(parser, "The database GZip filename field is truncated.");
        break;
    case KDBXGzipStatusInvalidCommentField:
        kdbx_parser_set_error(parser, "The database GZip comment field is truncated.");
        break;
    case KDBXGzipStatusInvalidHeaderCrcField:
        kdbx_parser_set_error(parser, "The database GZip header checksum is truncated.");
        break;
    case KDBXGzipStatusInvalidBodyOffset:
        kdbx_parser_set_error(parser, "The compressed database payload is empty.");
        break;
    case KDBXGzipStatusTruncatedInput:
    case KDBXGzipStatusInputSizeMismatch:
        kdbx_parser_set_error(parser, "The compressed database payload is truncated.");
        break;
    case KDBXGzipStatusOutputTooLarge:
        kdbx_parser_set_error(
            parser, "This compressed database expands beyond FlipPass's streaming limit.");
        break;
    case KDBXGzipStatusWorkspaceAllocFailed:
    case KDBXGzipStatusWorkspaceTotalTooSmall:
    case KDBXGzipStatusWorkspacePageAllocFailed:
        kdbx_parser_set_error(
            parser, "Not enough RAM is available to keep the GZip dictionary while streaming.");
        break;
    case KDBXGzipStatusWorkspaceStorageFailed:
        kdbx_parser_set_error(
            parser, "The encrypted GZip dictionary scratch file could not be used safely.");
        break;
    case KDBXGzipStatusWorkspaceVerifyFailed:
        kdbx_parser_set_error(
            parser, "The encrypted GZip dictionary scratch file did not verify cleanly.");
        break;
    case KDBXGzipStatusOutputSizeMismatch:
        kdbx_parser_set_error(
            parser,
            "The decompressed database size did not match the GZip trailer (%lu vs %lu).",
            (unsigned long)telemetry->actual_output_size,
            (unsigned long)telemetry->expected_output_size);
        break;
    case KDBXGzipStatusCrcMismatch:
        kdbx_parser_set_error(
            parser, "The decompressed database CRC did not match the GZip trailer.");
        break;
    case KDBXGzipStatusOutputRejected:
        if(parser->last_error[0] == '\0') {
            kdbx_parser_set_error(parser, "The payload consumer rejected the database stream.");
        }
        break;
    case KDBXGzipStatusInflateFailed:
    case KDBXGzipStatusOutputAllocFailed:
    case KDBXGzipStatusOutputHeapFragmented:
    case KDBXGzipStatusInvalidArgument:
    default:
        if(parser->last_error[0] == '\0') {
            kdbx_parser_set_error(parser, "Unable to decompress the database payload.");
        }
        break;
    }
}

KDBXParser* kdbx_parser_alloc() {
    KDBXParser* parser = malloc(sizeof(KDBXParser));
    furi_assert(parser);
    parser->storage = furi_record_open(RECORD_STORAGE);
    parser->stream = file_stream_alloc(parser->storage);
    parser->header_parsed = false;
    parser->stream_open = false;
    parser->header.kdf_parameters = NULL;
    parser->header.kdf_parameters_size = 0;
    parser->decrypt_budget_free_heap = 0U;
    parser->decrypt_budget_max_free_block = 0U;
    parser->last_error[0] = '\0';
    return parser;
}

static void kdbx_parser_clear_header(KDBXParser* parser) {
    kdbx_parser_release_kdf_parameters(parser);
    memzero(&parser->header, sizeof(parser->header));
    parser->header_parsed = false;
}

static void kdbx_parser_release_kdf_parameters(KDBXParser* parser) {
    if(parser->header.kdf_parameters) {
        memzero(parser->header.kdf_parameters, parser->header.kdf_parameters_size);
        free(parser->header.kdf_parameters);
    }
    parser->header.kdf_parameters = NULL;
    parser->header.kdf_parameters_size = 0;
}

static void kdbx_parser_clear_error(KDBXParser* parser) {
    furi_assert(parser);
    parser->last_error[0] = '\0';
}

static void kdbx_parser_set_error(KDBXParser* parser, const char* format, ...) {
    furi_assert(parser);
    furi_assert(format);

    va_list args;
    va_start(args, format);
    vsnprintf(parser->last_error, sizeof(parser->last_error), format, args);
    va_end(args);
}

#if FLIPPASS_ENABLE_VERBOSE_UNLOCK_LOG
static void kdbx_parser_trace(const KDBXParser* parser, const char* format, ...) {
    furi_assert(parser);
    furi_assert(format);

    Stream* log_stream = file_stream_alloc(parser->storage);
    if(log_stream == NULL) {
        return;
    }

    if(file_stream_open(log_stream, KDBX_PARSER_LOG_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        stream_write_cstring(log_stream, "AUTO: ");

        va_list args;
        va_start(args, format);
        stream_write_vaformat(log_stream, format, args);
        va_end(args);

        stream_write_cstring(log_stream, "\n");
        file_stream_close(log_stream);
    }
    stream_free(log_stream);
}
#endif

void kdbx_parser_reset(KDBXParser* parser) {
    furi_assert(parser);

    if(parser->stream_open) {
        file_stream_close(parser->stream);
        parser->stream_open = false;
    }

    kdbx_parser_clear_error(parser);
    parser->decrypt_budget_free_heap = 0U;
    parser->decrypt_budget_max_free_block = 0U;
    kdbx_parser_clear_header(parser);
}

void kdbx_parser_free(KDBXParser* parser) {
    furi_assert(parser);
    kdbx_parser_reset(parser);
    stream_free(parser->stream);
    furi_record_close(RECORD_STORAGE);
    free(parser);
}

const KDBXHeader* kdbx_parser_get_header(const KDBXParser* parser) {
    furi_assert(parser);
    return parser->header_parsed ? &parser->header : NULL;
}

const char* kdbx_parser_get_last_error(const KDBXParser* parser) {
    furi_assert(parser);
    return parser->last_error;
}

bool kdbx_parser_process_file(KDBXParser* parser, const char* file_path) {
    furi_assert(parser);
    furi_assert(file_path);

    kdbx_parser_trace(
        parser,
        "PROCESS_FILE_BEGIN path=%s free=%lu max=%lu",
        file_path,
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());
    kdbx_parser_reset(parser);
    kdbx_parser_trace(parser, "PROCESS_FILE_RESET_OK");

    if(!file_stream_open(parser->stream, file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E("KDBXParser", "Failed to open file: %s", file_path);
        kdbx_parser_trace(parser, "PROCESS_FILE_OPEN_FAIL");
        return false;
    }
    parser->stream_open = true;
    kdbx_parser_trace(parser, "PROCESS_FILE_OPEN_OK");

    kdbx_parser_trace(parser, "PROCESS_FILE_HEADER_BEGIN");
    if(!kdbx_parser_read_header(parser)) {
        kdbx_parser_trace(parser, "PROCESS_FILE_HEADER_FAIL");
        kdbx_parser_reset(parser);
        return false;
    }
    kdbx_parser_trace(parser, "PROCESS_FILE_HEADER_OK");

    return true;
}

void kdbx_parser_set_kdf_progress_callback(
    KDBXParser* parser,
    KDBXParserKdfProgressCallback callback,
    void* context) {
    furi_assert(parser);

    parser->kdf_progress_callback = callback;
    parser->kdf_progress_context = context;
}

static bool kdbx_parser_read_header(KDBXParser* parser) {
    uint32_t signature1, signature2;
    if(stream_read(parser->stream, (uint8_t*)&signature1, sizeof(signature1)) !=
           sizeof(signature1) ||
       stream_read(parser->stream, (uint8_t*)&signature2, sizeof(signature2)) !=
           sizeof(signature2)) {
        FURI_LOG_E("KDBXParser", "Failed to read signatures");
        return false;
    }

    if(signature1 != KDBX_SIGNATURE_1 || signature2 != KDBX_SIGNATURE_2) {
        FURI_LOG_E("KDBXParser", "Invalid KDBX signature");
        return false;
    }

    uint32_t version;
    if(stream_read(parser->stream, (uint8_t*)&version, sizeof(version)) != sizeof(version)) {
        FURI_LOG_E("KDBXParser", "Failed to read version");
        return false;
    }

    parser->header.version_minor = version & 0xFFFF;
    parser->header.version_major = version >> 16;

    // We only support KDBX4 for now
    if(parser->header.version_major != 4) {
        FURI_LOG_E(
            "KDBXParser",
            "Unsupported KDBX version: %d.%d",
            parser->header.version_major,
            parser->header.version_minor);
        return false;
    }

    while(true) {
        uint8_t field_id;
        if(stream_read(parser->stream, &field_id, sizeof(field_id)) != sizeof(field_id)) {
            FURI_LOG_E("KDBXParser", "Failed to read header field ID");
            return false;
        }

        if(field_id == KDBX_HEADER_FIELD_ID_END) {
            uint32_t field_size = 0;
            if(stream_read(parser->stream, (uint8_t*)&field_size, sizeof(field_size)) !=
               sizeof(field_size)) {
                FURI_LOG_E("KDBXParser", "Failed to read end-of-header field size");
                return false;
            }
            if(field_size != 4) {
                FURI_LOG_E(
                    "KDBXParser",
                    "Unexpected end-of-header marker size: %lu",
                    (unsigned long)field_size);
                return false;
            }

            uint8_t end_marker[4];
            if(stream_read(parser->stream, end_marker, sizeof(end_marker)) != sizeof(end_marker)) {
                FURI_LOG_E("KDBXParser", "Failed to read end-of-header marker bytes");
                return false;
            }
            break;
        }

        uint32_t field_size;
        if(stream_read(parser->stream, (uint8_t*)&field_size, sizeof(field_size)) !=
           sizeof(field_size)) {
            FURI_LOG_E("KDBXParser", "Failed to read header field size for ID %d", field_id);
            return false;
        }

        switch(field_id) {
        case KDBX_HEADER_FIELD_ID_ENCRYPTION_ALGORITHM:
            if(field_size != sizeof(parser->header.encryption_algorithm_uuid)) {
                FURI_LOG_E("KDBXParser", "Invalid size for encryption UUID");
                return false;
            }
            if(stream_read(parser->stream, parser->header.encryption_algorithm_uuid, field_size) !=
               field_size) {
                FURI_LOG_E("KDBXParser", "Failed to read encryption UUID");
                return false;
            }
            break;
        case KDBX_HEADER_FIELD_ID_COMPRESSION_ALGORITHM:
            if(stream_read(
                   parser->stream, (uint8_t*)&parser->header.compression_algorithm, field_size) !=
               field_size) {
                FURI_LOG_E("KDBXParser", "Failed to read compression algorithm");
                return false;
            }
            break;
        case KDBX_HEADER_FIELD_ID_MASTER_SEED:
            if(field_size != sizeof(parser->header.master_seed)) {
                FURI_LOG_E("KDBXParser", "Invalid size for master seed");
                return false;
            }
            if(stream_read(parser->stream, parser->header.master_seed, field_size) != field_size) {
                FURI_LOG_E("KDBXParser", "Failed to read master seed");
                return false;
            }
            break;
        case KDBX_HEADER_FIELD_ID_ENCRYPTION_IV:
            if(field_size > sizeof(parser->header.encryption_iv)) {
                FURI_LOG_E("KDBXParser", "Encryption IV too large");
                return false;
            }
            memzero(parser->header.encryption_iv, sizeof(parser->header.encryption_iv));
            parser->header.encryption_iv_size = field_size;
            if(stream_read(parser->stream, parser->header.encryption_iv, field_size) !=
               field_size) {
                FURI_LOG_E("KDBXParser", "Failed to read encryption IV");
                return false;
            }
            break;
        case KDBX_HEADER_FIELD_ID_KDF_PARAMETERS:
            parser->header.kdf_parameters_size = field_size;
            parser->header.kdf_parameters = malloc(field_size);
            if(!parser->header.kdf_parameters) {
                FURI_LOG_E("KDBXParser", "Failed to allocate KDF parameters");
                return false;
            }
            if(stream_read(parser->stream, parser->header.kdf_parameters, field_size) !=
               field_size) {
                FURI_LOG_E("KDBXParser", "Failed to read KDF parameters");
                free(parser->header.kdf_parameters);
                parser->header.kdf_parameters = NULL;
                return false;
            }
            break;
        default:
            // Skip unknown fields
            if(!stream_seek(parser->stream, field_size, StreamOffsetFromCurrent)) {
                FURI_LOG_E("KDBXParser", "Failed to seek past unknown field %d", field_id);
                return false;
            }
            break;
        }
    }

    parser->header_parsed = true;
    return true;
}

static bool kdbx_parser_stream_emit(
    KDBXParser* parser,
    KDBXParserOutputCallback callback,
    void* context,
    size_t* emitted_bytes,
    const uint8_t* data,
    size_t data_size) {
    furi_assert(parser);
    furi_assert(callback);
    furi_assert(emitted_bytes);

    if(data_size == 0U) {
        return true;
    }

    if(*emitted_bytes > (KDBX_MAX_STREAM_OUTPUT_SIZE - data_size)) {
        kdbx_parser_set_error(parser, "This database exceeds FlipPass's streaming output limit.");
        return false;
    }

    if(!callback(data, data_size, context)) {
        if(parser->last_error[0] == '\0') {
            kdbx_parser_set_error(parser, "The payload consumer rejected the database stream.");
        }
        return false;
    }

    *emitted_bytes += data_size;
    return true;
}

static bool kdbx_parser_stream_payload_internal(
    KDBXParser* parser,
    const uint8_t* cipher_key,
    size_t cipher_key_size,
    const uint8_t* hmac_key,
    size_t hmac_key_size,
    KDBXParserOutputCallback callback,
    void* context,
    bool inflate_gzip) {
    furi_assert(parser);
    furi_assert(cipher_key);
    furi_assert(hmac_key);
    furi_assert(callback);

    kdbx_parser_clear_error(parser);
    parser->decrypt_budget_free_heap = memmgr_get_free_heap();
    parser->decrypt_budget_max_free_block = memmgr_heap_get_max_free_block();
    FURI_LOG_T(
        KDBX_PARSER_TRACE_TAG,
        "payload start compression=%lu cipher=%s free=%lu max=%lu",
        (unsigned long)parser->header.compression_algorithm,
        memcmp(
            parser->header.encryption_algorithm_uuid,
            KDBX_UUID_AES256,
            sizeof(KDBX_UUID_AES256)) == 0 ?
            "aes" :
            "chacha20",
        (unsigned long)parser->decrypt_budget_free_heap,
        (unsigned long)parser->decrypt_budget_max_free_block);

    if(cipher_key_size != 32 || hmac_key_size != 64) {
        FURI_LOG_E("KDBXParser", "Invalid cipher or HMAC key size");
        kdbx_parser_set_error(parser, "FlipPass received invalid derived key sizes.");
        kdbx_parser_trace_fail(parser, "key_sizes");
        return false;
    }
    const bool use_aes = memcmp(
                             parser->header.encryption_algorithm_uuid,
                             KDBX_UUID_AES256,
                             sizeof(KDBX_UUID_AES256)) == 0;
    const bool use_chacha20 = memcmp(
                                  parser->header.encryption_algorithm_uuid,
                                  KDBX_UUID_CHACHA20,
                                  sizeof(KDBX_UUID_CHACHA20)) == 0;

    if(!parser->header_parsed) {
        FURI_LOG_E("KDBXParser", "Header not parsed, cannot decrypt payload");
        kdbx_parser_set_error(parser, "The database header is not ready for payload decryption.");
        kdbx_parser_trace_fail(parser, "header_not_ready");
        return false;
    }
    if(!parser->stream_open) {
        FURI_LOG_E("KDBXParser", "Payload stream is not open");
        kdbx_parser_set_error(parser, "The database payload stream is not open.");
        kdbx_parser_trace_fail(parser, "stream_not_open");
        return false;
    }
    if(!use_aes && !use_chacha20) {
        FURI_LOG_E("KDBXParser", "Unsupported payload cipher");
        kdbx_parser_set_error(parser, "This database uses an unsupported payload cipher.");
        kdbx_parser_trace_fail(parser, "cipher_unsupported");
        return false;
    }

    size_t stream_remaining = stream_size(parser->stream) - stream_tell(parser->stream);
    if(stream_remaining < KDBX_HEADER_VERIFICATION_BYTES) {
        FURI_LOG_E("KDBXParser", "KDBX4 header verification data is truncated");
        kdbx_parser_set_error(parser, "The KDBX4 header verification bytes are truncated.");
        kdbx_parser_trace_fail(parser, "verification_bytes");
        return false;
    }
    if(!stream_seek(parser->stream, KDBX_HEADER_VERIFICATION_BYTES, StreamOffsetFromCurrent)) {
        FURI_LOG_E("KDBXParser", "Failed to skip KDBX4 header verification data");
        kdbx_parser_set_error(parser, "Unable to skip the KDBX4 header verification bytes.");
        kdbx_parser_trace_fail(parser, "verification_skip");
        return false;
    }
    FURI_LOG_T(
        KDBX_PARSER_TRACE_TAG,
        "payload skip ok remaining=%lu",
        (unsigned long)(stream_size(parser->stream) - stream_tell(parser->stream)));
    FURI_LOG_T(
        KDBX_PARSER_TRACE_TAG,
        "io alloc begin chunk=%lu free=%lu max=%lu",
        (unsigned long)KDBX_STREAM_IO_CHUNK_SIZE,
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());

    uint8_t* io_buffer = malloc(KDBX_STREAM_IO_CHUNK_SIZE);
    if(io_buffer == NULL) {
        kdbx_parser_set_error(parser, "Not enough RAM is available for the payload stream.");
        kdbx_parser_trace_fail(parser, "io_buffer_alloc");
        return false;
    }
    FURI_LOG_T(
        KDBX_PARSER_TRACE_TAG,
        "io alloc ok free=%lu max=%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());

    KDBXParserHeapBuffer gzip_buffer = {0};
    KDBXGzipTelemetry gzip_telemetry = {0};
    const bool buffer_gzip_member =
        (parser->header.compression_algorithm == KDBX_COMPRESSION_GZIP) && inflate_gzip;
    size_t emitted_bytes = 0U;
    size_t block_bytes = 0U;
    size_t plain_bytes_before_gzip = 0U;
    uint64_t block_index = 0U;
    KDBXParserAesState aes_state;
    KDBXProtectedStream chacha_stream;
    memset(&aes_state, 0, sizeof(aes_state));
    kdbx_protected_stream_reset(&chacha_stream);

    if(buffer_gzip_member) {
        if(!kdbx_parser_heap_buffer_reserve(&gzip_buffer, KDBX_STREAM_IO_CHUNK_SIZE)) {
            free(io_buffer);
            kdbx_parser_set_error(
                parser, "Not enough RAM is available for the GZip stream buffer.");
            kdbx_parser_trace_fail(parser, "gzip_buffer_alloc");
            return false;
        }
    } else if(
        parser->header.compression_algorithm != KDBX_COMPRESSION_NONE &&
        parser->header.compression_algorithm != KDBX_COMPRESSION_GZIP) {
        free(io_buffer);
        kdbx_parser_set_error(parser, "This database uses an unsupported compression algorithm.");
        kdbx_parser_trace_fail(parser, "compression_unsupported");
        return false;
    }
    FURI_LOG_T(
        KDBX_PARSER_TRACE_TAG,
        "io ready chunk=%lu gzip_buffer=%lu",
        (unsigned long)KDBX_STREAM_IO_CHUNK_SIZE,
        (unsigned long)gzip_buffer.capacity);

    if(use_aes) {
        kdbx_parser_aes_state_init(&aes_state, cipher_key, parser->header.encryption_iv);
    } else if(!kdbx_chacha20_stream_init(
                  &chacha_stream,
                  cipher_key,
                  cipher_key_size,
                  parser->header.encryption_iv,
                  parser->header.encryption_iv_size,
                  0U)) {
        free(io_buffer);
        kdbx_parser_set_error(parser, "Unable to initialize the ChaCha20 payload stream.");
        kdbx_parser_trace_fail(parser, "chacha_init");
        return false;
    }

    while(true) {
        uint8_t hmac[32];
        if(stream_read(parser->stream, hmac, 32) != 32) {
            FURI_LOG_E("KDBXParser", "Failed to read HMAC for block %llu", block_index);
            kdbx_parser_set_error(parser, "Unable to read the encrypted database payload.");
            kdbx_parser_trace(
                parser, "PAYLOAD_BLOCKS_FAIL cause=read_hmac block=%llu", block_index);
            kdbx_parser_trace_fail(parser, "read_hmac");
            goto cleanup_fail;
        }

        uint32_t block_size = 0U;
        if(stream_read(parser->stream, (uint8_t*)&block_size, 4) != 4) {
            FURI_LOG_E("KDBXParser", "Failed to read size for block %llu", block_index);
            kdbx_parser_set_error(parser, "Unable to read the encrypted database payload.");
            kdbx_parser_trace(
                parser, "PAYLOAD_BLOCKS_FAIL cause=read_size block=%llu", block_index);
            kdbx_parser_trace_fail(parser, "read_block_size");
            goto cleanup_fail;
        }
        FURI_LOG_T(
            KDBX_PARSER_TRACE_TAG,
            "block header index=%llu size=%lu",
            (unsigned long long)block_index,
            (unsigned long)block_size);

        if(block_size > KDBX_MAX_STREAM_OUTPUT_SIZE) {
            kdbx_parser_set_error(
                parser, "A database payload block exceeds FlipPass's stream limit.");
            kdbx_parser_trace_fail(parser, "block_too_large");
            goto cleanup_fail;
        }

        const size_t remaining_after_size =
            stream_size(parser->stream) - stream_tell(parser->stream);
        if(block_size > remaining_after_size) {
            kdbx_parser_set_error(parser, "The encrypted database payload is truncated.");
            kdbx_parser_trace_fail(parser, "block_truncated");
            goto cleanup_fail;
        }

        if(!kdbx_parser_verify_block_hmac(
               parser,
               hmac_key,
               hmac_key_size,
               block_index,
               block_size,
               hmac,
               io_buffer,
               KDBX_STREAM_IO_CHUNK_SIZE)) {
            FURI_LOG_E("KDBXParser", "HMAC verification failed for block %llu", block_index);
            kdbx_parser_trace(parser, "PAYLOAD_BLOCKS_FAIL cause=hmac block=%llu", block_index);
            kdbx_parser_set_error(parser, "Wrong password or damaged database payload.");
            kdbx_parser_trace_fail(parser, "block_hmac");
            goto cleanup_fail;
        }
        FURI_LOG_T(
            KDBX_PARSER_TRACE_TAG, "block hmac ok index=%llu", (unsigned long long)block_index);

        if(block_size == 0U) {
            FURI_LOG_T(
                KDBX_PARSER_TRACE_TAG,
                "block zero break index=%llu",
                (unsigned long long)block_index);
            break;
        }

        size_t block_remaining = block_size;
        while(block_remaining > 0U) {
            const size_t chunk_size = (block_remaining > KDBX_STREAM_IO_CHUNK_SIZE) ?
                                          KDBX_STREAM_IO_CHUNK_SIZE :
                                          block_remaining;

            if(stream_read(parser->stream, io_buffer, chunk_size) != chunk_size) {
                kdbx_parser_set_error(parser, "Unable to read the encrypted database payload.");
                kdbx_parser_trace_fail(parser, "read_block_data");
                goto cleanup_fail;
            }

            if(use_aes) {
                if(!kdbx_parser_aes_state_feed(
                       &aes_state,
                       parser,
                       &emitted_bytes,
                       callback,
                       context,
                       buffer_gzip_member ? &gzip_buffer : NULL,
                       io_buffer,
                       chunk_size)) {
                    kdbx_parser_trace_fail(parser, "payload_cipher");
                    goto cleanup_fail;
                }
            } else {
                if(!kdbx_protected_stream_apply(&chacha_stream, io_buffer, chunk_size)) {
                    kdbx_parser_set_error(parser, "Unable to decrypt the database payload.");
                    kdbx_parser_trace_fail(parser, "payload_cipher");
                    goto cleanup_fail;
                }

                if(!kdbx_parser_plain_sink(
                       parser,
                       &emitted_bytes,
                       callback,
                       context,
                       buffer_gzip_member ? &gzip_buffer : NULL,
                       io_buffer,
                       chunk_size)) {
                    kdbx_parser_trace_fail(parser, "payload_sink");
                    goto cleanup_fail;
                }
            }

            block_bytes += chunk_size;
            block_remaining -= chunk_size;
        }

        block_index++;
    }

    FURI_LOG_T(KDBX_PARSER_TRACE_TAG, "payload blocks ok size=%lu", (unsigned long)block_bytes);

    if(use_aes) {
        FURI_LOG_T(
            KDBX_PARSER_TRACE_TAG,
            "cipher finish begin emitted=%lu gzip_size=%lu",
            (unsigned long)emitted_bytes,
            (unsigned long)(gzip_buffer.size));
        if(!kdbx_parser_aes_state_finish(
               &aes_state,
               parser,
               &emitted_bytes,
               callback,
               context,
               buffer_gzip_member ? &gzip_buffer : NULL)) {
            kdbx_parser_trace_fail(parser, "payload_cipher_finish");
            goto cleanup_fail;
        }
        FURI_LOG_T(
            KDBX_PARSER_TRACE_TAG,
            "cipher finish ok emitted=%lu gzip_size=%lu",
            (unsigned long)emitted_bytes,
            (unsigned long)(gzip_buffer.size));
    }

    plain_bytes_before_gzip = buffer_gzip_member ? gzip_buffer.size : emitted_bytes;
    UNUSED(plain_bytes_before_gzip);
    FURI_LOG_T(
        KDBX_PARSER_TRACE_TAG,
        "payload cipher ok size=%lu",
        (unsigned long)plain_bytes_before_gzip);

    if(parser->header.compression_algorithm == KDBX_COMPRESSION_GZIP) {
        if(!inflate_gzip) {
            FURI_LOG_T(
                KDBX_PARSER_TRACE_TAG,
                "payload gzip member ok size=%lu",
                (unsigned long)emitted_bytes);
        } else {
            emitted_bytes = 0U;
            FURI_LOG_T(
                KDBX_PARSER_TRACE_TAG,
                "payload gzip begin compressed=%lu",
                (unsigned long)gzip_buffer.size);
            if(!kdbx_gzip_emit_stream(
                   gzip_buffer.data,
                   gzip_buffer.size,
                   KDBX_MAX_STREAM_OUTPUT_SIZE,
                   callback,
                   context,
                   &gzip_telemetry)) {
                kdbx_parser_set_gzip_error(parser, &gzip_telemetry);
                kdbx_parser_trace_fail(parser, "payload_gzip");
                goto cleanup_fail;
            }
            emitted_bytes = gzip_telemetry.actual_output_size;
            FURI_LOG_T(
                KDBX_PARSER_TRACE_TAG,
                "payload gzip return status=%u actual=%lu consumed=%lu",
                (unsigned)gzip_telemetry.status,
                (unsigned long)gzip_telemetry.actual_output_size,
                (unsigned long)gzip_telemetry.consumed_input_size);
            FURI_LOG_T(
                KDBX_PARSER_TRACE_TAG, "payload gzip ok size=%lu", (unsigned long)emitted_bytes);
        }
    } else {
        FURI_LOG_T(KDBX_PARSER_TRACE_TAG, "payload raw ok size=%lu", (unsigned long)emitted_bytes);
    }
    FURI_LOG_T(
        KDBX_PARSER_TRACE_TAG, "payload stream done size=%lu", (unsigned long)emitted_bytes);

    memzero(io_buffer, KDBX_STREAM_IO_CHUNK_SIZE);
    free(io_buffer);
    kdbx_parser_heap_buffer_free(&gzip_buffer);
    kdbx_protected_stream_reset(&chacha_stream);
    return true;

cleanup_fail:
    if(io_buffer != NULL) {
        memzero(io_buffer, KDBX_STREAM_IO_CHUNK_SIZE);
        free(io_buffer);
    }
    kdbx_parser_heap_buffer_free(&gzip_buffer);
    kdbx_protected_stream_reset(&chacha_stream);
    memzero(&aes_state, sizeof(aes_state));
    return false;
}

bool kdbx_parser_stream_payload(
    KDBXParser* parser,
    const uint8_t* cipher_key,
    size_t cipher_key_size,
    const uint8_t* hmac_key,
    size_t hmac_key_size,
    KDBXParserOutputCallback callback,
    void* context) {
    return kdbx_parser_stream_payload_internal(
        parser, cipher_key, cipher_key_size, hmac_key, hmac_key_size, callback, context, true);
}

bool kdbx_parser_stream_outer_payload(
    KDBXParser* parser,
    const uint8_t* cipher_key,
    size_t cipher_key_size,
    const uint8_t* hmac_key,
    size_t hmac_key_size,
    KDBXParserOutputCallback callback,
    void* context) {
    return kdbx_parser_stream_payload_internal(
        parser, cipher_key, cipher_key_size, hmac_key, hmac_key_size, callback, context, false);
}

uint8_t* kdbx_parser_decrypt_payload(
    KDBXParser* parser,
    const uint8_t* cipher_key,
    size_t cipher_key_size,
    const uint8_t* hmac_key,
    size_t hmac_key_size,
    size_t* payload_size) {
    furi_assert(payload_size);

    KDBXParserCollectState collect = {0};
    if(!kdbx_parser_stream_payload(
           parser,
           cipher_key,
           cipher_key_size,
           hmac_key,
           hmac_key_size,
           kdbx_parser_collect_callback,
           &collect)) {
        if(collect.data != NULL) {
            memzero(collect.data, collect.capacity);
            free(collect.data);
        }
        *payload_size = 0U;
        return NULL;
    }

    if(collect.failed) {
        if(collect.data != NULL) {
            memzero(collect.data, collect.capacity);
            free(collect.data);
        }
        kdbx_parser_set_error(parser, "Not enough RAM is available to collect the payload.");
        *payload_size = 0U;
        return NULL;
    }

    *payload_size = collect.size;
    return collect.data;
}

static inline uint32_t read_uint32_le(const uint8_t* p) {
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static inline uint64_t read_uint64_le(const uint8_t* p) {
    return ((uint64_t)read_uint32_le(p)) | ((uint64_t)read_uint32_le(p + 4) << 32);
}

static bool
    kdbx_parser_variant_name_equals(const char* name, uint32_t name_size, const char* expected) {
    const size_t expected_len = strlen(expected);
    return expected_len == name_size && memcmp(name, expected, name_size) == 0;
}

static bool kdbx_parser_variant_read_uint(
    uint8_t value_type,
    const uint8_t* value,
    uint32_t value_size,
    uint64_t* out_value) {
    if(value == NULL || out_value == NULL) {
        return false;
    }

    if(value_type == 0x04U && value_size == 4U) {
        *out_value = read_uint32_le(value);
        return true;
    }

    if(value_type == 0x05U && value_size == 8U) {
        *out_value = read_uint64_le(value);
        return true;
    }

    return false;
}

static bool kdbx_parser_has_bytes(const uint8_t* p, const uint8_t* end, size_t needed) {
    return (p <= end) && ((size_t)(end - p) >= needed);
}

// AES-KDF UUID: C9D9F39A628A4460BF740D08C18A4FEA
static const uint8_t KDBX_KDF_UUID_AES[16] =
    {0xC9, 0xD9, 0xF3, 0x9A, 0x62, 0x8A, 0x44, 0x60, 0xBF, 0x74, 0x0D, 0x08, 0xC1, 0x8A, 0x4F, 0xEA};
// Argon2d UUID: EF636DDF8C29444B91F7A9A403E30A0C
static const uint8_t KDBX_KDF_UUID_ARGON2D[16] =
    {0xEF, 0x63, 0x6D, 0xDF, 0x8C, 0x29, 0x44, 0x4B, 0x91, 0xF7, 0xA9, 0xA4, 0x03, 0xE3, 0x0A, 0x0C};
// Argon2id UUID: 9E298B1956DB4773B23DFC3EC6F0A1E6
static const uint8_t KDBX_KDF_UUID_ARGON2ID[16] =
    {0x9E, 0x29, 0x8B, 0x19, 0x56, 0xDB, 0x47, 0x73, 0xB2, 0x3D, 0xFC, 0x3E, 0xC6, 0xF0, 0xA1, 0xE6};

static bool
    kdbx_parser_parse_kdf_parameters(const KDBXParser* parser, KDBXParserKdfParameters* params) {
    const uint8_t* p = NULL;
    const uint8_t* end = NULL;
    uint16_t version = 0U;
    uint8_t kdf_uuid[16] = {0};

    furi_assert(parser);
    furi_assert(params);

    memset(params, 0, sizeof(*params));

    if(!parser->header.kdf_parameters || parser->header.kdf_parameters_size < 2U) {
        kdbx_parser_set_error((KDBXParser*)parser, "Missing KDF parameters.");
        return false;
    }

    p = parser->header.kdf_parameters;
    end = p + parser->header.kdf_parameters_size;
    if(!kdbx_parser_has_bytes(p, end, 2U)) {
        kdbx_parser_set_error((KDBXParser*)parser, "The KDF parameter block is truncated.");
        return false;
    }

    version = (uint16_t)((p[1] << 8) | p[0]);
    p += 2;
    if(version != 0x0100U) {
        kdbx_parser_set_error(
            (KDBXParser*)parser,
            "This database uses an unsupported KDF dictionary version (%04X).",
            version);
        return false;
    }

    while(p < end && *p != 0x00U) {
        uint8_t value_type = 0U;
        uint32_t name_size = 0U;
        uint32_t value_size = 0U;
        const char* name = NULL;
        const uint8_t* value = NULL;
        uint64_t parsed_uint = 0U;

        if(!kdbx_parser_has_bytes(p, end, 1U + 4U)) {
            kdbx_parser_set_error((KDBXParser*)parser, "A KDF parameter header is truncated.");
            return false;
        }

        value_type = *p++;
        name_size = read_uint32_le(p);
        p += 4;
        if(!kdbx_parser_has_bytes(p, end, name_size + 4U)) {
            kdbx_parser_set_error((KDBXParser*)parser, "A KDF parameter name is truncated.");
            return false;
        }

        name = (const char*)p;
        p += name_size;
        value_size = read_uint32_le(p);
        p += 4;
        if(!kdbx_parser_has_bytes(p, end, value_size)) {
            kdbx_parser_set_error((KDBXParser*)parser, "A KDF parameter value is truncated.");
            return false;
        }

        value = p;
        p += value_size;

        if(kdbx_parser_variant_name_equals(name, name_size, "$UUID")) {
            if(value_type != 0x42U || value_size != sizeof(kdf_uuid)) {
                kdbx_parser_set_error((KDBXParser*)parser, "The KDF UUID parameter is invalid.");
                return false;
            }
            memcpy(kdf_uuid, value, sizeof(kdf_uuid));
            continue;
        }

        if(kdbx_parser_variant_name_equals(name, name_size, "S")) {
            if(value_type != 0x42U || value_size == 0U) {
                kdbx_parser_set_error((KDBXParser*)parser, "The KDF salt parameter is invalid.");
                return false;
            }
            params->salt = value;
            params->salt_size = value_size;
            continue;
        }

        if(kdbx_parser_variant_name_equals(name, name_size, "R")) {
            if(!kdbx_parser_variant_read_uint(value_type, value, value_size, &parsed_uint)) {
                kdbx_parser_set_error(
                    (KDBXParser*)parser, "The AES-KDF rounds parameter is invalid.");
                return false;
            }
            params->rounds = parsed_uint;
            continue;
        }

        (void)value_type;
        (void)value;
        (void)value_size;
        (void)parsed_uint;
    }

    if(memcmp(kdf_uuid, KDBX_KDF_UUID_AES, sizeof(kdf_uuid)) == 0) {
        params->type = KDBXParserKdfTypeAes;
    } else if(memcmp(kdf_uuid, KDBX_KDF_UUID_ARGON2D, sizeof(kdf_uuid)) == 0) {
        params->type = KDBXParserKdfTypeArgon2Unsupported;
    } else if(memcmp(kdf_uuid, KDBX_KDF_UUID_ARGON2ID, sizeof(kdf_uuid)) == 0) {
        params->type = KDBXParserKdfTypeArgon2Unsupported;
    } else {
        kdbx_parser_set_error((KDBXParser*)parser, "This database uses an unsupported KDF.");
        return false;
    }

    if(params->type == KDBXParserKdfTypeArgon2Unsupported) {
        kdbx_parser_set_error((KDBXParser*)parser, KDBX_ARGON2_UNSUPPORTED_MESSAGE);
        return false;
    }

    if(params->salt == NULL) {
        kdbx_parser_set_error((KDBXParser*)parser, "The KDF salt parameter is missing.");
        return false;
    }

    if(params->salt_size != 32U || params->rounds == 0U) {
        kdbx_parser_set_error(
            (KDBXParser*)parser, "The AES-KDF parameters are incomplete or invalid.");
        return false;
    }

    return true;
}

static bool kdbx_parser_derive_transformed_key_aes(
    const KDBXParser* parser,
    const char* password,
    const KDBXParserKdfParameters* params,
    uint8_t transformed_key[32]) {
    uint8_t password_hash[32];
    uint8_t composite_key[32];
    aes_encrypt_ctx aes_ctx;

    furi_assert(parser);
    furi_assert(password);
    furi_assert(params);
    furi_assert(transformed_key);

    sha256_Raw((const uint8_t*)password, strlen(password), password_hash);
    sha256_Raw(password_hash, sizeof(password_hash), composite_key);

    aes_encrypt_key256(params->salt, &aes_ctx);

    if(parser->kdf_progress_callback != NULL) {
        parser->kdf_progress_callback(0U, params->rounds, parser->kdf_progress_context);
    }

    uint64_t progress_step = params->rounds / 48U;
    if(progress_step == 0U) {
        progress_step = 1U;
    }

    uint64_t next_progress = progress_step;
    for(uint64_t i = 0; i < params->rounds; ++i) {
        aes_encrypt(composite_key, composite_key, &aes_ctx);
        aes_encrypt(composite_key + 16, composite_key + 16, &aes_ctx);
        if(parser->kdf_progress_callback != NULL &&
           ((i + 1U) >= next_progress || (i + 1U) == params->rounds)) {
            parser->kdf_progress_callback(i + 1U, params->rounds, parser->kdf_progress_context);
            next_progress += progress_step;
        }
    }

    sha256_Raw(composite_key, sizeof(composite_key), transformed_key);
    memzero(password_hash, sizeof(password_hash));
    memzero(composite_key, sizeof(composite_key));
    memzero(&aes_ctx, sizeof(aes_ctx));
    return true;
}

static bool kdbx_parser_derive_transformed_key(
    const KDBXParser* parser,
    const char* password,
    uint8_t transformed_key[32]) {
    KDBXParserKdfParameters params;

    furi_assert(parser);
    furi_assert(password);
    furi_assert(transformed_key);

    if(!parser->header_parsed) {
        FURI_LOG_E("KDBXParser", "Header not parsed, cannot derive key");
        kdbx_parser_set_error(
            (KDBXParser*)parser, "The database header is not ready for key derivation.");
        return false;
    }

    if(!kdbx_parser_parse_kdf_parameters(parser, &params)) {
        return false;
    }

    switch(params.type) {
    case KDBXParserKdfTypeAes:
        return kdbx_parser_derive_transformed_key_aes(parser, password, &params, transformed_key);
    case KDBXParserKdfTypeArgon2Unsupported:
        kdbx_parser_set_error((KDBXParser*)parser, KDBX_ARGON2_UNSUPPORTED_MESSAGE);
        return false;
    case KDBXParserKdfTypeInvalid:
    default:
        kdbx_parser_set_error((KDBXParser*)parser, "This database uses an unsupported KDF.");
        return false;
    }
}

bool kdbx_parser_derive_key(
    const KDBXParser* parser,
    const char* password,
    uint8_t* cipher_key,
    size_t cipher_key_size,
    uint8_t* hmac_key,
    size_t hmac_key_size) {
    furi_assert(parser);
    furi_assert(password);
    furi_assert(cipher_key);
    furi_assert(hmac_key);

    if(cipher_key_size != 32 || hmac_key_size != 64) {
        FURI_LOG_E("KDBXParser", "Invalid requested cipher or HMAC key size");
        kdbx_parser_set_error(
            (KDBXParser*)parser, "FlipPass received invalid derived key buffer sizes.");
        return false;
    }

    uint8_t transformed_key[32];
    uint8_t key_material[65];

    if(!kdbx_parser_derive_transformed_key(parser, password, transformed_key)) {
        memzero(transformed_key, sizeof(transformed_key));
        return false;
    }

    memcpy(key_material, parser->header.master_seed, 32);
    memcpy(key_material + 32, transformed_key, 32);
    sha256_Raw(key_material, 64, cipher_key);

    key_material[64] = 1;
    sha512_Raw(key_material, sizeof(key_material), hmac_key);
    kdbx_parser_release_kdf_parameters((KDBXParser*)parser);

    memzero(key_material, sizeof(key_material));
    memzero(transformed_key, sizeof(transformed_key));
    return true;
}
