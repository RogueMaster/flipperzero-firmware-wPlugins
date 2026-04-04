#include "flippass_db.h"

#include "flippass.h"
#include "kdbx/kdbx_gzip.h"
#include "kdbx/memzero.h"
#include "kdbx/xml_parser.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLIPPASS_DB_SAFETY_RESERVE_BYTES      (8U * 1024U)
#define FLIPPASS_DB_FILE_MODEL_SAFETY_RESERVE_BYTES (4U * 1024U)
#define FLIPPASS_DB_MODEL_GROWTH_RESERVE_BYTES (4U * 1024U)
#define FLIPPASS_DB_ARENA_CHUNK_SIZE          256U
#define FLIPPASS_DB_MAX_XML_STREAM_BYTES (2U * 1024U * 1024U)
#define FLIPPASS_DB_MAX_FIELD_PLAIN_BYTES (256U * 1024U)
#define FLIPPASS_DB_MAX_XML_DEPTH        64U
#define FLIPPASS_DB_GZIP_DICT_RESERVE_BYTES (32U * 1024U)
#define FLIPPASS_DB_GZIP_MEMBER_PREFIX_BYTES 512U
#define FLIPPASS_DB_GZIP_MEMBER_RAM_LIMIT (16U * 1024U)
#define FLIPPASS_DB_GZIP_MEMBER_SAMPLE_COUNT 8U
#define FLIPPASS_DB_GZIP_TRACE_EVENT_LIMIT 20U
#define FLIPPASS_DB_GZIP_TRACE_TEXT_LIMIT  224U

#if FLIPPASS_ENABLE_VERBOSE_UNLOCK_LOG
#define FLIPPASS_VERBOSE_LOG(app, ...) flippass_log_event(app, __VA_ARGS__)
#else
#define FLIPPASS_VERBOSE_LOG(app, ...) \
    do {                               \
        UNUSED(app);                   \
    } while(0)
#endif

#define FLIPPASS_DEBUG_EVENT(app, ...) FLIPPASS_VERBOSE_LOG(app, __VA_ARGS__)

#if FLIPPASS_ENABLE_VERBOSE_UNLOCK_LOG
#define FLIPPASS_DB_DEBUG_LOG_MEM(ctx, stage) flippass_db_log_mem_snapshot((ctx), (stage))
#define FLIPPASS_DB_DEBUG_LOG_RAM(ctx, stage, field_name, request_size) \
    flippass_db_log_ram_failure((ctx), (stage), (field_name), (request_size))
#define FLIPPASS_DB_DEBUG_LOG_CHECKPOINT(ctx, stage) \
    flippass_db_log_checkpoint_snapshot((ctx), (stage))
#else
#define FLIPPASS_DB_DEBUG_LOG_MEM(ctx, stage) \
    do {                                      \
        UNUSED(ctx);                          \
        UNUSED(stage);                        \
    } while(0)
#define FLIPPASS_DB_DEBUG_LOG_RAM(ctx, stage, field_name, request_size) \
    do {                                                                 \
        UNUSED(ctx);                                                     \
        UNUSED(stage);                                                   \
        UNUSED(field_name);                                              \
        UNUSED(request_size);                                            \
    } while(0)
#define FLIPPASS_DB_DEBUG_LOG_CHECKPOINT(ctx, stage) \
    do {                                             \
        UNUSED(ctx);                                 \
        UNUSED(stage);                               \
    } while(0)
#endif

typedef enum {
    FlipPassDbTextStateNone = 0,
    FlipPassDbTextStateGroupName,
    FlipPassDbTextStateGroupUuid,
    FlipPassDbTextStateEntryUuid,
    FlipPassDbTextStateStringKey,
    FlipPassDbTextStateStringValue,
    FlipPassDbTextStateAutoTypeSequence,
} FlipPassDbTextState;

typedef struct {
    uint8_t* data;
    size_t size;
    size_t capacity;
} FlipPassDbByteBuffer;

typedef struct {
    App* app;
    FuriString* error;
    KDBXArena* arena;
    KDBXVault* vault;
    KDBXVaultWriter field_writer;
    XmlParser* xml_parser;
    KDBXProtectedStream protected_stream;
    KDBXGroup* root_group;
    KDBXGroup* current_group;
    KDBXEntry* current_entry;
    FuriString* text_value;
    FuriString* string_key;
    FlipPassDbTextState text_state;
    bool parse_failed;
    bool inner_header_done;
    bool in_group;
    bool in_entry;
    bool in_string;
    bool in_autotype;
    bool skipping_history;
    bool history_in_string;
    bool history_collect_protected_value;
    bool history_value_protected;
    bool value_protected;
    bool protected_discard_active;
    bool deferred_stream_active;
    bool deferred_stream_logged_large;
    int parsing_depth;
    size_t history_skip_depth;
    size_t xml_bytes;
    size_t xml_total_bytes_hint;
    size_t deferred_stream_plain_bytes;
    size_t committed_bytes;
    size_t commit_limit;
    size_t group_count;
    size_t entry_count;
    size_t deferred_field_count;
    size_t deferred_plain_bytes;
    size_t next_entry_checkpoint;
    size_t next_record_checkpoint;
    uint8_t inner_header_prefix[5];
    uint8_t field_writer_pending[KDBX_VAULT_RECORD_PLAIN_MAX];
    size_t inner_header_prefix_len;
    uint8_t inner_field_id;
    uint32_t inner_field_size;
    size_t inner_field_remaining;
    uint32_t protected_stream_id;
    FlipPassDbByteBuffer protected_stream_key;
    FlipPassDbByteBuffer protected_value_buffer;
    KDBXProtectedDiscardState protected_discard_state;
    bool vault_promotion_attempted;
    char parse_error[STATUS_MESSAGE_SIZE];
} FlipPassDbLoadContext;

typedef struct {
    App* app;
    KDBXVaultWriter writer;
    KDBXVaultBackend backend;
    const char* path;
    KDBXVault** vault_slot;
    struct FlipPassDbGzipTraceContext* trace;
    bool failed;
    bool alloc_failed;
    bool storage_failed;
    size_t chunk_count;
    size_t plain_bytes;
    size_t checkpoint_count;
    size_t checkpoint_chunk_index;
    size_t checkpoint_chunk_size;
    size_t checkpoint_plain_bytes;
    size_t checkpoint_record_count;
    size_t checkpoint_free_heap;
    size_t checkpoint_max_free_block;
} FlipPassDbScratchWriteContext;

typedef struct FlipPassDbGzipTraceContext {
    App* app;
#if FLIPPASS_ENABLE_GZIP_PAGED_TRACE
    size_t event_count;
    size_t stored_count;
    size_t dropped_count;
    size_t next_index;
    bool buffer_only;
    char events[FLIPPASS_DB_GZIP_TRACE_EVENT_LIMIT][FLIPPASS_DB_GZIP_TRACE_TEXT_LIMIT];
#endif
} FlipPassDbGzipTraceContext;

typedef struct {
    App* app;
    KDBXVaultWriter writer;
    FlipPassDbByteBuffer ram_buffer;
    KDBXVaultBackend spill_backend;
    const char* spill_path;
    KDBXVault* spill_vault;
    bool failed;
    size_t chunk_count;
    size_t total_bytes;
    size_t prefix_len;
    size_t trailer_len;
    size_t sample_count;
    size_t spill_started_at_bytes;
    size_t failure_chunk_index;
    size_t failure_chunk_size;
    size_t failure_free_heap;
    size_t failure_max_free_block;
    bool spill_started;
    uint8_t prefix[FLIPPASS_DB_GZIP_MEMBER_PREFIX_BYTES];
    uint8_t trailer[8];
    uint16_t sample_sizes[FLIPPASS_DB_GZIP_MEMBER_SAMPLE_COUNT];
} FlipPassDbMemberCollectContext;

typedef struct {
    FlipPassDbScratchWriteContext scratch;
    FlipPassDbGzipTraceContext trace;
    FlipPassDbMemberCollectContext member;
    KDBXGzipTelemetry telemetry;
    KDBXGzipTraceConfig trace_config;
    KDBXGzipMemberInfo member_info;
    KDBXFieldRef member_ref;
    void* inflate_workspace;
} FlipPassDbGzipStageState;

static const char* flippass_db_text_state_label(FlipPassDbTextState state) {
    switch(state) {
    case FlipPassDbTextStateNone:
        return "none";
    case FlipPassDbTextStateGroupName:
        return "group_name";
    case FlipPassDbTextStateGroupUuid:
        return "group_uuid";
    case FlipPassDbTextStateEntryUuid:
        return "entry_uuid";
    case FlipPassDbTextStateStringKey:
        return "string_key";
    case FlipPassDbTextStateStringValue:
        return "string_value";
    case FlipPassDbTextStateAutoTypeSequence:
        return "autotype_sequence";
    default:
        return "unknown";
    }
}

typedef enum {
    FlipPassDbPreflightTextStateNone = 0,
    FlipPassDbPreflightTextStateGroupName,
    FlipPassDbPreflightTextStateEntryUuid,
    FlipPassDbPreflightTextStateStringKey,
    FlipPassDbPreflightTextStateStringValue,
    FlipPassDbPreflightTextStateAutoTypeSequence,
} FlipPassDbPreflightTextState;

typedef struct {
    size_t total_bytes;
    size_t current_chunk_payload;
    size_t current_chunk_used;
    size_t chunk_count;
} FlipPassDbArenaEstimate;

typedef struct {
    size_t total_bytes;
    size_t current_page_payload;
    size_t current_page_used;
    uint32_t record_count;
    size_t plain_bytes;
} FlipPassDbVaultRamEstimate;

typedef struct {
    uint32_t record_count;
    size_t plain_bytes;
} FlipPassDbVaultFileEstimate;

typedef struct {
    App* app;
    XmlParser* xml_parser;
    FuriString* text_value;
    FuriString* string_key;
    FlipPassDbPreflightTextState text_state;
    bool parse_failed;
    bool inner_header_done;
    bool in_group;
    bool in_entry;
    bool in_string;
    bool in_autotype;
    bool skipping_history;
    size_t group_depth;
    size_t history_skip_depth;
    size_t xml_bytes;
    uint8_t inner_header_prefix[5];
    size_t inner_header_prefix_len;
    uint8_t inner_field_id;
    uint32_t inner_field_size;
    size_t inner_field_remaining;
    size_t group_count;
    size_t entry_count;
    size_t custom_field_count;
    FlipPassDbArenaEstimate ram_arena;
    FlipPassDbArenaEstimate file_arena;
    FlipPassDbVaultRamEstimate ram_vault;
    FlipPassDbVaultFileEstimate file_vault;
    char parse_error[STATUS_MESSAGE_SIZE];
} FlipPassDbPreflightContext;

typedef struct {
    size_t group_count;
    size_t entry_count;
    size_t custom_field_count;
    size_t ram_arena_bytes;
    size_t file_arena_bytes;
    size_t ram_vault_page_bytes;
    size_t ram_vault_index_bytes;
    size_t file_vault_index_bytes;
    size_t ram_vault_plain_bytes;
    size_t file_vault_plain_bytes;
    uint32_t ram_record_count;
    uint32_t file_record_count;
    size_t ram_total_bytes;
    size_t file_total_bytes;
    size_t free_heap;
    size_t max_free_block;
    size_t ram_budget;
    size_t file_budget;
    bool prefer_file;
} FlipPassDbPreflightSummary;

static const char* flippass_db_field_log_name(uint32_t field_mask);
static bool flippass_db_is_supported_string_key(const char* key);
static KDBXVaultBackend flippass_db_select_gzip_scratch_backend(KDBXVaultBackend preferred_backend);
static const char* flippass_db_gzip_scratch_path(KDBXVaultBackend backend);
static const char* flippass_db_gzip_member_path(KDBXVaultBackend backend);
static void flippass_db_set_gzip_stage_error(FuriString* error, const KDBXGzipTelemetry* telemetry);
static void flippass_db_gzip_member_log_summary(
    App* app,
    const char* label,
    const FlipPassDbMemberCollectContext* collect);
static void flippass_db_progress_update(App* app, const char* stage, const char* detail, uint8_t percent);
static void flippass_db_kdf_progress_callback(uint64_t current_round, uint64_t total_rounds, void* context);
static void flippass_db_gzip_progress_callback(
    const char* event,
    const KDBXGzipTelemetry* telemetry,
    void* context);
static void flippass_db_gzip_trace_store(
    FlipPassDbGzipTraceContext* trace,
    const char* format,
    ...);
static bool flippass_db_gzip_scratch_ensure_writer(FlipPassDbScratchWriteContext* scratch);
static size_t flippass_db_active_safety_reserve(const FlipPassDbLoadContext* ctx);
static void flippass_db_refresh_commit_budget(FlipPassDbLoadContext* ctx);
static bool flippass_db_should_preemptively_promote(
    const FlipPassDbLoadContext* ctx,
    size_t next_plain_len);
static bool flippass_db_should_stream_string_value(const FlipPassDbLoadContext* ctx);
static bool flippass_db_begin_streamed_value(FlipPassDbLoadContext* ctx, const char* field_name);
static bool flippass_db_write_streamed_value_chunk(
    FlipPassDbLoadContext* ctx,
    const char* field_name,
    const char* data,
    size_t data_len);
static bool flippass_db_write_streamed_protected_chunk(
    const uint8_t* data,
    size_t data_len,
    void* context);
static bool flippass_db_commit_streamed_value(
    FlipPassDbLoadContext* ctx,
    KDBXEntry* entry,
    const char* field_name);
static bool flippass_db_prepare_for_arena_alloc(
    FlipPassDbLoadContext* ctx,
    const char* stage,
    size_t request_size);
static void flippass_db_set_error(FlipPassDbLoadContext* ctx, const char* format, ...);
#if FLIPPASS_ENABLE_VERBOSE_UNLOCK_LOG
static void flippass_db_log_mem_snapshot(FlipPassDbLoadContext* ctx, const char* stage);
static void flippass_db_log_checkpoint_snapshot(FlipPassDbLoadContext* ctx, const char* stage);
static void flippass_db_log_ram_failure(
    FlipPassDbLoadContext* ctx,
    const char* stage,
    const char* field_name,
    size_t request_size);
#endif
static void flippass_db_prepare_fallback_message(
    FlipPassDbLoadContext* ctx,
    const char* stage,
    size_t request_size);
static bool flippass_db_promote_vault_to_ext(FlipPassDbLoadContext* ctx);
static bool flippass_db_write_deferred_value(
    FlipPassDbLoadContext* ctx,
    const char* field_name,
    const char* value,
    size_t value_len,
    KDBXFieldRef* out_ref);
static FlipPassDbLoadContext* flippass_db_load_context_alloc(App* app, FuriString* error);
static void flippass_db_load_context_free(FlipPassDbLoadContext* ctx);
static FlipPassDbGzipStageState* flippass_db_gzip_stage_state_alloc(void);
static void flippass_db_gzip_stage_state_free(FlipPassDbGzipStageState* state);
static size_t flippass_db_align_up_size(size_t value, size_t alignment);
static bool flippass_db_arena_estimate_alloc(
    FlipPassDbArenaEstimate* estimate,
    size_t chunk_size,
    size_t size,
    size_t alignment);
static bool flippass_db_vault_ram_estimate_add_plain(
    FlipPassDbVaultRamEstimate* estimate,
    size_t plain_len);
static bool flippass_db_vault_file_estimate_add_plain(
    FlipPassDbVaultFileEstimate* estimate,
    size_t plain_len);
static bool flippass_db_preflight_append_text_segment(
    FlipPassDbPreflightContext* ctx,
    const char* data,
    int len);
static void flippass_db_preflight_begin_text(
    FlipPassDbPreflightContext* ctx,
    FlipPassDbPreflightTextState state);
static void flippass_db_preflight_set_error(
    FlipPassDbPreflightContext* ctx,
    const char* format,
    ...);
static bool flippass_db_preflight_commit_entry_value(
    FlipPassDbPreflightContext* ctx,
    const char* key,
    size_t value_len);
static bool flippass_db_preflight_commit_text_value(
    FlipPassDbPreflightContext* ctx,
    FlipPassDbPreflightTextState state,
    const char* value,
    size_t value_len);
static bool flippass_db_preflight_consume_inner_header(
    FlipPassDbPreflightContext* ctx,
    const uint8_t* data,
    size_t data_size,
    size_t* consumed);
static void flippass_db_preflight_start_element(
    void* context,
    const char* name,
    const char** attributes);
static void flippass_db_preflight_end_element(void* context, const char* name);
static void flippass_db_preflight_character_data(void* context, const char* data, int len);
static bool flippass_db_preflight_payload_chunk_callback(
    const uint8_t* data,
    size_t data_size,
    void* context);
static FlipPassDbPreflightContext* flippass_db_preflight_context_alloc(App* app);
static void flippass_db_preflight_context_free(FlipPassDbPreflightContext* ctx);
static bool flippass_db_preflight_finalize(
    FlipPassDbPreflightContext* ctx,
    FlipPassDbPreflightSummary* out_summary);
static bool flippass_db_run_preflight_from_vault(
    App* app,
    KDBXVault* vault,
    const KDBXFieldRef* ref,
    FlipPassDbPreflightSummary* out_summary,
    FuriString* error);
static bool flippass_db_try_fast_preflight_summary(
    size_t plain_xml_bytes,
    FlipPassDbPreflightSummary* out_summary);
static bool flippass_db_run_preflight_from_payload(
    App* app,
    const uint8_t* cipher_key,
    size_t cipher_key_size,
    const uint8_t* hmac_key,
    size_t hmac_key_size,
    FlipPassDbPreflightSummary* out_summary,
    FuriString* error);
static void flippass_db_log_preflight_summary(
    App* app,
    const FlipPassDbPreflightSummary* summary,
    const char* source);
static bool flippass_db_apply_preflight_decision(
    App* app,
    const FlipPassDbPreflightSummary* summary,
    KDBXVaultBackend* backend,
    FuriString* error);

static bool flippass_db_byte_buffer_reserve(FlipPassDbByteBuffer* buffer, size_t capacity) {
    uint8_t* next = NULL;

    furi_assert(buffer);

    if(capacity <= buffer->capacity) {
        return true;
    }

    size_t next_capacity = buffer->capacity > 0U ? buffer->capacity : 512U;
    while(next_capacity < capacity) {
        if(next_capacity > (SIZE_MAX / 2U)) {
            next_capacity = capacity;
            break;
        }
        next_capacity *= 2U;
    }
    if(next_capacity < capacity) {
        return false;
    }

    next = malloc(next_capacity);
    if(next == NULL) {
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
    buffer->capacity = next_capacity;
    return true;
}

static size_t flippass_db_byte_buffer_quantized_capacity(size_t capacity, size_t quantum) {
    if(quantum == 0U) {
        return capacity;
    }

    const size_t remainder = capacity % quantum;
    if(remainder == 0U) {
        return capacity;
    }

    if(capacity > (SIZE_MAX - (quantum - remainder))) {
        return 0U;
    }

    return capacity + (quantum - remainder);
}

static bool flippass_db_byte_buffer_reserve_exact(FlipPassDbByteBuffer* buffer, size_t capacity) {
    uint8_t* next = NULL;

    furi_assert(buffer);

    if(capacity <= buffer->capacity) {
        return true;
    }

    next = malloc(capacity);
    if(next == NULL) {
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
    buffer->capacity = capacity;
    return true;
}

static bool flippass_db_byte_buffer_append_quantized(
    FlipPassDbByteBuffer* buffer,
    const uint8_t* data,
    size_t data_size,
    size_t quantum) {
    furi_assert(buffer);

    if(data_size == 0U) {
        return true;
    }

    if(data == NULL || data_size > (SIZE_MAX - buffer->size)) {
        return false;
    }

    const size_t required = buffer->size + data_size;
    const size_t capacity = flippass_db_byte_buffer_quantized_capacity(required, quantum);
    if(capacity == 0U || !flippass_db_byte_buffer_reserve_exact(buffer, capacity)) {
        return false;
    }

    memcpy(buffer->data + buffer->size, data, data_size);
    buffer->size += data_size;
    return true;
}

static bool flippass_db_byte_buffer_append(
    FlipPassDbByteBuffer* buffer,
    const uint8_t* data,
    size_t data_size) {
    furi_assert(buffer);

    if(data_size == 0U) {
        return true;
    }

    if(data == NULL || data_size > (SIZE_MAX - buffer->size)) {
        return false;
    }

    if(!flippass_db_byte_buffer_reserve(buffer, buffer->size + data_size)) {
        return false;
    }

    memcpy(buffer->data + buffer->size, data, data_size);
    buffer->size += data_size;
    return true;
}

static void flippass_db_byte_buffer_free(FlipPassDbByteBuffer* buffer) {
    if(buffer == NULL) {
        return;
    }

    if(buffer->data != NULL) {
        memzero(buffer->data, buffer->capacity);
        free(buffer->data);
    }

    memset(buffer, 0, sizeof(*buffer));
}

static FlipPassDbLoadContext* flippass_db_load_context_alloc(App* app, FuriString* error) {
    FlipPassDbLoadContext* ctx = malloc(sizeof(*ctx));
    if(ctx == NULL) {
        return NULL;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->app = app;
    ctx->error = error;
    return ctx;
}

static void flippass_db_load_context_free(FlipPassDbLoadContext* ctx) {
    if(ctx == NULL) {
        return;
    }

    memzero(ctx, sizeof(*ctx));
    free(ctx);
}

static size_t flippass_db_align_up_size(size_t value, size_t alignment) {
    if(alignment <= 1U) {
        return value;
    }

    const size_t mask = alignment - 1U;
    return (value + mask) & ~mask;
}

static bool flippass_db_arena_estimate_alloc(
    FlipPassDbArenaEstimate* estimate,
    size_t chunk_size,
    size_t size,
    size_t alignment) {
    furi_assert(estimate);

    if(size == 0U) {
        return true;
    }

    if(chunk_size < 256U) {
        chunk_size = 256U;
    }
    if(alignment < sizeof(void*)) {
        alignment = sizeof(void*);
    }
    if((alignment & (alignment - 1U)) != 0U) {
        return false;
    }

    size_t offset = 0U;
    if(estimate->current_chunk_payload > 0U) {
        offset = flippass_db_align_up_size(estimate->current_chunk_used, alignment);
    }

    if(estimate->current_chunk_payload == 0U || offset > estimate->current_chunk_payload ||
       size > (estimate->current_chunk_payload - offset)) {
        size_t payload = size + alignment;
        if(payload < chunk_size) {
            payload = chunk_size;
        }
        if(payload < size) {
            return false;
        }

        const size_t chunk_bytes = kdbx_arena_chunk_overhead_bytes() + payload;
        if(chunk_bytes < payload || estimate->total_bytes > (SIZE_MAX - chunk_bytes)) {
            return false;
        }

        estimate->total_bytes += chunk_bytes;
        estimate->current_chunk_payload = payload;
        estimate->current_chunk_used = 0U;
        estimate->chunk_count++;
        offset = flippass_db_align_up_size(estimate->current_chunk_used, alignment);
    }

    if(offset > estimate->current_chunk_payload || size > (estimate->current_chunk_payload - offset)) {
        return false;
    }

    estimate->current_chunk_used = offset + size;
    return true;
}

static bool flippass_db_vault_ram_estimate_add_plain(
    FlipPassDbVaultRamEstimate* estimate,
    size_t plain_len) {
    const size_t record_overhead = kdbx_vault_record_overhead_bytes();
    const size_t page_payload_size = kdbx_vault_ram_page_payload_size();
    const size_t page_overhead = kdbx_vault_ram_page_overhead_bytes();
    size_t remaining = plain_len;

    furi_assert(estimate);

    if(plain_len == 0U) {
        return true;
    }

    while(remaining > 0U) {
        const size_t chunk =
            (remaining > KDBX_VAULT_RECORD_PLAIN_MAX) ? KDBX_VAULT_RECORD_PLAIN_MAX : remaining;
        const size_t record_size = record_overhead + chunk;
        size_t offset = 0U;

        if(estimate->record_count == UINT32_MAX) {
            return false;
        }

        if(estimate->current_page_payload > 0U) {
            offset = flippass_db_align_up_size(estimate->current_page_used, sizeof(uint32_t));
        }

        if(estimate->current_page_payload == 0U || offset > estimate->current_page_payload ||
           record_size > (estimate->current_page_payload - offset)) {
            size_t payload = record_size + sizeof(uint32_t);
            if(payload < page_payload_size) {
                payload = page_payload_size;
            }
            if(payload < record_size) {
                return false;
            }

            const size_t page_bytes = page_overhead + payload;
            if(page_bytes < payload || estimate->total_bytes > (SIZE_MAX - page_bytes)) {
                return false;
            }

            estimate->total_bytes += page_bytes;
            estimate->current_page_payload = payload;
            estimate->current_page_used = 0U;
            offset = flippass_db_align_up_size(estimate->current_page_used, sizeof(uint32_t));
        }

        if(offset > estimate->current_page_payload ||
           record_size > (estimate->current_page_payload - offset)) {
            return false;
        }

        estimate->current_page_used = offset + record_size;
        estimate->record_count++;
        estimate->plain_bytes += chunk;
        remaining -= chunk;
    }

    return true;
}

static bool flippass_db_vault_file_estimate_add_plain(
    FlipPassDbVaultFileEstimate* estimate,
    size_t plain_len) {
    furi_assert(estimate);

    if(plain_len == 0U) {
        return true;
    }

    const uint32_t records =
        (uint32_t)((plain_len + KDBX_VAULT_RECORD_PLAIN_MAX - 1U) / KDBX_VAULT_RECORD_PLAIN_MAX);
    if(estimate->record_count > (UINT32_MAX - records) ||
       estimate->plain_bytes > (SIZE_MAX - plain_len)) {
        return false;
    }

    estimate->record_count += records;
    estimate->plain_bytes += plain_len;
    return true;
}

static bool flippass_db_preflight_append_text_segment(
    FlipPassDbPreflightContext* ctx,
    const char* data,
    int len) {
    furi_assert(ctx);
    furi_assert(data);

    if(!furi_string_cat_printf(ctx->text_value, "%.*s", len, data)) {
        flippass_db_preflight_set_error(
            ctx, "Not enough RAM is available to estimate the XML model safely.");
        return false;
    }

    return true;
}

static void flippass_db_preflight_begin_text(
    FlipPassDbPreflightContext* ctx,
    FlipPassDbPreflightTextState state) {
    furi_assert(ctx);

    ctx->text_state = state;
    furi_string_reset(ctx->text_value);
    if(state != FlipPassDbPreflightTextStateStringValue) {
        furi_string_reset(ctx->string_key);
    }
}

static void flippass_db_preflight_set_error(
    FlipPassDbPreflightContext* ctx,
    const char* format,
    ...) {
    furi_assert(ctx);
    furi_assert(format);

    if(ctx->parse_failed) {
        return;
    }

    ctx->parse_failed = true;
    va_list args;
    va_start(args, format);
    vsnprintf(ctx->parse_error, sizeof(ctx->parse_error), format, args);
    va_end(args);
}

static bool flippass_db_preflight_commit_entry_value(
    FlipPassDbPreflightContext* ctx,
    const char* key,
    size_t value_len) {
    furi_assert(ctx);

    if(key == NULL || key[0] == '\0') {
        return false;
    }

    if(strcmp(key, "Title") == 0) {
        const size_t alloc_size = value_len + 1U;
        return flippass_db_arena_estimate_alloc(
                   &ctx->ram_arena, FLIPPASS_DB_ARENA_CHUNK_SIZE, alloc_size, sizeof(char)) &&
               flippass_db_arena_estimate_alloc(
                   &ctx->file_arena, FLIPPASS_DB_ARENA_CHUNK_SIZE, alloc_size, sizeof(char));
    }

    if(strcmp(key, "UUID") == 0) {
        const size_t alloc_size = value_len + 1U;
        return flippass_db_arena_estimate_alloc(
                   &ctx->ram_arena, FLIPPASS_DB_ARENA_CHUNK_SIZE, alloc_size, sizeof(char)) &&
               flippass_db_vault_ram_estimate_add_plain(&ctx->ram_vault, value_len) &&
               flippass_db_vault_file_estimate_add_plain(&ctx->file_vault, value_len);
    }

    if(strcmp(key, "UserName") == 0 || strcmp(key, "Password") == 0 || strcmp(key, "URL") == 0 ||
       strcmp(key, "Notes") == 0 || strcmp(key, "AutoType") == 0) {
        if(!flippass_db_vault_ram_estimate_add_plain(&ctx->ram_vault, value_len) ||
           !flippass_db_vault_file_estimate_add_plain(&ctx->file_vault, value_len)) {
            return false;
        }
        return true;
    }

    const size_t key_len = strlen(key);
    ctx->custom_field_count++;
    if(!flippass_db_arena_estimate_alloc(
           &ctx->ram_arena,
           FLIPPASS_DB_ARENA_CHUNK_SIZE,
           sizeof(KDBXCustomField),
           sizeof(void*)) ||
       !flippass_db_arena_estimate_alloc(
           &ctx->ram_arena,
           FLIPPASS_DB_ARENA_CHUNK_SIZE,
           key_len + 1U,
           sizeof(char)) ||
       !flippass_db_arena_estimate_alloc(
           &ctx->file_arena,
           FLIPPASS_DB_ARENA_CHUNK_SIZE,
           sizeof(KDBXCustomField),
           sizeof(void*)) ||
       !flippass_db_arena_estimate_alloc(
           &ctx->file_arena,
           FLIPPASS_DB_ARENA_CHUNK_SIZE,
           key_len + 1U,
           sizeof(char)) ||
       !flippass_db_vault_ram_estimate_add_plain(&ctx->ram_vault, value_len) ||
       !flippass_db_vault_file_estimate_add_plain(&ctx->file_vault, value_len)) {
        return false;
    }

    return true;
}

static bool flippass_db_preflight_commit_text_value(
    FlipPassDbPreflightContext* ctx,
    FlipPassDbPreflightTextState state,
    const char* value,
    size_t value_len) {
    furi_assert(ctx);
    UNUSED(value);

    switch(state) {
    case FlipPassDbPreflightTextStateGroupName:
        return flippass_db_arena_estimate_alloc(
                   &ctx->ram_arena,
                   FLIPPASS_DB_ARENA_CHUNK_SIZE,
                   value_len + 1U,
                   sizeof(char)) &&
               flippass_db_arena_estimate_alloc(
                   &ctx->file_arena,
                   FLIPPASS_DB_ARENA_CHUNK_SIZE,
                   value_len + 1U,
                   sizeof(char));
    case FlipPassDbPreflightTextStateEntryUuid: {
        const size_t alloc_size = value_len + 1U;
        if(!flippass_db_arena_estimate_alloc(
               &ctx->ram_arena, FLIPPASS_DB_ARENA_CHUNK_SIZE, alloc_size, sizeof(char))) {
            return false;
        }
        if(value_len == 0U) {
            return true;
        }

        return flippass_db_vault_ram_estimate_add_plain(&ctx->ram_vault, value_len) &&
               flippass_db_vault_file_estimate_add_plain(&ctx->file_vault, value_len);
    }
    case FlipPassDbPreflightTextStateAutoTypeSequence:
        return flippass_db_preflight_commit_entry_value(ctx, "AutoType", value_len);
    case FlipPassDbPreflightTextStateStringValue:
        return flippass_db_preflight_commit_entry_value(
            ctx, furi_string_get_cstr(ctx->string_key), value_len);
    case FlipPassDbPreflightTextStateStringKey:
    case FlipPassDbPreflightTextStateNone:
    default:
        return true;
    }
}

static bool flippass_db_preflight_consume_inner_header(
    FlipPassDbPreflightContext* ctx,
    const uint8_t* data,
    size_t data_size,
    size_t* consumed) {
    furi_assert(ctx);
    furi_assert(consumed);

    *consumed = 0U;
    while(*consumed < data_size && !ctx->inner_header_done && !ctx->parse_failed) {
        if(ctx->inner_header_prefix_len == 0U && ctx->inner_field_remaining == 0U &&
           ctx->inner_field_id == 0U &&
           (data[*consumed] == '<' || data[*consumed] == 0xEFU)) {
            ctx->inner_header_done = true;
            return true;
        }

        if(ctx->inner_field_remaining == 0U &&
           ctx->inner_header_prefix_len < sizeof(ctx->inner_header_prefix)) {
            ctx->inner_header_prefix[ctx->inner_header_prefix_len++] = data[*consumed];
            (*consumed)++;

            if(ctx->inner_header_prefix_len < sizeof(ctx->inner_header_prefix)) {
                continue;
            }

            ctx->inner_field_id = ctx->inner_header_prefix[0];
            ctx->inner_field_size =
                ((uint32_t)ctx->inner_header_prefix[1]) |
                ((uint32_t)ctx->inner_header_prefix[2] << 8) |
                ((uint32_t)ctx->inner_header_prefix[3] << 16) |
                ((uint32_t)ctx->inner_header_prefix[4] << 24);
            ctx->inner_field_remaining = ctx->inner_field_size;
            ctx->inner_header_prefix_len = 0U;
        }

        const size_t available = data_size - *consumed;
        const size_t take =
            (available < ctx->inner_field_remaining) ? available : ctx->inner_field_remaining;

        *consumed += take;
        ctx->inner_field_remaining -= take;

        if(ctx->inner_field_remaining != 0U) {
            continue;
        }

        if(ctx->inner_field_id == 0U) {
            ctx->inner_header_done = true;
            return true;
        }

        ctx->inner_field_id = 0U;
        ctx->inner_field_size = 0U;
    }

    return !ctx->parse_failed;
}

static void flippass_db_preflight_start_element(
    void* context,
    const char* name,
    const char** attributes) {
    UNUSED(attributes);
    FlipPassDbPreflightContext* ctx = context;
    furi_assert(ctx);

    if(ctx->parse_failed) {
        return;
    }

    if(ctx->skipping_history) {
        ctx->history_skip_depth++;
        return;
    }

    if(strcmp(name, "History") == 0 && ctx->in_entry) {
        ctx->skipping_history = true;
        ctx->history_skip_depth = 1U;
        return;
    }

    if(strcmp(name, "Group") == 0) {
        if(!flippass_db_arena_estimate_alloc(
               &ctx->ram_arena,
               FLIPPASS_DB_ARENA_CHUNK_SIZE,
               sizeof(KDBXGroup),
               sizeof(void*)) ||
           !flippass_db_arena_estimate_alloc(
               &ctx->file_arena,
               FLIPPASS_DB_ARENA_CHUNK_SIZE,
               sizeof(KDBXGroup),
               sizeof(void*))) {
            flippass_db_preflight_set_error(
                ctx, "Not enough RAM is available to estimate the XML model safely.");
            return;
        }

        ctx->group_count++;
        ctx->group_depth++;
        ctx->in_group = true;
        return;
    }

    if(strcmp(name, "Entry") == 0) {
        if(!flippass_db_arena_estimate_alloc(
               &ctx->ram_arena,
               FLIPPASS_DB_ARENA_CHUNK_SIZE,
               sizeof(KDBXEntry),
               sizeof(void*)) ||
           !flippass_db_arena_estimate_alloc(
               &ctx->file_arena,
               FLIPPASS_DB_ARENA_CHUNK_SIZE,
               sizeof(KDBXEntry),
               sizeof(void*))) {
            flippass_db_preflight_set_error(
                ctx, "Not enough RAM is available to estimate the XML model safely.");
            return;
        }

        ctx->entry_count++;
        ctx->in_entry = true;
        return;
    }

    if(strcmp(name, "AutoType") == 0 && ctx->in_entry) {
        ctx->in_autotype = true;
        return;
    }

    if(strcmp(name, "Name") == 0 && ctx->in_group && !ctx->in_entry) {
        flippass_db_preflight_begin_text(ctx, FlipPassDbPreflightTextStateGroupName);
        return;
    }

    if(strcmp(name, "UUID") == 0 && ctx->in_entry) {
        flippass_db_preflight_begin_text(ctx, FlipPassDbPreflightTextStateEntryUuid);
        return;
    }

    if(strcmp(name, "DefaultSequence") == 0 && ctx->in_entry && ctx->in_autotype) {
        flippass_db_preflight_begin_text(ctx, FlipPassDbPreflightTextStateAutoTypeSequence);
        return;
    }

    if(strcmp(name, "String") == 0) {
        ctx->in_string = true;
        furi_string_reset(ctx->string_key);
        return;
    }

    if(strcmp(name, "Key") == 0 && ctx->in_entry && ctx->in_string) {
        flippass_db_preflight_begin_text(ctx, FlipPassDbPreflightTextStateStringKey);
        return;
    }

    if(strcmp(name, "Value") == 0 && ctx->in_entry && ctx->in_string) {
        flippass_db_preflight_begin_text(ctx, FlipPassDbPreflightTextStateStringValue);
    }
}

static void flippass_db_preflight_end_element(void* context, const char* name) {
    FlipPassDbPreflightContext* ctx = context;
    furi_assert(ctx);

    if(ctx->parse_failed) {
        return;
    }

    if(ctx->skipping_history) {
        if(ctx->history_skip_depth > 0U) {
            ctx->history_skip_depth--;
        }
        if(ctx->history_skip_depth == 0U) {
            ctx->skipping_history = false;
        }
        return;
    }

    if(strcmp(name, "Key") == 0 && ctx->text_state == FlipPassDbPreflightTextStateStringKey) {
        furi_string_set(ctx->string_key, ctx->text_value);
        furi_string_reset(ctx->text_value);
        ctx->text_state = FlipPassDbPreflightTextStateNone;
        return;
    }

    if((strcmp(name, "Value") == 0 && ctx->text_state == FlipPassDbPreflightTextStateStringValue) ||
       (strcmp(name, "Name") == 0 && ctx->text_state == FlipPassDbPreflightTextStateGroupName) ||
       (strcmp(name, "UUID") == 0 && ctx->text_state == FlipPassDbPreflightTextStateEntryUuid) ||
       (strcmp(name, "DefaultSequence") == 0 &&
        ctx->text_state == FlipPassDbPreflightTextStateAutoTypeSequence)) {
        const char* text = furi_string_get_cstr(ctx->text_value);
        if(!flippass_db_preflight_commit_text_value(
               ctx, ctx->text_state, text, furi_string_size(ctx->text_value))) {
            flippass_db_preflight_set_error(
                ctx, "Not enough RAM is available to estimate the XML model safely.");
        }
        furi_string_reset(ctx->text_value);
        ctx->text_state = FlipPassDbPreflightTextStateNone;
    }

    if(strcmp(name, "String") == 0) {
        ctx->in_string = false;
        return;
    }

    if(strcmp(name, "AutoType") == 0) {
        ctx->in_autotype = false;
        return;
    }

    if(strcmp(name, "Entry") == 0) {
        ctx->in_entry = false;
        return;
    }

    if(strcmp(name, "Group") == 0) {
        if(ctx->group_depth > 0U) {
            ctx->group_depth--;
        }
        ctx->in_group = ctx->group_depth > 0U;
    }
}

static void flippass_db_preflight_character_data(void* context, const char* data, int len) {
    FlipPassDbPreflightContext* ctx = context;
    furi_assert(ctx);

    if(ctx->parse_failed || data == NULL || len <= 0 || ctx->skipping_history ||
       ctx->text_state == FlipPassDbPreflightTextStateNone) {
        return;
    }

    flippass_db_preflight_append_text_segment(ctx, data, len);
}

static bool flippass_db_preflight_payload_chunk_callback(
    const uint8_t* data,
    size_t data_size,
    void* context) {
    FlipPassDbPreflightContext* ctx = context;
    size_t consumed = 0U;

    furi_assert(ctx);

    if(data == NULL) {
        return data_size == 0U;
    }

    while(consumed < data_size) {
        if(!ctx->inner_header_done) {
            size_t inner_consumed = 0U;
            if(!flippass_db_preflight_consume_inner_header(
                   ctx, data + consumed, data_size - consumed, &inner_consumed)) {
                return false;
            }
            consumed += inner_consumed;
            continue;
        }

        const size_t xml_chunk = data_size - consumed;
        if(ctx->xml_bytes > (FLIPPASS_DB_MAX_XML_STREAM_BYTES - xml_chunk)) {
            flippass_db_preflight_set_error(
                ctx, "The XML payload exceeds FlipPass's streaming limit.");
            return false;
        }

        ctx->xml_bytes += xml_chunk;
        if(!xml_parser_feed(ctx->xml_parser, (const char*)(data + consumed), xml_chunk, false)) {
            flippass_db_preflight_set_error(
                ctx,
                "%s",
                xml_parser_get_last_error(ctx->xml_parser) != NULL ?
                    xml_parser_get_last_error(ctx->xml_parser) :
                    "The XML payload could not be estimated.");
            return false;
        }

        consumed += xml_chunk;
    }

    return !ctx->parse_failed;
}

static FlipPassDbPreflightContext* flippass_db_preflight_context_alloc(App* app) {
    FlipPassDbPreflightContext* ctx = malloc(sizeof(*ctx));
    if(ctx == NULL) {
        return NULL;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->app = app;
    ctx->xml_parser = xml_parser_alloc();
    ctx->text_value = furi_string_alloc();
    ctx->string_key = furi_string_alloc();
    if(ctx->text_value != NULL) {
        furi_string_reserve(ctx->text_value, 96U);
    }
    if(ctx->string_key != NULL) {
        furi_string_reserve(ctx->string_key, 24U);
    }

    if(ctx->xml_parser == NULL || ctx->text_value == NULL || ctx->string_key == NULL) {
        flippass_db_preflight_context_free(ctx);
        return NULL;
    }

    xml_parser_set_callback_context(ctx->xml_parser, ctx);
    xml_parser_set_element_handlers(
        ctx->xml_parser, flippass_db_preflight_start_element, flippass_db_preflight_end_element);
    xml_parser_set_character_data_handler(ctx->xml_parser, flippass_db_preflight_character_data);
    return ctx;
}

static void flippass_db_preflight_context_free(FlipPassDbPreflightContext* ctx) {
    if(ctx == NULL) {
        return;
    }

    if(ctx->xml_parser != NULL) {
        xml_parser_free(ctx->xml_parser);
    }
    if(ctx->text_value != NULL) {
        furi_string_free(ctx->text_value);
    }
    if(ctx->string_key != NULL) {
        furi_string_free(ctx->string_key);
    }

    memzero(ctx, sizeof(*ctx));
    free(ctx);
}

static bool flippass_db_preflight_finalize(
    FlipPassDbPreflightContext* ctx,
    FlipPassDbPreflightSummary* out_summary) {
    furi_assert(ctx);
    furi_assert(out_summary);

    if(!xml_parser_feed(ctx->xml_parser, NULL, 0U, true)) {
        flippass_db_preflight_set_error(
            ctx,
            "%s",
            xml_parser_get_last_error(ctx->xml_parser) != NULL ?
                xml_parser_get_last_error(ctx->xml_parser) :
                "The XML payload could not be estimated.");
        return false;
    }

    if(ctx->parse_failed) {
        return false;
    }

    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->group_count = ctx->group_count;
    out_summary->entry_count = ctx->entry_count;
    out_summary->custom_field_count = ctx->custom_field_count;
    out_summary->ram_arena_bytes = ctx->ram_arena.total_bytes;
    out_summary->file_arena_bytes = ctx->file_arena.total_bytes;
    out_summary->ram_vault_page_bytes = ctx->ram_vault.total_bytes;
    out_summary->ram_vault_index_bytes = kdbx_vault_estimate_index_bytes(ctx->ram_vault.record_count);
    out_summary->file_vault_index_bytes =
        kdbx_vault_estimate_index_bytes(ctx->file_vault.record_count);
    out_summary->ram_vault_plain_bytes = ctx->ram_vault.plain_bytes;
    out_summary->file_vault_plain_bytes = ctx->file_vault.plain_bytes;
    out_summary->ram_record_count = ctx->ram_vault.record_count;
    out_summary->file_record_count = ctx->file_vault.record_count;
    out_summary->ram_total_bytes =
        out_summary->ram_arena_bytes + out_summary->ram_vault_page_bytes +
        out_summary->ram_vault_index_bytes;
    out_summary->file_total_bytes =
        out_summary->file_arena_bytes + out_summary->file_vault_index_bytes;
    out_summary->free_heap = memmgr_get_free_heap();
    out_summary->max_free_block = memmgr_heap_get_max_free_block();
    out_summary->ram_budget =
        (out_summary->free_heap > FLIPPASS_DB_SAFETY_RESERVE_BYTES) ?
            (out_summary->free_heap - FLIPPASS_DB_SAFETY_RESERVE_BYTES) :
            0U;
    out_summary->file_budget =
        (out_summary->free_heap > FLIPPASS_DB_FILE_MODEL_SAFETY_RESERVE_BYTES) ?
            (out_summary->free_heap - FLIPPASS_DB_FILE_MODEL_SAFETY_RESERVE_BYTES) :
            0U;
    const size_t ram_headroom =
        (out_summary->ram_budget > out_summary->ram_total_bytes) ?
            (out_summary->ram_budget - out_summary->ram_total_bytes) :
            0U;
    out_summary->prefer_file =
        out_summary->ram_total_bytes + FLIPPASS_DB_MODEL_GROWTH_RESERVE_BYTES >
            out_summary->ram_budget ||
        (out_summary->ram_vault_page_bytes >= (2U * KDBX_VAULT_RECORD_PLAIN_MAX) &&
         ram_headroom < (2U * FLIPPASS_DB_MODEL_GROWTH_RESERVE_BYTES));
    return true;
}

static bool flippass_db_run_preflight_from_vault(
    App* app,
    KDBXVault* vault,
    const KDBXFieldRef* ref,
    FlipPassDbPreflightSummary* out_summary,
    FuriString* error) {
    FlipPassDbPreflightContext* ctx = NULL;
    bool ok = false;

    furi_assert(app);
    furi_assert(out_summary);
    furi_assert(error);

    if(ref != NULL && flippass_db_try_fast_preflight_summary(ref->plain_len, out_summary)) {
        return true;
    }

    ctx = flippass_db_preflight_context_alloc(app);
    if(ctx == NULL) {
        furi_string_set_str(error, "Not enough RAM is available to estimate the XML model.");
        return false;
    }

    flippass_db_progress_update(app, "Sizing Model", "", 74U);
    ok = kdbx_vault_stream_ref(vault, ref, flippass_db_preflight_payload_chunk_callback, ctx);
    if(ok) {
        ok = flippass_db_preflight_finalize(ctx, out_summary);
    }

    if(!ok) {
        furi_string_set_str(
            error,
            ctx->parse_error[0] != '\0' ? ctx->parse_error :
                                          "The XML payload could not be estimated.");
    }

    flippass_db_preflight_context_free(ctx);
    return ok;
}

static bool flippass_db_try_fast_preflight_summary(
    size_t plain_xml_bytes,
    FlipPassDbPreflightSummary* out_summary) {
    FlipPassDbVaultRamEstimate ram_vault = {0};
    const size_t free_heap = memmgr_get_free_heap();
    const size_t ram_budget =
        (free_heap > FLIPPASS_DB_SAFETY_RESERVE_BYTES) ?
            (free_heap - FLIPPASS_DB_SAFETY_RESERVE_BYTES) :
            0U;
    const size_t estimated_deferred_plain = plain_xml_bytes / 3U;
    const size_t estimated_arena_bytes =
        (plain_xml_bytes / 24U) + (8U * FLIPPASS_DB_ARENA_CHUNK_SIZE);

    furi_assert(out_summary);

    if(plain_xml_bytes < (free_heap * 2U) && plain_xml_bytes < (64U * 1024U)) {
        return false;
    }

    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->free_heap = free_heap;
    out_summary->max_free_block = memmgr_heap_get_max_free_block();
    out_summary->ram_budget = ram_budget;
    out_summary->file_budget =
        (free_heap > FLIPPASS_DB_FILE_MODEL_SAFETY_RESERVE_BYTES) ?
            (free_heap - FLIPPASS_DB_FILE_MODEL_SAFETY_RESERVE_BYTES) :
            0U;
    out_summary->ram_arena_bytes = estimated_arena_bytes;
    out_summary->file_arena_bytes = estimated_arena_bytes;

    if(!flippass_db_vault_ram_estimate_add_plain(&ram_vault, estimated_deferred_plain)) {
        out_summary->prefer_file = true;
        out_summary->ram_total_bytes = plain_xml_bytes;
        out_summary->file_total_bytes = estimated_arena_bytes;
        return true;
    }

    out_summary->ram_vault_plain_bytes = ram_vault.plain_bytes;
    out_summary->file_vault_plain_bytes = estimated_deferred_plain;
    out_summary->ram_record_count = ram_vault.record_count;
    out_summary->file_record_count = ram_vault.record_count;
    out_summary->ram_vault_page_bytes = ram_vault.total_bytes;
    out_summary->ram_vault_index_bytes = kdbx_vault_estimate_index_bytes(ram_vault.record_count);
    out_summary->file_vault_index_bytes = out_summary->ram_vault_index_bytes;
    out_summary->ram_total_bytes =
        out_summary->ram_arena_bytes + out_summary->ram_vault_page_bytes +
        out_summary->ram_vault_index_bytes;
    out_summary->file_total_bytes =
        out_summary->file_arena_bytes + out_summary->file_vault_index_bytes;
    out_summary->prefer_file = true;
    return true;
}

static bool flippass_db_run_preflight_from_payload(
    App* app,
    const uint8_t* cipher_key,
    size_t cipher_key_size,
    const uint8_t* hmac_key,
    size_t hmac_key_size,
    FlipPassDbPreflightSummary* out_summary,
    FuriString* error) {
    FlipPassDbPreflightContext* ctx = NULL;
    bool ok = false;

    furi_assert(app);
    furi_assert(out_summary);
    furi_assert(error);

    ctx = flippass_db_preflight_context_alloc(app);
    if(ctx == NULL) {
        furi_string_set_str(error, "Not enough RAM is available to estimate the XML model.");
        return false;
    }

    flippass_db_progress_update(app, "Sizing Model", "", 60U);
    ok = kdbx_parser_stream_payload(
        app->kdbx_parser,
        cipher_key,
        cipher_key_size,
        hmac_key,
        hmac_key_size,
        flippass_db_preflight_payload_chunk_callback,
        ctx);
    if(ok) {
        ok = flippass_db_preflight_finalize(ctx, out_summary);
    }

    if(!ok) {
        furi_string_set_str(
            error,
            ctx->parse_error[0] != '\0' ? ctx->parse_error :
                                          "The XML payload could not be estimated.");
    }

    flippass_db_preflight_context_free(ctx);
    return ok;
}

static void flippass_db_log_preflight_summary(
    App* app,
    const FlipPassDbPreflightSummary* summary,
    const char* source) {
    UNUSED(source);

    if(app == NULL || summary == NULL) {
        return;
    }

    FLIPPASS_DEBUG_EVENT(
        app,
        "VAULT_PREFLIGHT source=%s groups=%lu entries=%lu custom=%lu ram_total=%lu ram_arena=%lu "
        "ram_pages=%lu ram_index=%lu ram_records=%lu file_total=%lu file_arena=%lu "
        "file_index=%lu file_records=%lu free=%lu max=%lu ram_budget=%lu file_budget=%lu "
        "prefer=%s",
        source != NULL ? source : "-",
        (unsigned long)summary->group_count,
        (unsigned long)summary->entry_count,
        (unsigned long)summary->custom_field_count,
        (unsigned long)summary->ram_total_bytes,
        (unsigned long)summary->ram_arena_bytes,
        (unsigned long)summary->ram_vault_page_bytes,
        (unsigned long)summary->ram_vault_index_bytes,
        (unsigned long)summary->ram_record_count,
        (unsigned long)summary->file_total_bytes,
        (unsigned long)summary->file_arena_bytes,
        (unsigned long)summary->file_vault_index_bytes,
        (unsigned long)summary->file_record_count,
        (unsigned long)summary->free_heap,
        (unsigned long)summary->max_free_block,
        (unsigned long)summary->ram_budget,
        (unsigned long)summary->file_budget,
        summary->prefer_file ? "ext" : "ram");
}

static bool flippass_db_apply_preflight_decision(
    App* app,
    const FlipPassDbPreflightSummary* summary,
    KDBXVaultBackend* backend,
    FuriString* error) {
    furi_assert(app);
    furi_assert(summary);
    furi_assert(backend);
    furi_assert(error);

    if(!summary->prefer_file) {
        return true;
    }

    if(!app->allow_ext_vault_promotion) {
        app->pending_vault_fallback = true;
        flippass_log_event(
            app,
            "VAULT_FALLBACK_OFFER stage=preflight remaining=%lu max=%lu request=%lu",
            (unsigned long)((summary->ram_budget > summary->ram_total_bytes) ?
                                (summary->ram_budget - summary->ram_total_bytes) :
                                0U),
            (unsigned long)summary->max_free_block,
            (unsigned long)(summary->ram_total_bytes + FLIPPASS_DB_MODEL_GROWTH_RESERVE_BYTES));
        if(app->rpc_mode) {
            furi_string_set_str(
                error,
                "The encrypted RAM vault needs /ext to finish this database. Retry unlock with backend 'ext'.");
        } else {
            furi_string_set_str(
                error,
                "FlipPass needs an encrypted /ext session file to finish opening this database.");
        }
        return false;
    }

    *backend = KDBXVaultBackendFileExt;
    app->requested_vault_backend = *backend;
    FLIPPASS_DEBUG_EVENT(
        app,
        "VAULT_PREFLIGHT_SWITCH from=%s to=%s ram_total=%lu file_total=%lu",
        kdbx_vault_backend_label(KDBXVaultBackendRam),
        kdbx_vault_backend_label(*backend),
        (unsigned long)summary->ram_total_bytes,
        (unsigned long)summary->file_total_bytes);
    flippass_db_progress_update(app, "Continuing on /ext", "", 80U);
    return true;
}

static FlipPassDbGzipStageState* flippass_db_gzip_stage_state_alloc(void) {
    FlipPassDbGzipStageState* state = malloc(sizeof(*state));
    if(state == NULL) {
        return NULL;
    }

    memset(state, 0, sizeof(*state));
    return state;
}

static void flippass_db_gzip_stage_state_free(FlipPassDbGzipStageState* state) {
    if(state == NULL) {
        return;
    }

    if(state->inflate_workspace != NULL) {
        memzero(state->inflate_workspace, kdbx_gzip_file_paged_workspace_size());
        free(state->inflate_workspace);
        state->inflate_workspace = NULL;
    }

    memzero(state, sizeof(*state));
    free(state);
}

static void flippass_db_progress_update(
    App* app,
    const char* stage,
    const char* detail,
    uint8_t percent) {
    if(app == NULL) {
        return;
    }

    flippass_progress_update(app, stage, detail, percent);
}

static void flippass_db_kdf_progress_callback(
    uint64_t current_round,
    uint64_t total_rounds,
    void* context) {
    App* app = context;
    char detail[64];
    uint8_t percent = 10U;
    uint32_t round_percent = 0U;

    if(app == NULL || total_rounds == 0U) {
        return;
    }

    if(current_round > total_rounds) {
        current_round = total_rounds;
    }

    percent = (uint8_t)(10U + ((current_round * 25U) / total_rounds));
    if(percent > 35U) {
        percent = 35U;
    }
    if(percent <= app->progress_percent && current_round != total_rounds) {
        return;
    }

    round_percent = (uint32_t)((current_round * 100U) / total_rounds);
    if(round_percent > 100U) {
        round_percent = 100U;
    }

    snprintf(detail, sizeof(detail), "Rounds %lu%%", (unsigned long)round_percent);
    flippass_db_progress_update(app, "Key Derivation", detail, percent);
}

static void flippass_db_gzip_progress_callback(
    const char* event,
    const KDBXGzipTelemetry* telemetry,
    void* context) {
    App* app = context;
    uint8_t percent = 58U;

    if(app == NULL || event == NULL || telemetry == NULL || telemetry->expected_output_size == 0U) {
        return;
    }

    if(strcmp(event, "progress") != 0 && strcmp(event, "done") != 0) {
        return;
    }

    percent = (uint8_t)(58U + ((telemetry->actual_output_size * 24U) / telemetry->expected_output_size));
    if(percent > 82U) {
        percent = 82U;
    }
    if(percent <= app->progress_percent && strcmp(event, "done") != 0) {
        return;
    }

    flippass_db_progress_update(app, "Uncompressing", "", percent);
}

static size_t flippass_db_active_safety_reserve(const FlipPassDbLoadContext* ctx) {
    if(ctx != NULL && ctx->inner_header_done && ctx->vault != NULL &&
       kdbx_vault_get_backend(ctx->vault) != KDBXVaultBackendRam) {
        return FLIPPASS_DB_FILE_MODEL_SAFETY_RESERVE_BYTES;
    }

    return FLIPPASS_DB_SAFETY_RESERVE_BYTES;
}

static void flippass_db_refresh_commit_budget(FlipPassDbLoadContext* ctx) {
    const size_t safety_reserve = flippass_db_active_safety_reserve(ctx);

    furi_assert(ctx);

    ctx->committed_bytes = kdbx_arena_bytes(ctx->arena);
    if(ctx->vault != NULL) {
        ctx->committed_bytes += kdbx_vault_index_bytes(ctx->vault);
        ctx->committed_bytes += kdbx_vault_page_bytes(ctx->vault);
    }

    const size_t free_heap = memmgr_get_free_heap();
    if(free_heap <= safety_reserve) {
        ctx->commit_limit = ctx->committed_bytes;
    } else {
        ctx->commit_limit = ctx->committed_bytes + free_heap - safety_reserve;
    }

    kdbx_arena_set_budget(ctx->arena, &ctx->committed_bytes, ctx->commit_limit);
    if(ctx->vault != NULL) {
        kdbx_vault_set_budget(ctx->vault, &ctx->committed_bytes, ctx->commit_limit);
    }
}

static bool flippass_db_should_preemptively_promote(
    const FlipPassDbLoadContext* ctx,
    size_t next_plain_len) {
    const size_t remaining_budget =
        (ctx != NULL && ctx->commit_limit > ctx->committed_bytes) ?
            (ctx->commit_limit - ctx->committed_bytes) :
            0U;
    const size_t max_free_block = memmgr_heap_get_max_free_block();

    if(ctx == NULL || ctx->app == NULL || ctx->vault == NULL ||
       kdbx_vault_get_backend(ctx->vault) != KDBXVaultBackendRam ||
       !kdbx_vault_backend_supported(KDBXVaultBackendFileExt)) {
        return false;
    }

    return remaining_budget <= (next_plain_len + (2U * KDBX_VAULT_RECORD_PLAIN_MAX)) ||
           max_free_block <= (6U * KDBX_VAULT_RECORD_PLAIN_MAX);
}

static bool flippass_db_should_stream_string_value(const FlipPassDbLoadContext* ctx) {
    const char* key = NULL;

    if(ctx == NULL || ctx->current_entry == NULL || ctx->vault == NULL ||
       kdbx_vault_get_backend(ctx->vault) == KDBXVaultBackendRam) {
        return false;
    }

    key = furi_string_get_cstr(ctx->string_key);
    if(key == NULL || key[0] == '\0') {
        return false;
    }

    return strcmp(key, "Title") != 0;
}

static bool flippass_db_begin_streamed_value(FlipPassDbLoadContext* ctx, const char* field_name) {
    furi_assert(ctx);
    furi_assert(field_name);

    ctx->deferred_stream_active = false;
    ctx->deferred_stream_plain_bytes = 0U;
    ctx->deferred_stream_logged_large = false;

    flippass_db_refresh_commit_budget(ctx);
    if(flippass_db_should_preemptively_promote(ctx, KDBX_VAULT_RECORD_PLAIN_MAX)) {
        if(ctx->app != NULL && !ctx->app->allow_ext_vault_promotion) {
            flippass_db_prepare_fallback_message(ctx, field_name, KDBX_VAULT_RECORD_PLAIN_MAX);
            return false;
        }
        if(!flippass_db_promote_vault_to_ext(ctx)) {
            return false;
        }
    }

    kdbx_vault_writer_reset_with_pending(
        &ctx->field_writer,
        ctx->vault,
        ctx->field_writer_pending,
        sizeof(ctx->field_writer_pending));
    if(ctx->field_writer.failed) {
        flippass_db_set_error(ctx, "Not enough RAM is available to keep this database open.");
        return false;
    }

    ctx->deferred_stream_active = true;
    return true;
}

static bool flippass_db_write_streamed_value_chunk(
    FlipPassDbLoadContext* ctx,
    const char* field_name,
    const char* data,
    size_t data_len) {
    furi_assert(ctx);
    furi_assert(field_name);
    furi_assert(data);

    if(!ctx->deferred_stream_active) {
        return false;
    }

    if(ctx->deferred_stream_plain_bytes > (FLIPPASS_DB_MAX_FIELD_PLAIN_BYTES - data_len)) {
        kdbx_vault_writer_abort(&ctx->field_writer);
        ctx->deferred_stream_active = false;
        flippass_db_set_error(
            ctx,
            "A database field exceeded FlipPass's %lu-byte field limit.",
            (unsigned long)FLIPPASS_DB_MAX_FIELD_PLAIN_BYTES);
        return false;
    }

    if(!kdbx_vault_writer_write(&ctx->field_writer, (const uint8_t*)data, data_len)) {
        kdbx_vault_writer_abort(&ctx->field_writer);
        ctx->deferred_stream_active = false;
        flippass_db_set_error(ctx, "Not enough RAM is available to keep this database open.");
        return false;
    }

    ctx->deferred_stream_plain_bytes += data_len;
    if(ctx->app != NULL && !ctx->deferred_stream_logged_large &&
       ctx->deferred_stream_plain_bytes >= 512U) {
        FLIPPASS_DEBUG_EVENT(
            ctx->app,
            "STREAM_VALUE_PROGRESS entry=%lu bytes=%lu records=%lu",
            (unsigned long)ctx->entry_count,
            (unsigned long)ctx->deferred_stream_plain_bytes,
            (unsigned long)kdbx_vault_record_count(ctx->vault));
        ctx->deferred_stream_logged_large = true;
    }
    UNUSED(field_name);
    return true;
}

static bool flippass_db_write_streamed_protected_chunk(
    const uint8_t* data,
    size_t data_len,
    void* context) {
    FlipPassDbLoadContext* ctx = context;

    if(data_len == 0U) {
        return true;
    }

    if(ctx == NULL || data == NULL) {
        return false;
    }

    return flippass_db_write_streamed_value_chunk(
        ctx,
        furi_string_get_cstr(ctx->string_key),
        (const char*)data,
        data_len);
}

static bool flippass_db_commit_streamed_value(
    FlipPassDbLoadContext* ctx,
    KDBXEntry* entry,
    const char* field_name) {
    KDBXFieldRef ref;
    const size_t value_len = ctx != NULL ? ctx->deferred_stream_plain_bytes : 0U;

    furi_assert(ctx);
    furi_assert(entry);
    furi_assert(field_name);

    if(!ctx->deferred_stream_active) {
        return false;
    }

    memset(&ref, 0, sizeof(ref));
    if(!kdbx_vault_writer_finish(&ctx->field_writer, &ref)) {
        kdbx_vault_writer_abort(&ctx->field_writer);
        ctx->deferred_stream_active = false;
        flippass_db_set_error(ctx, "Not enough RAM is available to keep this database open.");
        return false;
    }

    ctx->deferred_stream_active = false;
    ctx->deferred_stream_logged_large = false;

    if(strcmp(field_name, "UUID") == 0) {
        if(!kdbx_entry_set_uuid_ref(entry, &ref)) {
            flippass_db_set_error(ctx, "Not enough RAM is available to keep this database open.");
            return false;
        }
    } else if(strcmp(field_name, "UserName") == 0) {
        if(!kdbx_entry_set_field_ref(entry, KDBXEntryFieldUsername, &ref)) {
            flippass_db_set_error(ctx, "Not enough RAM is available to keep this database open.");
            return false;
        }
    } else if(strcmp(field_name, "Password") == 0) {
        if(!kdbx_entry_set_field_ref(entry, KDBXEntryFieldPassword, &ref)) {
            flippass_db_set_error(ctx, "Not enough RAM is available to keep this database open.");
            return false;
        }
    } else if(strcmp(field_name, "URL") == 0) {
        if(!kdbx_entry_set_field_ref(entry, KDBXEntryFieldUrl, &ref)) {
            flippass_db_set_error(ctx, "Not enough RAM is available to keep this database open.");
            return false;
        }
    } else if(strcmp(field_name, "Notes") == 0) {
        if(!kdbx_entry_set_field_ref(entry, KDBXEntryFieldNotes, &ref)) {
            flippass_db_set_error(ctx, "Not enough RAM is available to keep this database open.");
            return false;
        }
    } else {
        if(!flippass_db_prepare_for_arena_alloc(
               ctx,
               "custom_field",
               sizeof(KDBXCustomField) +
                   (strlen(field_name) < SIZE_MAX ? (strlen(field_name) + 1U) : strlen(field_name)))) {
            return false;
        }
        if(kdbx_entry_add_custom_field(entry, ctx->arena, field_name, &ref) == NULL) {
            FLIPPASS_DB_DEBUG_LOG_RAM(ctx, "custom_field_finish", field_name, value_len);
            flippass_db_set_error(ctx, "Not enough RAM is available to keep this database open.");
            return false;
        }
    }

    ctx->deferred_field_count++;
    ctx->deferred_plain_bytes += value_len;
    if(ctx->next_record_checkpoint == 0U) {
        ctx->next_record_checkpoint = 64U;
    }
    while(kdbx_vault_record_count(ctx->vault) >= ctx->next_record_checkpoint) {
        FLIPPASS_DB_DEBUG_LOG_CHECKPOINT(ctx, "record_checkpoint");
        ctx->next_record_checkpoint += 64U;
    }

    ctx->deferred_stream_plain_bytes = 0U;
    return true;
}

static bool flippass_db_prepare_for_arena_alloc(
    FlipPassDbLoadContext* ctx,
    const char* stage,
    size_t request_size) {
    size_t predicted_need = request_size;

    furi_assert(ctx);
    furi_assert(stage);

    flippass_db_refresh_commit_budget(ctx);

    if(predicted_need <= (SIZE_MAX - FLIPPASS_DB_MODEL_GROWTH_RESERVE_BYTES)) {
        predicted_need += FLIPPASS_DB_MODEL_GROWTH_RESERVE_BYTES;
    } else {
        predicted_need = SIZE_MAX;
    }

    if(!flippass_db_should_preemptively_promote(ctx, predicted_need)) {
        return true;
    }

    if(ctx->app != NULL && !ctx->app->allow_ext_vault_promotion) {
        flippass_db_prepare_fallback_message(ctx, stage, request_size);
        return false;
    }

    FLIPPASS_DEBUG_EVENT(
        ctx->app,
        "VAULT_PROMOTE_MODEL_HINT stage=%s remaining=%lu max=%lu request=%lu",
        stage,
        (unsigned long)((ctx->commit_limit > ctx->committed_bytes) ?
                            (ctx->commit_limit - ctx->committed_bytes) :
                            0U),
        (unsigned long)memmgr_heap_get_max_free_block(),
        (unsigned long)request_size);
    return flippass_db_promote_vault_to_ext(ctx);
}

static bool flippass_db_promote_vault_to_ext(FlipPassDbLoadContext* ctx) {
    KDBXVault* source_vault = NULL;
    KDBXVault* target_vault = NULL;

    furi_assert(ctx);

    if(ctx->vault == NULL || kdbx_vault_get_backend(ctx->vault) != KDBXVaultBackendRam ||
       !kdbx_vault_backend_supported(KDBXVaultBackendFileExt) ||
       ctx->app == NULL || !ctx->app->allow_ext_vault_promotion) {
        return false;
    }

    ctx->vault_promotion_attempted = true;
    source_vault = ctx->vault;
    FLIPPASS_DEBUG_EVENT(
        ctx->app,
        "VAULT_PROMOTE_BEGIN from=%s to=%s records=%lu",
        kdbx_vault_backend_label(kdbx_vault_get_backend(source_vault)),
        kdbx_vault_backend_label(KDBXVaultBackendFileExt),
        (unsigned long)kdbx_vault_record_count(source_vault));
    flippass_db_progress_update(ctx->app, "Continuing on /ext", "", ctx->app->progress_percent);

    target_vault = kdbx_vault_alloc(KDBXVaultBackendFileExt, NULL, 0U);
    if(target_vault == NULL || kdbx_vault_storage_failed(target_vault)) {
        if(target_vault != NULL) {
            kdbx_vault_free(target_vault);
        }
        FLIPPASS_DEBUG_EVENT(ctx->app, "VAULT_PROMOTE_FAIL stage=open_ext");
        flippass_db_set_error(
            ctx,
            "The RAM vault filled up and FlipPass could not continue on the encrypted SD-card session file.");
        return false;
    }

    if(!kdbx_vault_promote_ram_to_file(source_vault, target_vault)) {
        FLIPPASS_DEBUG_EVENT(
            ctx->app,
            "VAULT_PROMOTE_FAIL stage=%s",
            kdbx_vault_storage_stage(target_vault));
        kdbx_vault_free(target_vault);
        flippass_db_set_error(
            ctx,
            "The RAM vault filled up and FlipPass could not continue on the encrypted SD-card session file.");
        return false;
    }

    ctx->vault = target_vault;
    kdbx_vault_free(source_vault);
    flippass_db_refresh_commit_budget(ctx);
    FLIPPASS_DEBUG_EVENT(
        ctx->app,
        "VAULT_PROMOTE_OK records=%lu committed=%lu limit=%lu",
        (unsigned long)kdbx_vault_record_count(ctx->vault),
        (unsigned long)ctx->committed_bytes,
        (unsigned long)ctx->commit_limit);
    FLIPPASS_DB_DEBUG_LOG_MEM(ctx, "vault_promoted");
    return true;
}

static bool flippass_db_write_deferred_value(
    FlipPassDbLoadContext* ctx,
    const char* field_name,
    const char* value,
    size_t value_len,
    KDBXFieldRef* out_ref) {
    const uint32_t promotion_percent =
        ctx != NULL && ctx->app != NULL ? ctx->app->progress_percent : 0U;

    furi_assert(ctx);
    furi_assert(out_ref);

    flippass_db_refresh_commit_budget(ctx);

    if(flippass_db_should_preemptively_promote(ctx, value_len)) {
        if(ctx->app != NULL && !ctx->app->allow_ext_vault_promotion) {
            flippass_db_prepare_fallback_message(ctx, field_name, value_len);
            return false;
        }

        FLIPPASS_DEBUG_EVENT(
            ctx->app,
            "VAULT_PROMOTE_HINT remaining=%lu max=%lu request=%lu",
            (unsigned long)((ctx->commit_limit > ctx->committed_bytes) ?
                                (ctx->commit_limit - ctx->committed_bytes) :
                                0U),
            (unsigned long)memmgr_heap_get_max_free_block(),
            (unsigned long)value_len);
        if(!flippass_db_promote_vault_to_ext(ctx)) {
            return false;
        }
    }

    for(uint8_t attempt = 0U; attempt < 2U; attempt++) {
        kdbx_vault_writer_reset_with_pending(
            &ctx->field_writer,
            ctx->vault,
            ctx->field_writer_pending,
            sizeof(ctx->field_writer_pending));
        if(value_len > 0U &&
           !kdbx_vault_writer_write(&ctx->field_writer, (const uint8_t*)value, value_len)) {
            kdbx_vault_writer_abort(&ctx->field_writer);
            if(attempt == 0U && ctx->vault != NULL &&
               kdbx_vault_get_backend(ctx->vault) == KDBXVaultBackendRam &&
               kdbx_vault_budget_failed(ctx->vault)) {
                FLIPPASS_DB_DEBUG_LOG_RAM(ctx, "vault_writer_write", field_name, value_len);
                if(ctx->app != NULL && !ctx->app->allow_ext_vault_promotion) {
                    flippass_db_prepare_fallback_message(ctx, "vault_writer_write", value_len);
                    return false;
                }
                if(flippass_db_promote_vault_to_ext(ctx)) {
                    if(ctx->app != NULL) {
                        flippass_db_progress_update(
                            ctx->app, "Continuing on /ext", "", promotion_percent);
                    }
                    continue;
                }

                return false;
            }

            FLIPPASS_DB_DEBUG_LOG_RAM(ctx, "vault_writer_write", field_name, value_len);
            flippass_db_set_error(ctx, "Not enough RAM is available to keep this database open.");
            return false;
        }

        if(!kdbx_vault_writer_finish(&ctx->field_writer, out_ref)) {
            kdbx_vault_writer_abort(&ctx->field_writer);
            if(attempt == 0U && ctx->vault != NULL &&
               kdbx_vault_get_backend(ctx->vault) == KDBXVaultBackendRam &&
               kdbx_vault_budget_failed(ctx->vault)) {
                FLIPPASS_DB_DEBUG_LOG_RAM(ctx, "vault_writer_finish", field_name, value_len);
                if(ctx->app != NULL && !ctx->app->allow_ext_vault_promotion) {
                    flippass_db_prepare_fallback_message(ctx, "vault_writer_finish", value_len);
                    return false;
                }
                if(flippass_db_promote_vault_to_ext(ctx)) {
                    if(ctx->app != NULL) {
                        flippass_db_progress_update(
                            ctx->app, "Continuing on /ext", "", promotion_percent);
                    }
                    continue;
                }

                return false;
            }

            FLIPPASS_DB_DEBUG_LOG_RAM(ctx, "vault_writer_finish", field_name, value_len);
            flippass_db_set_error(ctx, "Not enough RAM is available to keep this database open.");
            return false;
        }

        return true;
    }

    flippass_db_set_error(ctx, "Not enough RAM is available to keep this database open.");
    return false;
}

static void flippass_db_set_error(FlipPassDbLoadContext* ctx, const char* format, ...) {
    furi_assert(ctx);
    furi_assert(format);

    if(ctx->parse_failed) {
        return;
    }

    ctx->parse_failed = true;
    va_list args;
    va_start(args, format);
    vsnprintf(ctx->parse_error, sizeof(ctx->parse_error), format, args);
    va_end(args);

    if(ctx->app != NULL) {
        const char* string_key =
            (ctx->string_key != NULL) ? furi_string_get_cstr(ctx->string_key) : NULL;
        flippass_log_event(
            ctx->app,
            "PARSE_ERROR_CTX reason=%s state=%s key=%s in_group=%u in_entry=%u groups=%lu "
            "entries=%lu xml=%lu arena=%lu arena_fail=%u arena_reason=%s arena_size=%lu "
            "arena_committed=%lu arena_max=%lu vault=%s records=%lu index=%lu pages=%lu "
            "vault_fail=%u vault_reason=%s vault_reader=%s vault_reader_record=%lu "
            "storage_stage=%s free=%lu max=%lu stack=%lu",
            ctx->parse_error,
            flippass_db_text_state_label(ctx->text_state),
            (string_key != NULL && string_key[0] != '\0') ? string_key : "-",
            ctx->in_group ? 1U : 0U,
            ctx->in_entry ? 1U : 0U,
            (unsigned long)ctx->group_count,
            (unsigned long)ctx->entry_count,
            (unsigned long)ctx->xml_bytes,
            (unsigned long)kdbx_arena_bytes(ctx->arena),
            kdbx_arena_budget_failed(ctx->arena) ? 1U : 0U,
            kdbx_arena_failure_reason(ctx->arena),
            (unsigned long)kdbx_arena_last_failed_size(ctx->arena),
            (unsigned long)kdbx_arena_last_failed_committed(ctx->arena),
            (unsigned long)kdbx_arena_last_failed_max_free_block(ctx->arena),
            (ctx->vault != NULL) ? kdbx_vault_backend_label(kdbx_vault_get_backend(ctx->vault)) :
                                   "-",
            (unsigned long)kdbx_vault_record_count(ctx->vault),
            (unsigned long)kdbx_vault_index_bytes(ctx->vault),
            (unsigned long)kdbx_vault_page_bytes(ctx->vault),
            kdbx_vault_budget_failed(ctx->vault) ? 1U : 0U,
            kdbx_vault_failure_reason(ctx->vault),
            kdbx_vault_last_reader_failure(ctx->vault),
            (unsigned long)kdbx_vault_last_reader_failure_record(ctx->vault),
            kdbx_vault_storage_stage(ctx->vault),
            (unsigned long)memmgr_get_free_heap(),
            (unsigned long)memmgr_heap_get_max_free_block(),
            (unsigned long)furi_thread_get_stack_space(furi_thread_get_current_id()));
    }
}

#if FLIPPASS_ENABLE_VERBOSE_UNLOCK_LOG
static void flippass_db_log_mem_snapshot(FlipPassDbLoadContext* ctx, const char* stage) {
    furi_assert(ctx);
    furi_assert(stage);

    if(ctx->app == NULL) {
        return;
    }

    flippass_log_event(
        ctx->app,
        "MEM stage=%s free=%lu max=%lu committed=%lu limit=%lu arena=%lu arena_fail=%s "
        "vault_records=%lu vault_index=%lu vault_ram=%lu vault_fail=%s xml=%lu groups=%lu "
        "entries=%lu fields=%lu field_plain=%lu",
        stage,
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block(),
        (unsigned long)ctx->committed_bytes,
        (unsigned long)ctx->commit_limit,
        (unsigned long)kdbx_arena_bytes(ctx->arena),
        kdbx_arena_failure_reason(ctx->arena),
        (unsigned long)kdbx_vault_record_count(ctx->vault),
        (unsigned long)kdbx_vault_index_bytes(ctx->vault),
        (unsigned long)kdbx_vault_page_bytes(ctx->vault),
        kdbx_vault_failure_reason(ctx->vault),
        (unsigned long)ctx->xml_bytes,
        (unsigned long)ctx->group_count,
        (unsigned long)ctx->entry_count,
        (unsigned long)ctx->deferred_field_count,
        (unsigned long)ctx->deferred_plain_bytes);
}

static void flippass_db_log_ram_failure(
    FlipPassDbLoadContext* ctx,
    const char* stage,
    const char* field_name,
    size_t request_size) {
    furi_assert(ctx);
    furi_assert(stage);

    if(ctx->app == NULL) {
        return;
    }

    flippass_log_event(
        ctx->app,
        "RAM_FAIL stage=%s field=%s request=%lu free=%lu max=%lu committed=%lu limit=%lu "
        "arena=%lu arena_fail=%s arena_need=%lu arena_commit=%lu arena_max=%lu "
        "vault_records=%lu vault_index=%lu vault_ram=%lu vault_fail=%s vault_need=%lu "
        "vault_commit=%lu vault_max=%lu vault_storage=%s xml=%lu groups=%lu entries=%lu "
        "fields=%lu field_plain=%lu",
        stage,
        field_name != NULL ? field_name : "-",
        (unsigned long)request_size,
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block(),
        (unsigned long)ctx->committed_bytes,
        (unsigned long)ctx->commit_limit,
        (unsigned long)kdbx_arena_bytes(ctx->arena),
        kdbx_arena_failure_reason(ctx->arena),
        (unsigned long)kdbx_arena_last_failed_size(ctx->arena),
        (unsigned long)kdbx_arena_last_failed_committed(ctx->arena),
        (unsigned long)kdbx_arena_last_failed_max_free_block(ctx->arena),
        (unsigned long)kdbx_vault_record_count(ctx->vault),
        (unsigned long)kdbx_vault_index_bytes(ctx->vault),
        (unsigned long)kdbx_vault_page_bytes(ctx->vault),
        kdbx_vault_failure_reason(ctx->vault),
        (unsigned long)kdbx_vault_last_failed_size(ctx->vault),
        (unsigned long)kdbx_vault_last_failed_committed(ctx->vault),
        (unsigned long)kdbx_vault_last_failed_max_free_block(ctx->vault),
        kdbx_vault_storage_stage(ctx->vault),
        (unsigned long)ctx->xml_bytes,
        (unsigned long)ctx->group_count,
        (unsigned long)ctx->entry_count,
        (unsigned long)ctx->deferred_field_count,
        (unsigned long)ctx->deferred_plain_bytes);
}

static void flippass_db_log_checkpoint_snapshot(FlipPassDbLoadContext* ctx, const char* stage) {
    flippass_db_log_mem_snapshot(ctx, stage);
}
#endif

static KDBXVaultBackend flippass_db_select_gzip_scratch_backend(KDBXVaultBackend preferred_backend) {
    if(kdbx_vault_backend_supported(KDBXVaultBackendFileExt)) {
        return KDBXVaultBackendFileExt;
    }

    if(kdbx_vault_backend_supported(KDBXVaultBackendFileInt)) {
        return KDBXVaultBackendFileInt;
    }

    if(preferred_backend != KDBXVaultBackendNone &&
       kdbx_vault_backend_supported(preferred_backend)) {
        return preferred_backend;
    }

    if(kdbx_vault_backend_supported(KDBXVaultBackendRam)) {
        return KDBXVaultBackendRam;
    }

    if(kdbx_vault_backend_supported(KDBXVaultBackendFileInt)) {
        return KDBXVaultBackendFileInt;
    }

    if(kdbx_vault_backend_supported(KDBXVaultBackendFileExt)) {
        return KDBXVaultBackendFileExt;
    }

    return KDBXVaultBackendNone;
}

static const char* flippass_db_gzip_scratch_path(KDBXVaultBackend backend) {
    switch(backend) {
    case KDBXVaultBackendFileInt:
        return KDBX_VAULT_SCRATCH_INT_PATH;
    case KDBXVaultBackendFileExt:
        return KDBX_VAULT_SCRATCH_EXT_PATH;
    default:
        return NULL;
    }
}

static const char* flippass_db_gzip_member_path(KDBXVaultBackend backend) {
    switch(backend) {
    case KDBXVaultBackendFileInt:
        return KDBX_VAULT_MEMBER_INT_PATH;
    case KDBXVaultBackendFileExt:
        return KDBX_VAULT_MEMBER_EXT_PATH;
    default:
        return NULL;
    }
}

static void flippass_db_set_gzip_stage_error(
    FuriString* error,
    const KDBXGzipTelemetry* telemetry) {
    furi_assert(error);
    furi_assert(telemetry);

    switch(telemetry->status) {
    case KDBXGzipStatusInvalidHeader:
        furi_string_set_str(error, "The staged GZip payload is not valid GZip data.");
        break;
    case KDBXGzipStatusReservedFlags:
        furi_string_set_str(error, "The staged GZip payload uses unsupported header flags.");
        break;
    case KDBXGzipStatusInvalidExtraField:
    case KDBXGzipStatusInvalidNameField:
    case KDBXGzipStatusInvalidCommentField:
    case KDBXGzipStatusInvalidHeaderCrcField:
    case KDBXGzipStatusInvalidBodyOffset:
    case KDBXGzipStatusTruncatedInput:
    case KDBXGzipStatusInputSizeMismatch:
        furi_string_set_str(error, "The staged GZip payload is truncated or malformed.");
        break;
    case KDBXGzipStatusOutputTooLarge:
        furi_string_set_str(
            error, "This compressed database expands beyond FlipPass's XML stream limit.");
        break;
    case KDBXGzipStatusWorkspaceAllocFailed:
    case KDBXGzipStatusWorkspaceTotalTooSmall:
    case KDBXGzipStatusWorkspacePageAllocFailed:
        furi_string_set_str(
            error, "Not enough RAM is available to keep the GZip dictionary while streaming.");
        break;
    case KDBXGzipStatusWorkspaceStorageFailed:
        furi_string_set_str(
            error, "The encrypted GZip dictionary scratch file could not be used safely.");
        break;
    case KDBXGzipStatusWorkspaceVerifyFailed:
        furi_string_set_str(
            error, "The encrypted GZip dictionary scratch file did not verify cleanly.");
        break;
    case KDBXGzipStatusPagedNoProgress:
        furi_string_set_str(
            error, "The paged GZip inflater stopped making progress before the payload could be replayed.");
        break;
    case KDBXGzipStatusPagedTimeLimit:
        furi_string_set_str(
            error, "The paged GZip inflater exceeded the safe runtime budget before it could finish.");
        break;
    case KDBXGzipStatusOutputSizeMismatch:
        furi_string_printf(
            error,
            "The decompressed database size did not match the GZip trailer (%lu vs %lu).",
            (unsigned long)telemetry->actual_output_size,
            (unsigned long)telemetry->expected_output_size);
        break;
    case KDBXGzipStatusCrcMismatch:
        furi_string_set_str(error, "The decompressed database CRC did not match the GZip trailer.");
        break;
    case KDBXGzipStatusOutputRejected:
        furi_string_set_str(error, "The staged GZip output was rejected by the scratch writer.");
        break;
    case KDBXGzipStatusInflateFailed:
    case KDBXGzipStatusOutputAllocFailed:
    case KDBXGzipStatusOutputHeapFragmented:
    case KDBXGzipStatusInvalidArgument:
    case KDBXGzipStatusOk:
    default:
        furi_string_set_str(error, "Unable to decompress the staged GZip payload.");
        break;
    }
}

static bool flippass_db_validate_header(const KDBXHeader* header, FuriString* error) {
    const bool is_aes =
        header != NULL &&
        memcmp(header->encryption_algorithm_uuid, KDBX_UUID_AES256, sizeof(KDBX_UUID_AES256)) ==
            0;
    const bool is_chacha20 =
        header != NULL &&
        memcmp(
            header->encryption_algorithm_uuid,
            KDBX_UUID_CHACHA20,
            sizeof(KDBX_UUID_CHACHA20)) == 0;

    if(header == NULL) {
        furi_string_set_str(error, "Failed to read the database header.");
        return false;
    }

    if(!is_aes && !is_chacha20) {
        furi_string_set_str(error, "Only AES256 or ChaCha20 KDBX 4 databases are supported.");
        return false;
    }

    if(header->compression_algorithm != KDBX_COMPRESSION_NONE &&
       header->compression_algorithm != KDBX_COMPRESSION_GZIP) {
        furi_string_set_str(
            error, "Only raw or GZip-compressed KDBX 4 payloads are supported.");
        return false;
    }

    if(is_aes && header->encryption_iv_size != 16U) {
        furi_string_set_str(error, "AES-encrypted databases must use a 16-byte IV.");
        return false;
    }

    if(is_chacha20 && header->encryption_iv_size != 12U) {
        furi_string_set_str(error, "ChaCha20-encrypted databases must use a 12-byte nonce.");
        return false;
    }

    return true;
}

static const char* flippass_db_find_attribute(const char** attributes, const char* name) {
    if(attributes == NULL || name == NULL) {
        return NULL;
    }

    for(size_t index = 0U; attributes[index] != NULL && attributes[index + 1U] != NULL;
        index += 2U) {
        if(strcmp(attributes[index], name) == 0) {
            return attributes[index + 1U];
        }
    }

    return NULL;
}

static bool flippass_db_store_field(
    FlipPassDbLoadContext* ctx,
    KDBXEntry* entry,
    uint32_t field_mask,
    const char* value,
    size_t value_len) {
    KDBXFieldRef ref;

    if(value_len > FLIPPASS_DB_MAX_FIELD_PLAIN_BYTES) {
        flippass_db_set_error(
            ctx,
            "A database field exceeded FlipPass's %lu-byte field limit.",
            (unsigned long)FLIPPASS_DB_MAX_FIELD_PLAIN_BYTES);
        return false;
    }

    if(!flippass_db_write_deferred_value(
           ctx, flippass_db_field_log_name(field_mask), value, value_len, &ref) ||
       !kdbx_entry_set_field_ref(entry, field_mask, &ref)) {
        return false;
    }

    ctx->deferred_field_count++;
    ctx->deferred_plain_bytes += value_len;
    if(ctx->next_record_checkpoint == 0U) {
        ctx->next_record_checkpoint = 64U;
    }
    while(kdbx_vault_record_count(ctx->vault) >= ctx->next_record_checkpoint) {
        FLIPPASS_DB_DEBUG_LOG_CHECKPOINT(ctx, "record_checkpoint");
        ctx->next_record_checkpoint += 64U;
    }

    return true;
}

static bool flippass_db_should_defer_entry_uuid(const FlipPassDbLoadContext* ctx) {
    return ctx != NULL && ctx->vault != NULL &&
           kdbx_vault_get_backend(ctx->vault) != KDBXVaultBackendRam;
}

static bool flippass_db_store_entry_uuid(
    FlipPassDbLoadContext* ctx,
    KDBXEntry* entry,
    const char* value,
    size_t value_len) {
    KDBXFieldRef ref;

    if(entry == NULL) {
        return false;
    }

    if(!flippass_db_should_defer_entry_uuid(ctx)) {
        if(!flippass_db_prepare_for_arena_alloc(
               ctx,
               "entry_uuid",
               value_len < SIZE_MAX ? (value_len + 1U) : value_len)) {
            return false;
        }
        if(!kdbx_entry_set_uuid(entry, ctx->arena, value)) {
            FLIPPASS_DB_DEBUG_LOG_RAM(ctx, "entry_uuid", "UUID", value_len);
            flippass_db_set_error(ctx, "Not enough RAM is available to keep this database open.");
            return false;
        }
        return true;
    }

    if(!flippass_db_write_deferred_value(ctx, "UUID", value, value_len, &ref) ||
       !kdbx_entry_set_uuid_ref(entry, &ref)) {
        return false;
    }

    ctx->deferred_field_count++;
    ctx->deferred_plain_bytes += value_len;
    if(ctx->next_record_checkpoint == 0U) {
        ctx->next_record_checkpoint = 64U;
    }
    while(kdbx_vault_record_count(ctx->vault) >= ctx->next_record_checkpoint) {
        FLIPPASS_DB_DEBUG_LOG_CHECKPOINT(ctx, "record_checkpoint");
        ctx->next_record_checkpoint += 64U;
    }

    return true;
}

static bool flippass_db_store_custom_field(
    FlipPassDbLoadContext* ctx,
    KDBXEntry* entry,
    const char* key,
    const char* value,
    size_t value_len) {
    KDBXFieldRef ref;

    if(entry == NULL || key == NULL || key[0] == '\0') {
        return false;
    }

    if(value_len > FLIPPASS_DB_MAX_FIELD_PLAIN_BYTES) {
        flippass_db_set_error(
            ctx,
            "A database field exceeded FlipPass's %lu-byte field limit.",
            (unsigned long)FLIPPASS_DB_MAX_FIELD_PLAIN_BYTES);
        return false;
    }

    if(ctx->app != NULL && value_len >= 512U) {
        FLIPPASS_DEBUG_EVENT(
            ctx->app,
            "CUSTOM_FIELD_LARGE_BEGIN entry=%lu len=%lu records=%lu",
            (unsigned long)ctx->entry_count,
            (unsigned long)value_len,
            (unsigned long)kdbx_vault_record_count(ctx->vault));
    }

    if(!flippass_db_write_deferred_value(ctx, key, value, value_len, &ref)) {
        return false;
    }

    if(!flippass_db_prepare_for_arena_alloc(
           ctx,
           "custom_field",
           sizeof(KDBXCustomField) + (strlen(key) < SIZE_MAX ? (strlen(key) + 1U) : strlen(key)))) {
        return false;
    }

    if(kdbx_entry_add_custom_field(entry, ctx->arena, key, &ref) == NULL) {
        FLIPPASS_DB_DEBUG_LOG_RAM(ctx, "custom_field_finish", key, value_len);
        flippass_db_set_error(ctx, "Not enough RAM is available to keep this database open.");
        return false;
    }

    if(ctx->app != NULL && value_len >= 512U) {
        FLIPPASS_DEBUG_EVENT(
            ctx->app,
            "CUSTOM_FIELD_LARGE_OK entry=%lu len=%lu records=%lu",
            (unsigned long)ctx->entry_count,
            (unsigned long)value_len,
            (unsigned long)kdbx_vault_record_count(ctx->vault));
    }

    ctx->deferred_field_count++;
    ctx->deferred_plain_bytes += value_len;
    if(ctx->next_record_checkpoint == 0U) {
        ctx->next_record_checkpoint = 64U;
    }
    while(kdbx_vault_record_count(ctx->vault) >= ctx->next_record_checkpoint) {
        FLIPPASS_DB_DEBUG_LOG_CHECKPOINT(ctx, "record_checkpoint");
        ctx->next_record_checkpoint += 64U;
    }

    return true;
}

static bool flippass_db_commit_entry_value(
    FlipPassDbLoadContext* ctx,
    KDBXEntry* entry,
    const char* key,
    const char* value,
    size_t value_len) {
    if(strcmp(key, "Title") == 0) {
        if(!flippass_db_prepare_for_arena_alloc(
               ctx,
               "entry_title",
               value_len < SIZE_MAX ? (value_len + 1U) : value_len)) {
            return false;
        }
        if(entry == NULL || !kdbx_entry_set_title(entry, ctx->arena, value)) {
            FLIPPASS_DB_DEBUG_LOG_RAM(ctx, "entry_title", "Title", value_len);
            flippass_db_set_error(ctx, "Not enough RAM is available to keep this database open.");
            return false;
        }

        return true;
    }

    if(strcmp(key, "UUID") == 0) {
        return flippass_db_store_entry_uuid(ctx, entry, value, value_len);
    }

    if(strcmp(key, "UserName") == 0) {
        return flippass_db_store_field(ctx, entry, KDBXEntryFieldUsername, value, value_len);
    }
    if(strcmp(key, "Password") == 0) {
        return flippass_db_store_field(ctx, entry, KDBXEntryFieldPassword, value, value_len);
    }
    if(strcmp(key, "URL") == 0) {
        return flippass_db_store_field(ctx, entry, KDBXEntryFieldUrl, value, value_len);
    }
    if(strcmp(key, "Notes") == 0) {
        return flippass_db_store_field(ctx, entry, KDBXEntryFieldNotes, value, value_len);
    }

    return flippass_db_store_custom_field(ctx, entry, key, value, value_len);
}

static bool flippass_db_commit_text_value(
    FlipPassDbLoadContext* ctx,
    FlipPassDbTextState state,
    const char* value,
    size_t value_len) {
    char* decoded_value = NULL;
    size_t decoded_size = 0U;
    bool ok = true;
    const char* string_key = NULL;

    switch(state) {
    case FlipPassDbTextStateGroupName:
        if(!flippass_db_prepare_for_arena_alloc(
               ctx,
               "group_name",
               value_len < SIZE_MAX ? (value_len + 1U) : value_len)) {
            return false;
        }
        ok = ctx->current_group != NULL &&
             kdbx_group_set_name(ctx->current_group, ctx->arena, value);
        if(!ok) {
            FLIPPASS_DB_DEBUG_LOG_RAM(ctx, "group_name", "Name", value_len);
        }
        break;
    case FlipPassDbTextStateGroupUuid:
        UNUSED(value);
        UNUSED(value_len);
        ok = true;
        break;
    case FlipPassDbTextStateEntryUuid:
        ok = ctx->current_entry != NULL &&
             flippass_db_store_entry_uuid(ctx, ctx->current_entry, value, value_len);
        break;
    case FlipPassDbTextStateAutoTypeSequence:
        ok = ctx->current_entry != NULL &&
             flippass_db_store_field(
                 ctx, ctx->current_entry, KDBXEntryFieldAutotype, value, value_len);
        break;
    case FlipPassDbTextStateStringValue:
        if(ctx->current_entry == NULL) {
            return false;
        }

        string_key = furi_string_get_cstr(ctx->string_key);

        if(ctx->value_protected) {
            if(!flippass_db_is_supported_string_key(string_key)) {
                if(!ctx->protected_stream.ready ||
                   !kdbx_protected_value_discard(&ctx->protected_stream, value)) {
                    flippass_db_set_error(ctx, "A protected entry field could not be decoded.");
                    return false;
                }

                return true;
            }

            if(!ctx->protected_stream.ready ||
               !kdbx_protected_value_decode_reuse(
                   &ctx->protected_stream,
                   value,
                   &decoded_value,
                   &decoded_size,
                   &ctx->protected_value_buffer.data,
                   &ctx->protected_value_buffer.capacity)) {
                flippass_db_set_error(ctx, "A protected entry field could not be decoded.");
                return false;
            }
            ctx->protected_value_buffer.size = decoded_size + 1U;
            value = decoded_value;
            value_len = decoded_size;
        }

        ok = flippass_db_commit_entry_value(
            ctx,
            ctx->current_entry,
            string_key,
            value,
            value_len);
        break;
    case FlipPassDbTextStateStringKey:
    case FlipPassDbTextStateNone:
    default:
        break;
    }

    if(decoded_value != NULL) {
        memzero(decoded_value, decoded_size + 1U);
        ctx->protected_value_buffer.size = 0U;
    }

    if(!ok) {
        flippass_db_set_error(ctx, "Not enough RAM is available to keep this database open.");
    }

    return ok;
}

static bool flippass_db_consume_history_protected_value(FlipPassDbLoadContext* ctx) {
    furi_assert(ctx);

    if(!ctx->history_value_protected || furi_string_size(ctx->text_value) == 0U) {
        return true;
    }

    if(!ctx->protected_stream.ready ||
       !kdbx_protected_value_discard(&ctx->protected_stream, furi_string_get_cstr(ctx->text_value))) {
        flippass_db_set_error(ctx, "A protected entry field could not be decoded.");
        return false;
    }

    return true;
}

static bool flippass_db_append_text_segment(
    FlipPassDbLoadContext* ctx,
    const char* data,
    int len) {
    furi_assert(ctx);
    furi_assert(data);

    if(!furi_string_cat_printf(ctx->text_value, "%.*s", len, data)) {
        FLIPPASS_DB_DEBUG_LOG_RAM(ctx, "text_value_append", "Text", (size_t)len);
        flippass_db_set_error(ctx, "Not enough RAM is available to keep this database open.");
        return false;
    }

    return true;
}

static void flippass_db_begin_text(FlipPassDbLoadContext* ctx, FlipPassDbTextState state) {
    furi_assert(ctx);

    ctx->text_state = state;
    furi_string_reset(ctx->text_value);
    if(state != FlipPassDbTextStateStringValue) {
        furi_string_reset(ctx->string_key);
    }
}

static void flippass_db_start_element(void* context, const char* name, const char** attributes) {
    FlipPassDbLoadContext* ctx = context;
    furi_assert(ctx);

    if(ctx->parse_failed) {
        return;
    }

    if(ctx->skipping_history) {
        ctx->history_skip_depth++;
        if(strcmp(name, "String") == 0) {
            ctx->history_in_string = true;
            ctx->history_collect_protected_value = false;
            ctx->history_value_protected = false;
            ctx->protected_discard_active = false;
            furi_string_reset(ctx->text_value);
        } else if(strcmp(name, "Value") == 0 && ctx->history_in_string) {
            const char* protected_value = flippass_db_find_attribute(attributes, "Protected");
            ctx->history_value_protected =
                protected_value != NULL &&
                (strcmp(protected_value, "True") == 0 || strcmp(protected_value, "true") == 0);
            ctx->history_collect_protected_value = ctx->history_value_protected;
            ctx->protected_discard_active = ctx->history_value_protected;
            if(ctx->protected_discard_active) {
                kdbx_protected_discard_state_init(&ctx->protected_discard_state);
            }
            furi_string_reset(ctx->text_value);
        }
        return;
    }

    if(strcmp(name, "History") == 0 && ctx->in_entry) {
        ctx->skipping_history = true;
        ctx->history_skip_depth = 1U;
        ctx->history_in_string = false;
        ctx->history_collect_protected_value = false;
        ctx->history_value_protected = false;
        ctx->protected_discard_active = false;
        furi_string_reset(ctx->text_value);
        return;
    }

    if(strcmp(name, "Group") == 0) {
        if(!flippass_db_prepare_for_arena_alloc(ctx, "group_alloc", sizeof(KDBXGroup))) {
            return;
        }
        KDBXGroup* group = kdbx_group_alloc(ctx->arena);
        if(group == NULL) {
            FLIPPASS_DB_DEBUG_LOG_RAM(ctx, "group_alloc", "Group", sizeof(KDBXGroup));
            flippass_db_set_error(ctx, "Not enough RAM is available to keep this database open.");
            return;
        }

        ctx->group_count++;
        if(ctx->current_group != NULL) {
            group->parent = ctx->current_group;
            group->next = ctx->current_group->children;
            ctx->current_group->children = group;
        } else {
            ctx->root_group = group;
        }

        ctx->current_group = group;
        ctx->in_group = true;
        ctx->parsing_depth++;
        if((size_t)ctx->parsing_depth > FLIPPASS_DB_MAX_XML_DEPTH) {
            flippass_db_set_error(ctx, "The XML nesting depth exceeds FlipPass's safe limit.");
        }
        return;
    }

    if(strcmp(name, "Entry") == 0) {
        if(ctx->current_group == NULL) {
            flippass_db_set_error(ctx, "The XML entry appeared outside of any group.");
            return;
        }

        if(!flippass_db_prepare_for_arena_alloc(ctx, "entry_alloc", sizeof(KDBXEntry))) {
            return;
        }
        KDBXEntry* entry = kdbx_entry_alloc(ctx->arena);
        if(entry == NULL) {
            FLIPPASS_DB_DEBUG_LOG_RAM(ctx, "entry_alloc", "Entry", sizeof(KDBXEntry));
            flippass_db_set_error(ctx, "Not enough RAM is available to keep this database open.");
            return;
        }

        ctx->entry_count++;
        if(ctx->next_entry_checkpoint == 0U) {
            ctx->next_entry_checkpoint = 16U;
        }
        while(ctx->entry_count >= ctx->next_entry_checkpoint) {
            FLIPPASS_DB_DEBUG_LOG_CHECKPOINT(ctx, "entry_checkpoint");
            ctx->next_entry_checkpoint += 16U;
        }
        entry->next = ctx->current_group->entries;
        ctx->current_group->entries = entry;
        ctx->current_entry = entry;
        ctx->in_entry = true;
        ctx->parsing_depth++;
        if((size_t)ctx->parsing_depth > FLIPPASS_DB_MAX_XML_DEPTH) {
            flippass_db_set_error(ctx, "The XML nesting depth exceeds FlipPass's safe limit.");
        }
        return;
    }

    if(strcmp(name, "AutoType") == 0 && ctx->in_entry) {
        ctx->in_autotype = true;
        return;
    }

    if(strcmp(name, "Name") == 0 && ctx->in_group && !ctx->in_entry) {
        flippass_db_begin_text(ctx, FlipPassDbTextStateGroupName);
        return;
    }

    if(strcmp(name, "UUID") == 0 && ctx->in_group && !ctx->in_entry) {
        flippass_db_begin_text(ctx, FlipPassDbTextStateGroupUuid);
        return;
    }

    if(strcmp(name, "UUID") == 0 && ctx->in_entry) {
        flippass_db_begin_text(ctx, FlipPassDbTextStateEntryUuid);
        return;
    }

    if(strcmp(name, "DefaultSequence") == 0 && ctx->in_entry && ctx->in_autotype) {
        flippass_db_begin_text(ctx, FlipPassDbTextStateAutoTypeSequence);
        return;
    }

    if(strcmp(name, "String") == 0) {
        ctx->in_string = true;
        furi_string_reset(ctx->string_key);
        return;
    }

    if(strcmp(name, "Key") == 0 && ctx->in_entry && ctx->in_string) {
        flippass_db_begin_text(ctx, FlipPassDbTextStateStringKey);
        return;
    }

    if(strcmp(name, "Value") == 0 && ctx->in_entry && ctx->in_string) {
        const char* protected_value = flippass_db_find_attribute(attributes, "Protected");
        ctx->value_protected =
            protected_value != NULL &&
            (strcmp(protected_value, "True") == 0 || strcmp(protected_value, "true") == 0);
        ctx->protected_discard_active = false;
        flippass_db_begin_text(ctx, FlipPassDbTextStateStringValue);
        if(flippass_db_should_stream_string_value(ctx)) {
            if(!flippass_db_begin_streamed_value(ctx, furi_string_get_cstr(ctx->string_key))) {
                return;
            }
            if(ctx->value_protected) {
                kdbx_protected_discard_state_init(&ctx->protected_discard_state);
            }
            return;
        }

        ctx->protected_discard_active =
            ctx->value_protected &&
            !flippass_db_is_supported_string_key(furi_string_get_cstr(ctx->string_key));
        if(ctx->protected_discard_active) {
            kdbx_protected_discard_state_init(&ctx->protected_discard_state);
        }
    }
}

static void flippass_db_end_element(void* context, const char* name) {
    FlipPassDbLoadContext* ctx = context;
    furi_assert(ctx);

    if(ctx->parse_failed) {
        return;
    }

    if(ctx->skipping_history) {
        if(strcmp(name, "Value") == 0 && ctx->history_in_string) {
            if(ctx->protected_discard_active) {
                if(!kdbx_protected_discard_state_finalize(
                       &ctx->protected_stream, &ctx->protected_discard_state)) {
                    flippass_db_set_error(ctx, "A protected entry field could not be decoded.");
                }
                ctx->protected_discard_active = false;
            } else {
                flippass_db_consume_history_protected_value(ctx);
            }
            furi_string_reset(ctx->text_value);
            ctx->history_collect_protected_value = false;
            ctx->history_value_protected = false;
        } else if(strcmp(name, "String") == 0) {
            ctx->history_in_string = false;
            ctx->history_collect_protected_value = false;
            ctx->history_value_protected = false;
            ctx->protected_discard_active = false;
            furi_string_reset(ctx->text_value);
        }

        if(ctx->history_skip_depth > 0U) {
            ctx->history_skip_depth--;
        }

        if(ctx->history_skip_depth == 0U) {
            ctx->skipping_history = false;
            ctx->history_in_string = false;
            ctx->history_collect_protected_value = false;
            ctx->history_value_protected = false;
            ctx->protected_discard_active = false;
            furi_string_reset(ctx->text_value);
        }

        return;
    }

    if(strcmp(name, "Key") == 0 && ctx->text_state == FlipPassDbTextStateStringKey) {
        furi_string_set(ctx->string_key, ctx->text_value);
        furi_string_reset(ctx->text_value);
        ctx->text_state = FlipPassDbTextStateNone;
        return;
    }

    if(strcmp(name, "Value") == 0 && ctx->text_state == FlipPassDbTextStateStringValue) {
        if(ctx->protected_discard_active) {
            if(!kdbx_protected_discard_state_finalize(
                   &ctx->protected_stream, &ctx->protected_discard_state)) {
                flippass_db_set_error(ctx, "A protected entry field could not be decoded.");
            }
            furi_string_reset(ctx->text_value);
            ctx->value_protected = false;
            ctx->protected_discard_active = false;
            ctx->text_state = FlipPassDbTextStateNone;
            return;
        }

        if(ctx->deferred_stream_active) {
            if(ctx->value_protected &&
               !kdbx_protected_decode_state_finalize(
                   &ctx->protected_stream,
                   &ctx->protected_discard_state,
                   flippass_db_write_streamed_protected_chunk,
                   ctx)) {
                kdbx_vault_writer_abort(&ctx->field_writer);
                ctx->deferred_stream_active = false;
                ctx->deferred_stream_plain_bytes = 0U;
                ctx->deferred_stream_logged_large = false;
                flippass_db_set_error(ctx, "A protected entry field could not be decoded.");
            }

            if(!ctx->parse_failed) {
                flippass_db_commit_streamed_value(
                    ctx, ctx->current_entry, furi_string_get_cstr(ctx->string_key));
            }
        } else {
            const char* text = furi_string_get_cstr(ctx->text_value);
            flippass_db_commit_text_value(
                ctx, ctx->text_state, text, furi_string_size(ctx->text_value));
        }
        furi_string_reset(ctx->text_value);
        ctx->value_protected = false;
        ctx->protected_discard_active = false;
        ctx->text_state = FlipPassDbTextStateNone;
        return;
    }

    if(((strcmp(name, "Name") == 0) && ctx->text_state == FlipPassDbTextStateGroupName) ||
       ((strcmp(name, "UUID") == 0) && ctx->text_state == FlipPassDbTextStateGroupUuid) ||
       ((strcmp(name, "UUID") == 0) && ctx->text_state == FlipPassDbTextStateEntryUuid) ||
       ((strcmp(name, "DefaultSequence") == 0) &&
        ctx->text_state == FlipPassDbTextStateAutoTypeSequence)) {
        const char* text = furi_string_get_cstr(ctx->text_value);
        flippass_db_commit_text_value(ctx, ctx->text_state, text, furi_string_size(ctx->text_value));
        furi_string_reset(ctx->text_value);
        ctx->text_state = FlipPassDbTextStateNone;
    }

    if(strcmp(name, "String") == 0) {
        ctx->in_string = false;
        return;
    }

    if(strcmp(name, "AutoType") == 0) {
        ctx->in_autotype = false;
        return;
    }

    if(strcmp(name, "Entry") == 0) {
        if(ctx->app != NULL && ctx->entry_count > 0U &&
           (ctx->entry_count >= 24U || (ctx->entry_count % 8U) == 0U)) {
            FLIPPASS_DEBUG_EVENT(
                ctx->app,
                "ENTRY_PROGRESS entries=%lu groups=%lu records=%lu xml=%lu",
                (unsigned long)ctx->entry_count,
                (unsigned long)ctx->group_count,
                (unsigned long)kdbx_vault_record_count(ctx->vault),
                (unsigned long)ctx->xml_bytes);
        }
        ctx->current_entry = NULL;
        ctx->in_entry = false;
        if(ctx->parsing_depth > 0) {
            ctx->parsing_depth--;
        }
        return;
    }

    if(strcmp(name, "Group") == 0) {
        if(ctx->current_group != NULL) {
            ctx->current_group = ctx->current_group->parent;
        }
        ctx->in_group = (ctx->current_group != NULL);
        if(ctx->parsing_depth > 0) {
            ctx->parsing_depth--;
        }
    }
}

static void flippass_db_character_data(void* context, const char* data, int len) {
    FlipPassDbLoadContext* ctx = context;
    furi_assert(ctx);

    if(ctx->parse_failed || data == NULL || len <= 0) {
        return;
    }

    if(ctx->skipping_history) {
        if(!ctx->history_collect_protected_value) {
            return;
        }

        if(ctx->protected_discard_active) {
            if(!kdbx_protected_discard_state_update(
                   &ctx->protected_stream, &ctx->protected_discard_state, data, (size_t)len)) {
                flippass_db_set_error(ctx, "A protected entry field could not be decoded.");
            }
            return;
        }

        flippass_db_append_text_segment(ctx, data, len);
        return;
    }

    if(ctx->text_state == FlipPassDbTextStateNone) {
        return;
    }

    if(ctx->text_state == FlipPassDbTextStateStringValue && !ctx->value_protected &&
       !flippass_db_is_supported_string_key(furi_string_get_cstr(ctx->string_key))) {
        return;
    }

    if(ctx->text_state == FlipPassDbTextStateStringValue && ctx->deferred_stream_active) {
        if(ctx->value_protected) {
            if(!kdbx_protected_decode_state_update(
                   &ctx->protected_stream,
                   &ctx->protected_discard_state,
                   data,
                   (size_t)len,
                   flippass_db_write_streamed_protected_chunk,
                   ctx)) {
                flippass_db_set_error(ctx, "A protected entry field could not be decoded.");
            }
        } else if(!flippass_db_write_streamed_value_chunk(
                      ctx, furi_string_get_cstr(ctx->string_key), data, (size_t)len)) {
            return;
        }
        return;
    }

    if(ctx->text_state == FlipPassDbTextStateStringValue && ctx->protected_discard_active) {
        if(!kdbx_protected_discard_state_update(
               &ctx->protected_stream, &ctx->protected_discard_state, data, (size_t)len)) {
            flippass_db_set_error(ctx, "A protected entry field could not be decoded.");
        }
        return;
    }

    flippass_db_append_text_segment(ctx, data, len);
}

static bool flippass_db_finish_inner_header(FlipPassDbLoadContext* ctx) {
    if(ctx->protected_stream_id != KDBXProtectedStreamNone) {
        if(ctx->protected_stream_key.size == 0U) {
            flippass_db_set_error(ctx, "The KDBX inner protected-value key is missing.");
            return false;
        }

        if(!kdbx_protected_stream_init(
               &ctx->protected_stream,
               (KDBXProtectedStreamAlgorithm)ctx->protected_stream_id,
               ctx->protected_stream_key.data,
               ctx->protected_stream_key.size)) {
            flippass_db_set_error(
                ctx, "Only Salsa20 or ChaCha20 protected values are supported.");
            return false;
        }
    }

    ctx->inner_header_done = true;
    return true;
}

static bool flippass_db_consume_inner_header(
    FlipPassDbLoadContext* ctx,
    const uint8_t* data,
    size_t data_size,
    size_t* consumed) {
    furi_assert(ctx);
    furi_assert(consumed);

    *consumed = 0U;

    while(*consumed < data_size && !ctx->inner_header_done && !ctx->parse_failed) {
        if(ctx->inner_header_prefix_len == 0U && ctx->inner_field_remaining == 0U &&
           ctx->inner_field_id == 0U &&
           (data[*consumed] == '<' || data[*consumed] == 0xEFU)) {
            return flippass_db_finish_inner_header(ctx);
        }

        if(ctx->inner_field_remaining == 0U &&
           ctx->inner_header_prefix_len < sizeof(ctx->inner_header_prefix)) {
            ctx->inner_header_prefix[ctx->inner_header_prefix_len++] = data[*consumed];
            (*consumed)++;

            if(ctx->inner_header_prefix_len < sizeof(ctx->inner_header_prefix)) {
                continue;
            }

            ctx->inner_field_id = ctx->inner_header_prefix[0];
            ctx->inner_field_size =
                ((uint32_t)ctx->inner_header_prefix[1]) |
                ((uint32_t)ctx->inner_header_prefix[2] << 8) |
                ((uint32_t)ctx->inner_header_prefix[3] << 16) |
                ((uint32_t)ctx->inner_header_prefix[4] << 24);
            ctx->inner_field_remaining = ctx->inner_field_size;
            ctx->inner_header_prefix_len = 0U;
            if(ctx->inner_field_id == 1U) {
                ctx->protected_stream_id = 0U;
            } else if(ctx->inner_field_id == 2U) {
                flippass_db_byte_buffer_free(&ctx->protected_stream_key);
            }
        }

        const size_t available = data_size - *consumed;
        const size_t take =
            (available < ctx->inner_field_remaining) ? available : ctx->inner_field_remaining;

        if(ctx->inner_field_id == 2U && take > 0U &&
           !flippass_db_byte_buffer_append(&ctx->protected_stream_key, data + *consumed, take)) {
            flippass_db_set_error(ctx, "Not enough RAM is available to keep this database open.");
            return false;
        }

        if(ctx->inner_field_id == 1U && ctx->inner_field_size == 4U && take > 0U) {
            const size_t field_offset = ctx->inner_field_size - ctx->inner_field_remaining;
            for(size_t index = 0U; index < take; ++index) {
                ctx->protected_stream_id |=
                    ((uint32_t)data[*consumed + index]) << ((field_offset + index) * 8U);
            }
        }

        *consumed += take;
        ctx->inner_field_remaining -= take;

        if(ctx->inner_field_remaining != 0U) {
            continue;
        }

        if(ctx->inner_field_id == 0U) {
            return flippass_db_finish_inner_header(ctx);
        }

        ctx->inner_field_id = 0U;
        ctx->inner_field_size = 0U;
    }

    return !ctx->parse_failed;
}

static bool flippass_db_payload_chunk_callback(const uint8_t* data, size_t data_size, void* context) {
    FlipPassDbLoadContext* ctx = context;
    size_t consumed = 0U;

    furi_assert(ctx);

    if(data == NULL) {
        return data_size == 0U;
    }

    while(consumed < data_size) {
        if(!ctx->inner_header_done) {
            size_t inner_consumed = 0U;
            if(!flippass_db_consume_inner_header(
                   ctx, data + consumed, data_size - consumed, &inner_consumed)) {
                return false;
            }
            consumed += inner_consumed;
            continue;
        }

        const size_t xml_chunk = data_size - consumed;
        if(ctx->xml_bytes > (FLIPPASS_DB_MAX_XML_STREAM_BYTES - xml_chunk)) {
            flippass_db_set_error(ctx, "The XML payload exceeds FlipPass's streaming limit.");
            return false;
        }

        ctx->xml_bytes += xml_chunk;
        const bool first_xml_chunk = (ctx->xml_bytes == xml_chunk);
        if(first_xml_chunk) {
            flippass_db_progress_update(
                ctx->app,
                "Modeling",
                "",
                ctx->xml_total_bytes_hint > 0U ? 82U : 70U);
            flippass_log_event(ctx->app, "XML_STREAM_FIRST_CHUNK size=%lu", (unsigned long)xml_chunk);
        }
        if(ctx->xml_total_bytes_hint > 0U) {
            uint8_t percent =
                (uint8_t)(82U + ((ctx->xml_bytes * 16U) / ctx->xml_total_bytes_hint));
            if(percent > 98U) {
                percent = 98U;
            }
            if(percent > ctx->app->progress_percent) {
                flippass_db_progress_update(ctx->app, "Modeling", "", percent);
            }
        }
        if(!xml_parser_feed(ctx->xml_parser, (const char*)(data + consumed), xml_chunk, false)) {
            flippass_db_set_error(
                ctx,
                "%s",
                xml_parser_get_last_error(ctx->xml_parser) != NULL ?
                    xml_parser_get_last_error(ctx->xml_parser) :
                    "The XML payload could not be parsed.");
            return false;
        }
        if(first_xml_chunk) {
            flippass_log_event(
                ctx->app,
                "XML_STREAM_FIRST_CHUNK_OK total=%lu stack=%lu",
                (unsigned long)ctx->xml_bytes,
                (unsigned long)furi_thread_get_stack_space(furi_thread_get_current_id()));
        }
        consumed += xml_chunk;
    }

    return !ctx->parse_failed;
}

static void flippass_db_context_cleanup(FlipPassDbLoadContext* ctx) {
    if(ctx == NULL) {
        return;
    }

    if(ctx->field_writer.pending != NULL) {
        kdbx_vault_writer_abort(&ctx->field_writer);
    }

    if(ctx->root_group != NULL) {
        kdbx_group_free(ctx->root_group);
        ctx->root_group = NULL;
    }
    if(ctx->vault != NULL) {
        kdbx_vault_free(ctx->vault);
        ctx->vault = NULL;
    }
    if(ctx->arena != NULL) {
        kdbx_arena_free(ctx->arena);
        ctx->arena = NULL;
    }
    if(ctx->xml_parser != NULL) {
        xml_parser_free(ctx->xml_parser);
        ctx->xml_parser = NULL;
    }
    if(ctx->text_value != NULL) {
        furi_string_free(ctx->text_value);
        ctx->text_value = NULL;
    }
    if(ctx->string_key != NULL) {
        furi_string_free(ctx->string_key);
        ctx->string_key = NULL;
    }

    flippass_db_byte_buffer_free(&ctx->protected_stream_key);
    flippass_db_byte_buffer_free(&ctx->protected_value_buffer);
    kdbx_protected_stream_reset(&ctx->protected_stream);
}

#if FLIPPASS_ENABLE_GZIP_PAGED_TRACE
static void flippass_db_gzip_trace_store(
    FlipPassDbGzipTraceContext* trace,
    const char* format,
    ...) {
    if(trace == NULL || format == NULL) {
        return;
    }

    const size_t slot =
        (trace->stored_count < FLIPPASS_DB_GZIP_TRACE_EVENT_LIMIT) ? trace->stored_count :
                                                                     trace->next_index;

    va_list args;
    va_start(args, format);
    vsnprintf(
        trace->events[slot],
        FLIPPASS_DB_GZIP_TRACE_TEXT_LIMIT,
        format,
        args);
    va_end(args);
    if(trace->stored_count < FLIPPASS_DB_GZIP_TRACE_EVENT_LIMIT) {
        trace->stored_count++;
    } else {
        trace->dropped_count++;
    }
    trace->next_index = (slot + 1U) % FLIPPASS_DB_GZIP_TRACE_EVENT_LIMIT;
}
#else
static void flippass_db_gzip_trace_store(
    FlipPassDbGzipTraceContext* trace,
    const char* format,
    ...) {
    UNUSED(trace);
    UNUSED(format);
}
#endif

static bool flippass_db_gzip_scratch_write(
    const uint8_t* data,
    size_t data_size,
    void* context) {
    FlipPassDbScratchWriteContext* scratch = context;
    furi_assert(scratch);

    if(data == NULL) {
        return data_size == 0U;
    }

    if(data_size == 0U) {
        return true;
    }

    if(!flippass_db_gzip_scratch_ensure_writer(scratch)) {
        scratch->failed = true;
        return false;
    }

    scratch->chunk_count++;
    if(scratch->chunk_count <= 2U || (scratch->chunk_count % 64U) == 0U) {
        scratch->checkpoint_count++;
        scratch->checkpoint_chunk_index = scratch->chunk_count;
        scratch->checkpoint_chunk_size = data_size;
        scratch->checkpoint_free_heap = memmgr_get_free_heap();
        scratch->checkpoint_max_free_block = memmgr_heap_get_max_free_block();
        scratch->checkpoint_plain_bytes = scratch->plain_bytes;
        scratch->checkpoint_record_count = kdbx_vault_record_count(scratch->writer.vault);
    }

    if(!kdbx_vault_writer_write(&scratch->writer, data, data_size)) {
        scratch->failed = true;
        flippass_db_gzip_trace_store(
            scratch->trace,
            "GZIP_STAGE_CHUNK_FAIL index=%lu size=%lu free=%lu max=%lu records=%lu vault_fail=%s storage_stage=%s",
            (unsigned long)scratch->chunk_count,
            (unsigned long)data_size,
            (unsigned long)memmgr_get_free_heap(),
            (unsigned long)memmgr_heap_get_max_free_block(),
            (unsigned long)kdbx_vault_record_count(scratch->writer.vault),
            kdbx_vault_failure_reason(scratch->writer.vault),
            kdbx_vault_storage_stage(scratch->writer.vault));
        return false;
    }

    scratch->plain_bytes += data_size;
    if(scratch->chunk_count <= 2U || (scratch->chunk_count % 64U) == 0U) {
        scratch->checkpoint_plain_bytes = scratch->plain_bytes;
        scratch->checkpoint_record_count = kdbx_vault_record_count(scratch->writer.vault);
    }
    return true;
}

static bool flippass_db_gzip_scratch_ensure_writer(FlipPassDbScratchWriteContext* scratch) {
    furi_assert(scratch);

    if(scratch->writer.vault != NULL) {
        return !scratch->writer.failed;
    }

    if(scratch->vault_slot == NULL) {
        scratch->alloc_failed = true;
        return false;
    }

    *scratch->vault_slot =
        scratch->path == NULL ? kdbx_vault_alloc(scratch->backend, NULL, 0U) :
                                kdbx_vault_alloc_with_path(scratch->backend, scratch->path, NULL, 0U);
    if(*scratch->vault_slot == NULL) {
        scratch->alloc_failed = true;
        return false;
    }

    if(kdbx_vault_storage_failed(*scratch->vault_slot)) {
        scratch->storage_failed = true;
        return false;
    }

    kdbx_vault_writer_reset(&scratch->writer, *scratch->vault_slot);
    kdbx_vault_writer_set_file_streaming(&scratch->writer, true);
    if(scratch->writer.failed) {
        scratch->alloc_failed = true;
        return false;
    }

    flippass_db_gzip_trace_store(
        scratch->trace,
        "GZIP_STAGE_OUTPUT_ALLOC backend=%s free=%lu max=%lu",
        kdbx_vault_backend_label(kdbx_vault_get_backend(*scratch->vault_slot)),
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());

    return true;
}

static bool flippass_db_gzip_member_prepare_spill(FlipPassDbMemberCollectContext* collect) {
    furi_assert(collect);

    if(collect->spill_vault == NULL) {
        collect->spill_vault =
            collect->spill_path == NULL ?
                kdbx_vault_alloc(collect->spill_backend, NULL, 0U) :
                kdbx_vault_alloc_with_path(collect->spill_backend, collect->spill_path, NULL, 0U);
        if(collect->spill_vault == NULL || kdbx_vault_storage_failed(collect->spill_vault)) {
            return false;
        }

        kdbx_vault_writer_reset(&collect->writer, collect->spill_vault);
        kdbx_vault_writer_set_file_streaming(&collect->writer, true);
    }

    if(collect->ram_buffer.size > 0U) {
        if(!kdbx_vault_writer_write(
               &collect->writer, collect->ram_buffer.data, collect->ram_buffer.size)) {
            return false;
        }
        flippass_db_byte_buffer_free(&collect->ram_buffer);
    }

    return true;
}

static bool flippass_db_gzip_member_collect(
    const uint8_t* data,
    size_t data_size,
    void* context) {
    FlipPassDbMemberCollectContext* collect = context;
    furi_assert(collect);

    if(data == NULL) {
        return data_size == 0U;
    }

    if(data_size == 0U) {
        return true;
    }

    collect->chunk_count++;
    if(collect->chunk_count == 1U && collect->app != NULL) {
        FLIPPASS_VERBOSE_LOG(
            collect->app,
            "GZIP_STAGE_MEMBER_FIRST_CHUNK size=%lu free=%lu max=%lu",
            (unsigned long)data_size,
            (unsigned long)memmgr_get_free_heap(),
            (unsigned long)memmgr_heap_get_max_free_block());
    }
    if(collect->sample_count < FLIPPASS_DB_GZIP_MEMBER_SAMPLE_COUNT) {
        const size_t sample_index = collect->sample_count++;
        collect->sample_sizes[sample_index] =
            (data_size > UINT16_MAX) ? UINT16_MAX : (uint16_t)data_size;
    }

    if(collect->prefix_len < sizeof(collect->prefix)) {
        const size_t copy =
            (sizeof(collect->prefix) - collect->prefix_len) < data_size ?
                (sizeof(collect->prefix) - collect->prefix_len) :
                data_size;
        memcpy(collect->prefix + collect->prefix_len, data, copy);
        collect->prefix_len += copy;
    }

    if(data_size >= sizeof(collect->trailer)) {
        memcpy(collect->trailer, data + data_size - sizeof(collect->trailer), sizeof(collect->trailer));
        collect->trailer_len = sizeof(collect->trailer);
    } else {
        const size_t keep =
            (collect->trailer_len + data_size > sizeof(collect->trailer)) ?
                (sizeof(collect->trailer) - data_size) :
                collect->trailer_len;
        if(keep > 0U) {
            memmove(
                collect->trailer,
                collect->trailer + (collect->trailer_len - keep),
                keep);
        }
        memcpy(collect->trailer + keep, data, data_size);
        collect->trailer_len = keep + data_size;
    }

    const size_t next_total_bytes = collect->total_bytes + data_size;
    const size_t next_capacity = flippass_db_byte_buffer_quantized_capacity(
        collect->ram_buffer.size + data_size, 1024U);
    const bool keep_in_ram =
        collect->spill_vault == NULL &&
        next_total_bytes <= FLIPPASS_DB_GZIP_MEMBER_RAM_LIMIT &&
        next_capacity > 0U &&
        memmgr_heap_get_max_free_block() >= next_capacity;
    if(keep_in_ram) {
        if(!flippass_db_byte_buffer_append_quantized(&collect->ram_buffer, data, data_size, 1024U)) {
            collect->failure_chunk_index = collect->chunk_count;
            collect->failure_chunk_size = data_size;
            collect->failure_free_heap = memmgr_get_free_heap();
            collect->failure_max_free_block = memmgr_heap_get_max_free_block();
            collect->failed = true;
            return false;
        }
    } else {
        if(collect->spill_vault == NULL) {
            collect->spill_started = true;
            collect->spill_started_at_bytes = collect->total_bytes;
            if(collect->app != NULL) {
                FLIPPASS_VERBOSE_LOG(
                    collect->app,
                    "GZIP_STAGE_MEMBER_SPILL_BEGIN at=%lu next=%lu free=%lu max=%lu",
                    (unsigned long)collect->total_bytes,
                    (unsigned long)next_total_bytes,
                    (unsigned long)memmgr_get_free_heap(),
                    (unsigned long)memmgr_heap_get_max_free_block());
            }

            if(!flippass_db_gzip_member_prepare_spill(collect)) {
                collect->failure_chunk_index = collect->chunk_count;
                collect->failure_chunk_size = data_size;
                collect->failure_free_heap = memmgr_get_free_heap();
                collect->failure_max_free_block = memmgr_heap_get_max_free_block();
                collect->failed = true;
                return false;
            }
        }

        if(!kdbx_vault_writer_write(&collect->writer, data, data_size)) {
            collect->failure_chunk_index = collect->chunk_count;
            collect->failure_chunk_size = data_size;
            collect->failure_free_heap = memmgr_get_free_heap();
            collect->failure_max_free_block = memmgr_heap_get_max_free_block();
            collect->failed = true;
            return false;
        }
    }

    collect->total_bytes += data_size;
    return true;
}

#if FLIPPASS_ENABLE_GZIP_PAGED_TRACE
static void flippass_db_gzip_member_log_summary(
    App* app,
    const char* label,
    const FlipPassDbMemberCollectContext* collect) {
    char sample_text[96];
    size_t cursor = 0U;

    furi_assert(label);
    furi_assert(collect);

    if(app == NULL) {
        return;
    }

    sample_text[0] = '\0';
    for(size_t index = 0U; index < collect->sample_count && cursor < sizeof(sample_text); index++) {
        const int written = snprintf(
            sample_text + cursor,
            sizeof(sample_text) - cursor,
            "%s%u",
            index == 0U ? "" : ",",
            (unsigned)collect->sample_sizes[index]);
        if(written <= 0) {
            break;
        }
        if((size_t)written >= (sizeof(sample_text) - cursor)) {
            cursor = sizeof(sample_text) - 1U;
            break;
        }
        cursor += (size_t)written;
    }

    flippass_log_event(
        app,
        "%s chunks=%lu bytes=%lu spill=%u spill_at=%lu failed=%u fail_chunk=%lu fail_size=%lu fail_free=%lu fail_max=%lu samples=%s",
        label,
        (unsigned long)collect->chunk_count,
        (unsigned long)collect->total_bytes,
        collect->spill_started ? 1U : 0U,
        (unsigned long)collect->spill_started_at_bytes,
        collect->failed ? 1U : 0U,
        (unsigned long)collect->failure_chunk_index,
        (unsigned long)collect->failure_chunk_size,
        (unsigned long)collect->failure_free_heap,
        (unsigned long)collect->failure_max_free_block,
        sample_text[0] != '\0' ? sample_text : "-");
}
#else
static void flippass_db_gzip_member_log_summary(
    App* app,
    const char* label,
    const FlipPassDbMemberCollectContext* collect) {
    UNUSED(app);
    UNUSED(label);
    UNUSED(collect);
}
#endif

static bool flippass_db_gzip_member_read_prefix(
    KDBXVault* vault,
    const KDBXFieldRef* ref,
    uint8_t* prefix,
    size_t prefix_size,
    size_t* out_size) {
    KDBXVaultReader reader;
    size_t total = 0U;

    furi_assert(prefix);
    furi_assert(out_size);

    *out_size = 0U;
    if(vault == NULL || ref == NULL) {
        return false;
    }

    kdbx_vault_reader_reset(&reader, vault, ref);
    while(total < prefix_size) {
        size_t chunk_size = 0U;
        if(!kdbx_vault_reader_read(&reader, prefix + total, prefix_size - total, &chunk_size)) {
            return false;
        }
        if(chunk_size == 0U) {
            break;
        }
        total += chunk_size;
    }

    *out_size = total;
    return true;
}

static bool flippass_db_gzip_member_parse_info(
    KDBXVault* member_vault,
    const KDBXFieldRef* member_ref,
    const FlipPassDbMemberCollectContext* collect,
    KDBXGzipTelemetry* telemetry,
    KDBXGzipMemberInfo* out_info) {
    uint8_t* expanded_prefix = NULL;
    size_t expanded_size = 0U;
    bool parsed = false;

    furi_assert(collect);

    parsed = kdbx_gzip_parse_member_info(
        collect->prefix,
        collect->prefix_len,
        collect->trailer,
        collect->total_bytes,
        FLIPPASS_DB_MAX_XML_STREAM_BYTES,
        telemetry,
        out_info);
    if(parsed) {
        return true;
    }

    if(collect->total_bytes <= collect->prefix_len ||
       collect->total_bytes <= sizeof(collect->trailer) ||
       member_vault == NULL ||
       member_ref == NULL) {
        return false;
    }

    if(
        telemetry == NULL ||
        (telemetry->status != KDBXGzipStatusTruncatedInput &&
         telemetry->status != KDBXGzipStatusInvalidExtraField &&
         telemetry->status != KDBXGzipStatusInvalidNameField &&
         telemetry->status != KDBXGzipStatusInvalidCommentField &&
         telemetry->status != KDBXGzipStatusInvalidHeaderCrcField)) {
        return false;
    }

    expanded_size = collect->total_bytes - sizeof(collect->trailer);
    if(expanded_size > (16U * 1024U)) {
        expanded_size = 16U * 1024U;
    }
    if(expanded_size <= collect->prefix_len) {
        return false;
    }

    expanded_prefix = malloc(expanded_size);
    if(expanded_prefix == NULL) {
        return false;
    }

    if(!flippass_db_gzip_member_read_prefix(
           member_vault, member_ref, expanded_prefix, expanded_size, &expanded_size)) {
        memzero(expanded_prefix, expanded_size);
        free(expanded_prefix);
        return false;
    }

    parsed = kdbx_gzip_parse_member_info(
        expanded_prefix,
        expanded_size,
        collect->trailer,
        collect->total_bytes,
        FLIPPASS_DB_MAX_XML_STREAM_BYTES,
        telemetry,
        out_info);
    memzero(expanded_prefix, expanded_size);
    free(expanded_prefix);
    return parsed;
}

#if FLIPPASS_ENABLE_GZIP_PAGED_TRACE
#if defined(__GNUC__) || defined(__clang__)
__attribute__((unused))
#endif
static void flippass_db_gzip_trace(
    const char* event,
    const KDBXGzipTelemetry* telemetry,
    void* context) {
    FlipPassDbGzipTraceContext* trace = context;
    if(trace == NULL || trace->app == NULL || event == NULL || telemetry == NULL) {
        return;
    }

    trace->event_count++;
    if(trace->event_count > 24U && strcmp(event, "progress") == 0) {
        return;
    }

    const bool switch_to_buffer_only =
        strcmp(event, "memory_file_attempt") == 0 || strcmp(event, "vault_file_attempt") == 0 ||
        strcmp(event, "memory_file_return") == 0 || strcmp(event, "vault_file_return") == 0;
    const bool keep_buffered_event =
        strcmp(event, "vault_file_probe_fail") == 0 || strcmp(event, "vault_file_return") == 0 ||
        strcmp(event, "memory_file_probe_fail") == 0 ||
        strcmp(event, "memory_file_return") == 0 || strcmp(event, "file_storage_failed") == 0 ||
        strcmp(event, "inflate_failed") == 0 || strcmp(event, "flush_rejected") == 0 ||
        strcmp(event, "no_progress") == 0 || strcmp(event, "done") == 0;

    if(
        !trace->buffer_only &&
        (strcmp(event, "vault_memory_attempt") == 0 ||
         strcmp(event, "vault_memory_return") == 0 ||
         strcmp(event, "vault_paged_attempt") == 0 ||
         strcmp(event, "vault_paged_return") == 0 ||
         strcmp(event, "vault_file_preferred") == 0 || strcmp(event, "vault_file_attempt") == 0 ||
         strcmp(event, "vault_file_probe_begin") == 0 ||
         strcmp(event, "vault_file_probe_ok") == 0 ||
         strcmp(event, "vault_file_workspace_ok") == 0 ||
         strcmp(event, "vault_file_workspace_none") == 0 ||
         strcmp(event, "vault_file_probe_fail") == 0 ||
         strcmp(event, "vault_file_return") == 0 ||
         strcmp(event, "window_storage_reuse") == 0 ||
         strcmp(event, "window_storage_open") == 0 ||
         strcmp(event, "window_cache_attempt") == 0 || strcmp(event, "window_cache_ok") == 0 ||
         strcmp(event, "window_mkdir_ok") == 0 || strcmp(event, "window_cleanup_ok") == 0 ||
         strcmp(event, "window_file_alloc_ok") == 0 ||
         strcmp(event, "window_open_create_ok") == 0 || strcmp(event, "window_keys_ok") == 0 ||
         strcmp(event, "file_probe_enter") == 0 || strcmp(event, "file_probe_stack_ok") == 0 ||
         strcmp(event, "file_probe_decomp_stack") == 0 ||
         strcmp(event, "file_probe_decomp_heap") == 0 ||
         strcmp(event, "file_decomp_external") == 0 ||
         strcmp(event, "file_probe_decomp_ok") == 0 ||
         strcmp(event, "file_probe_input_ok") == 0 ||
         strcmp(event, "file_probe_dict_ok") == 0 ||
         strcmp(event, "file_probe_window_begin") == 0 ||
         strcmp(event, "file_probe_window_fail") == 0 ||
         strcmp(event, "file_probe_window_ok") == 0 ||
         strcmp(event, "file_alloc_failed") == 0 || strcmp(event, "file_alloc_ok") == 0 ||
         strcmp(event, "file_dict_attempt") == 0 ||
         strcmp(event, "file_dict_alloc_fail") == 0 ||
         strcmp(event, "file_dict_stack") == 0 || strcmp(event, "file_dict_heap") == 0 ||
         strcmp(event, "file_dict_config_begin") == 0 || strcmp(event, "file_begin") == 0 ||
         strcmp(event, "file_input_request") == 0 || strcmp(event, "file_input_ready") == 0 ||
         strcmp(event, "file_input_eof") == 0 || strcmp(event, "file_first_call") == 0 ||
         strcmp(event, "file_call_begin") == 0 || strcmp(event, "file_call_return") == 0 ||
         strcmp(event, "acquire_begin") == 0 || strcmp(event, "acquire_hit") == 0 ||
         strcmp(event, "acquire_miss") == 0 || strcmp(event, "dict_set") == 0 ||
         strcmp(event, "dict_get") == 0 || strcmp(event, "dict_write") == 0 ||
         strcmp(event, "dict_flush") == 0 || strcmp(event, "page_read_zero") == 0 ||
         strcmp(event, "page_read_begin") == 0 || strcmp(event, "page_read_ok") == 0 ||
         strcmp(event, "page_write_begin") == 0 || strcmp(event, "page_write_ok") == 0 ||
         strcmp(event, "file_storage_failed") == 0 || strcmp(event, "inflate_failed") == 0 ||
         strcmp(event, "flush_rejected") == 0 || strcmp(event, "no_progress") == 0 ||
         strcmp(event, "done") == 0 || strcmp(event, "memory_file_attempt") == 0 ||
         strcmp(event, "memory_file_probe_begin") == 0 ||
         strcmp(event, "memory_file_probe_ok") == 0 ||
         strcmp(event, "memory_file_workspace_ok") == 0 ||
         strcmp(event, "memory_file_workspace_none") == 0 ||
         strcmp(event, "memory_file_probe_fail") == 0 ||
         strcmp(event, "memory_file_return") == 0 ||
         strcmp(event, "file_decomp_stack") == 0 || strcmp(event, "file_decomp_heap") == 0)) {
        flippass_log_event(
            trace->app,
            "GZIP_STAGE_PAGED_TRACE event=%s count=%lu output=%lu input=%lu free=%lu max=%lu pages=%lu fail_page=%lu stage=%s loops=%lu flushes=%lu yields=%lu last_in=%lu last_out=%lu last_dict=%lu last_status=%d timed_out=%u",
            event,
            (unsigned long)trace->event_count,
            (unsigned long)telemetry->actual_output_size,
            (unsigned long)telemetry->consumed_input_size,
            (unsigned long)telemetry->free_heap,
            (unsigned long)telemetry->max_free_block,
            (unsigned long)telemetry->workspace_pages_allocated,
            (unsigned long)telemetry->workspace_failed_page_index,
            telemetry->workspace_storage_stage != NULL ? telemetry->workspace_storage_stage : "-",
            (unsigned long)telemetry->paged_loop_count,
            (unsigned long)telemetry->paged_flush_count,
            (unsigned long)telemetry->paged_yield_count,
            (unsigned long)telemetry->paged_last_input_advance,
            (unsigned long)telemetry->paged_last_output_advance,
            (unsigned long)telemetry->paged_last_dict_offset,
            telemetry->paged_last_status,
            telemetry->paged_timed_out ? 1U : 0U);
        if(switch_to_buffer_only) {
            trace->buffer_only = true;
        }
        return;
    }

    if(switch_to_buffer_only) {
        trace->buffer_only = true;
    }

    if(trace->buffer_only && !keep_buffered_event && strcmp(event, "progress") == 0) {
        return;
    }

    flippass_db_gzip_trace_store(
        trace,
        "GZIP_STAGE_PAGED_TRACE event=%s count=%lu output=%lu input=%lu free=%lu max=%lu pages=%lu fail_page=%lu stage=%s loops=%lu flushes=%lu yields=%lu last_in=%lu last_out=%lu last_dict=%lu last_status=%d timed_out=%u",
        event,
        (unsigned long)trace->event_count,
        (unsigned long)telemetry->actual_output_size,
        (unsigned long)telemetry->consumed_input_size,
        (unsigned long)telemetry->free_heap,
        (unsigned long)telemetry->max_free_block,
        (unsigned long)telemetry->workspace_pages_allocated,
        (unsigned long)telemetry->workspace_failed_page_index,
        telemetry->workspace_storage_stage != NULL ? telemetry->workspace_storage_stage : "-",
        (unsigned long)telemetry->paged_loop_count,
        (unsigned long)telemetry->paged_flush_count,
        (unsigned long)telemetry->paged_yield_count,
        (unsigned long)telemetry->paged_last_input_advance,
        (unsigned long)telemetry->paged_last_output_advance,
        (unsigned long)telemetry->paged_last_dict_offset,
        telemetry->paged_last_status,
        telemetry->paged_timed_out ? 1U : 0U);
}
#endif

static bool flippass_db_stage_gzip_payload(
    App* app,
    KDBXVaultBackend preferred_backend,
    const uint8_t* cipher_key,
    size_t cipher_key_size,
    const uint8_t* hmac_key,
    size_t hmac_key_size,
    KDBXVault** out_scratch_vault,
    KDBXFieldRef* out_ref,
    size_t* out_plain_size,
    FuriString* error) {
    const KDBXVaultBackend member_backend =
        flippass_db_select_gzip_scratch_backend(preferred_backend);
    const KDBXVaultBackend scratch_backend =
        flippass_db_select_gzip_scratch_backend(preferred_backend);
    KDBXVaultBackend output_backend = scratch_backend;
    const char* output_scratch_path = NULL;
    const char* member_scratch_path = flippass_db_gzip_member_path(member_backend);
    FlipPassDbGzipStageState* stage = NULL;
    FlipPassDbScratchWriteContext* scratch_ctx = NULL;
    FlipPassDbGzipTraceContext* gzip_trace_ctx = NULL;
    FlipPassDbMemberCollectContext* member_collect_ctx = NULL;
    KDBXGzipTelemetry* gzip_telemetry = NULL;
    KDBXGzipTraceConfig* gzip_trace_config = NULL;
    KDBXGzipMemberInfo* member_info = NULL;
    KDBXVault* member_vault = NULL;
    KDBXFieldRef* member_ref = NULL;
    bool success = false;

    furi_assert(app);
    furi_assert(out_scratch_vault);
    furi_assert(out_ref);
    furi_assert(out_plain_size);
    furi_assert(error);

    *out_scratch_vault = NULL;
    memset(out_ref, 0, sizeof(*out_ref));
    *out_plain_size = 0U;
    stage = flippass_db_gzip_stage_state_alloc();
    if(stage == NULL) {
        furi_string_set_str(error, "Not enough RAM is available to prepare GZip staging.");
        return false;
    }
    scratch_ctx = &stage->scratch;
    gzip_trace_ctx = &stage->trace;
    member_collect_ctx = &stage->member;
    gzip_telemetry = &stage->telemetry;
    gzip_trace_config = &stage->trace_config;
    member_info = &stage->member_info;
    member_ref = &stage->member_ref;

    if(scratch_backend == KDBXVaultBackendNone || member_backend == KDBXVaultBackendNone) {
        furi_string_set_str(
            error, "No encrypted storage backend is available for GZip staging.");
        goto cleanup;
    }

    flippass_log_event(
        app,
        "GZIP_STAGE_START member_backend=%s output_backend=%s",
        kdbx_vault_backend_label(member_backend),
        kdbx_vault_backend_label(scratch_backend));
    flippass_db_progress_update(app, "Analyzing GZip", "", 50U);

    FLIPPASS_VERBOSE_LOG(app, "GZIP_STAGE_OUTER_BEGIN");
    member_collect_ctx->app = app;
    member_collect_ctx->spill_backend = member_backend;
    member_collect_ctx->spill_path = member_scratch_path;
    FLIPPASS_VERBOSE_LOG(
        app,
        "GZIP_STAGE_MEMBER_PREP backend=%s limit=%lu free=%lu max=%lu",
        kdbx_vault_backend_label(member_backend),
        (unsigned long)FLIPPASS_DB_GZIP_MEMBER_RAM_LIMIT,
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());
    if(!kdbx_parser_stream_outer_payload(
           app->kdbx_parser,
           cipher_key,
           cipher_key_size,
           hmac_key,
           hmac_key_size,
           flippass_db_gzip_member_collect,
           member_collect_ctx)) {
        flippass_db_gzip_member_log_summary(app, "GZIP_STAGE_OUTER_SUMMARY", member_collect_ctx);
        if(member_collect_ctx->failed) {
            flippass_log_event(
                app,
                "GZIP_STAGE_MEMBER_FAIL chunk=%lu size=%lu free=%lu max=%lu spill=%u vault_fail=%s storage_stage=%s",
                (unsigned long)member_collect_ctx->failure_chunk_index,
                (unsigned long)member_collect_ctx->failure_chunk_size,
                (unsigned long)member_collect_ctx->failure_free_heap,
                (unsigned long)member_collect_ctx->failure_max_free_block,
                member_collect_ctx->spill_started ? 1U : 0U,
                member_collect_ctx->writer.vault != NULL ?
                    kdbx_vault_failure_reason(member_collect_ctx->writer.vault) :
                    "-",
                member_collect_ctx->writer.vault != NULL ?
                    kdbx_vault_storage_stage(member_collect_ctx->writer.vault) :
                    "-");
        }
        if(member_collect_ctx->spill_vault != NULL &&
           kdbx_vault_storage_failed(member_collect_ctx->spill_vault)) {
            furi_string_set_str(
                error, "The encrypted GZip member scratch file could not be created safely.");
        } else if(kdbx_parser_get_last_error(app->kdbx_parser)[0] != '\0') {
            furi_string_set_str(
                error,
                kdbx_parser_get_last_error(app->kdbx_parser));
        } else {
            furi_string_set_str(error, "Not enough RAM is available to stage the GZip member.");
        }
        goto cleanup;
    }
    flippass_log_event(app, "GZIP_STAGE_OUTER_OK");
    flippass_db_gzip_member_log_summary(app, "GZIP_STAGE_OUTER_SUMMARY", member_collect_ctx);
    flippass_db_progress_update(app, "Analyzing GZip", "", 55U);

    member_vault = member_collect_ctx->spill_vault;
    FLIPPASS_VERBOSE_LOG(
        app,
        "GZIP_STAGE_MEMBER_MODE ram=%lu spill=%u spill_records=%lu",
        (unsigned long)member_collect_ctx->ram_buffer.size,
        member_vault != NULL ? 1U : 0U,
        (unsigned long)member_ref->record_count);
    if(member_vault != NULL) {
        FLIPPASS_VERBOSE_LOG(app, "GZIP_STAGE_MEMBER_FINISH_BEGIN");
        if(!kdbx_vault_writer_finish(&member_collect_ctx->writer, member_ref)) {
            furi_string_set_str(
                error, "The encrypted GZip member scratch file could not be finalized safely.");
            goto cleanup;
        }
        FLIPPASS_VERBOSE_LOG(
            app, "GZIP_STAGE_MEMBER_FINISH_OK records=%lu", (unsigned long)member_ref->record_count);
    }

    flippass_log_event(
        app,
        "GZIP_STAGE_MEMBER_OK backend=%s bytes=%lu records=%lu",
        member_vault != NULL ? "encrypted staged member vault" : "RAM collect buffer",
        (unsigned long)member_collect_ctx->total_bytes,
        (unsigned long)member_ref->record_count);

    flippass_log_event(app, "GZIP_STAGE_MEMBER_PARSE_BEGIN");
    const bool member_info_ok =
        member_collect_ctx->trailer_len >= sizeof(member_collect_ctx->trailer) &&
        ((member_collect_ctx->ram_buffer.size > 0U &&
          kdbx_gzip_parse_member_info(
              member_collect_ctx->ram_buffer.data,
              member_collect_ctx->ram_buffer.size,
              member_collect_ctx->ram_buffer.data +
                  (member_collect_ctx->ram_buffer.size - sizeof(member_collect_ctx->trailer)),
              member_collect_ctx->ram_buffer.size,
              FLIPPASS_DB_MAX_XML_STREAM_BYTES,
              gzip_telemetry,
              member_info)) ||
         flippass_db_gzip_member_parse_info(
             member_vault, member_ref, member_collect_ctx, gzip_telemetry, member_info));
    if(!member_info_ok) {
        flippass_db_set_gzip_stage_error(error, gzip_telemetry);
        goto cleanup;
    }
    flippass_log_event(app, "GZIP_STAGE_MEMBER_PARSE_OK");

    FLIPPASS_VERBOSE_LOG(
        app,
        "GZIP_STAGE_MEMBER_INFO body=%lu compressed=%lu expected=%lu",
        (unsigned long)member_info->body_offset,
        (unsigned long)member_info->compressed_size,
        (unsigned long)member_info->expected_output_size);

    output_backend = scratch_backend;
    output_scratch_path = flippass_db_gzip_scratch_path(output_backend);
    FLIPPASS_VERBOSE_LOG(
        app,
        "GZIP_STAGE_OUTPUT backend=%s expected=%lu",
        kdbx_vault_backend_label(output_backend),
        (unsigned long)member_info->expected_output_size);

    kdbx_parser_reset(app->kdbx_parser);
    flippass_log_event(app, "GZIP_STAGE_RESET_OK");

    flippass_log_event(
        app, "GZIP_STAGE_LOAD_OK bytes=%lu", (unsigned long)member_collect_ctx->total_bytes);

    flippass_log_event(
        app,
        "GZIP_STAGE_INFLATE_BEGIN compressed=%lu free=%lu max=%lu stack=%lu",
        (unsigned long)member_info->compressed_size,
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block(),
        (unsigned long)furi_thread_get_stack_space(furi_thread_get_current_id()));
    flippass_db_progress_update(app, "Uncompressing", "", 58U);
    *out_scratch_vault = NULL;
    memset(scratch_ctx, 0, sizeof(*scratch_ctx));
    scratch_ctx->app = app;
    scratch_ctx->backend = output_backend;
    scratch_ctx->path = output_scratch_path;
    scratch_ctx->vault_slot = out_scratch_vault;
    scratch_ctx->trace = gzip_trace_ctx;
    gzip_trace_ctx->app = app;
    gzip_trace_config->callback = flippass_db_gzip_progress_callback;
    gzip_trace_config->context = app;
    gzip_trace_config->interval_bytes =
        (member_info->expected_output_size >= 12U) ? (member_info->expected_output_size / 12U) :
                                                     1U;
    gzip_trace_config->inflate_workspace = NULL;
    gzip_trace_config->prefer_file_paged =
        member_info->expected_output_size > FLIPPASS_DB_GZIP_DICT_RESERVE_BYTES;

    const bool inflate_ok =
        member_collect_ctx->ram_buffer.size > 0U ?
            kdbx_gzip_emit_stream_ex(
                member_collect_ctx->ram_buffer.data,
                member_collect_ctx->ram_buffer.size,
                FLIPPASS_DB_MAX_XML_STREAM_BYTES,
                flippass_db_gzip_scratch_write,
                scratch_ctx,
                gzip_telemetry,
                gzip_trace_config) :
            kdbx_gzip_emit_vault_stream(
                member_vault,
                member_ref,
                member_info,
                FLIPPASS_DB_MAX_XML_STREAM_BYTES,
                flippass_db_gzip_scratch_write,
                scratch_ctx,
                gzip_telemetry,
                gzip_trace_config);
    if(!inflate_ok) {
        if(member_vault != NULL) {
            flippass_log_event(
                app,
                "GZIP_STAGE_MEMBER_READER stage=%s record=%lu",
                kdbx_vault_last_reader_failure(member_vault),
                (unsigned long)kdbx_vault_last_reader_failure_record(member_vault));
        }
        flippass_log_event(
            app,
            "GZIP_STAGE_FAIL_PRE_FLUSH status=%u path=%u exp=%lu out=%lu in=%lu free=%lu max=%lu stack=%lu ws=%lu page=%lu cache=%lu stage=%s loops=%lu flushes=%lu yields=%lu no_prog=%lu ofs=%lu last_in=%lu last_out=%lu last_dict=%lu last_status=%d timed_out=%u",
            (unsigned)gzip_telemetry->status,
            (unsigned)gzip_telemetry->inflate_path,
            (unsigned long)gzip_telemetry->expected_output_size,
            (unsigned long)gzip_telemetry->actual_output_size,
            (unsigned long)gzip_telemetry->consumed_input_size,
            (unsigned long)gzip_telemetry->free_heap,
            (unsigned long)gzip_telemetry->max_free_block,
            (unsigned long)furi_thread_get_stack_space(furi_thread_get_current_id()),
            (unsigned long)gzip_telemetry->workspace_total_size,
            (unsigned long)gzip_telemetry->workspace_page_size,
            (unsigned long)gzip_telemetry->workspace_cache_pages,
            gzip_telemetry->workspace_storage_stage != NULL ? gzip_telemetry->workspace_storage_stage : "-",
            (unsigned long)gzip_telemetry->paged_loop_count,
            (unsigned long)gzip_telemetry->paged_flush_count,
            (unsigned long)gzip_telemetry->paged_yield_count,
            (unsigned long)gzip_telemetry->paged_no_progress_count,
            (unsigned long)gzip_telemetry->paged_input_offset,
            (unsigned long)gzip_telemetry->paged_last_input_advance,
            (unsigned long)gzip_telemetry->paged_last_output_advance,
            (unsigned long)gzip_telemetry->paged_last_dict_offset,
            gzip_telemetry->paged_last_status,
            gzip_telemetry->paged_timed_out ? 1U : 0U);
        flippass_log_event(
            app,
            "GZIP_STAGE_OUTPUT_SUMMARY chunks=%lu plain=%lu checkpoints=%lu last_index=%lu last_size=%lu last_free=%lu last_max=%lu last_plain=%lu last_records=%lu failed=%u alloc_failed=%u storage_failed=%u",
            (unsigned long)scratch_ctx->chunk_count,
            (unsigned long)scratch_ctx->plain_bytes,
            (unsigned long)scratch_ctx->checkpoint_count,
            (unsigned long)scratch_ctx->checkpoint_chunk_index,
            (unsigned long)scratch_ctx->checkpoint_chunk_size,
            (unsigned long)scratch_ctx->checkpoint_free_heap,
            (unsigned long)scratch_ctx->checkpoint_max_free_block,
            (unsigned long)scratch_ctx->checkpoint_plain_bytes,
            (unsigned long)scratch_ctx->checkpoint_record_count,
            scratch_ctx->failed ? 1U : 0U,
            scratch_ctx->alloc_failed ? 1U : 0U,
            scratch_ctx->storage_failed ? 1U : 0U);
        if(scratch_ctx->failed) {
            if(scratch_ctx->storage_failed) {
                furi_string_set_str(
                    error,
                    "The encrypted GZip scratch file could not be created on the selected storage.");
            } else {
                furi_string_set_str(
                    error, "The encrypted GZip scratch file could not be written safely.");
            }
        } else {
            flippass_db_set_gzip_stage_error(error, gzip_telemetry);
        }
        flippass_log_event(
            app,
            "GZIP_STAGE_INFLATE_FAIL status=%u path=%u exp=%lu out=%lu in=%lu free=%lu max=%lu stack=%lu ws=%lu page=%lu cache=%lu stage=%s timeout=%lu pages=%lu fail_page=%lu loops=%lu flushes=%lu yields=%lu no_prog=%lu ofs=%lu last_in=%lu last_out=%lu last_dict=%lu last_status=%d timed_out=%u",
            (unsigned)gzip_telemetry->status,
            (unsigned)gzip_telemetry->inflate_path,
            (unsigned long)gzip_telemetry->expected_output_size,
            (unsigned long)gzip_telemetry->actual_output_size,
            (unsigned long)gzip_telemetry->consumed_input_size,
            (unsigned long)gzip_telemetry->free_heap,
            (unsigned long)gzip_telemetry->max_free_block,
            (unsigned long)furi_thread_get_stack_space(furi_thread_get_current_id()),
            gzip_telemetry->inflate_status,
            (unsigned long)gzip_telemetry->workspace_total_size,
            (unsigned long)gzip_telemetry->workspace_page_size,
            (unsigned long)gzip_telemetry->workspace_cache_pages,
            gzip_telemetry->workspace_storage_stage != NULL ?
                gzip_telemetry->workspace_storage_stage :
                "-",
            (unsigned long)gzip_telemetry->workspace_timeout_ms,
            (unsigned long)gzip_telemetry->workspace_pages_allocated,
            (unsigned long)gzip_telemetry->workspace_failed_page_index,
            (unsigned long)gzip_telemetry->paged_loop_count,
            (unsigned long)gzip_telemetry->paged_flush_count,
            (unsigned long)gzip_telemetry->paged_yield_count,
            (unsigned long)gzip_telemetry->paged_no_progress_count,
            (unsigned long)gzip_telemetry->paged_input_offset,
            (unsigned long)gzip_telemetry->paged_last_input_advance,
            (unsigned long)gzip_telemetry->paged_last_output_advance,
            (unsigned long)gzip_telemetry->paged_last_dict_offset,
            gzip_telemetry->paged_last_status,
             gzip_telemetry->paged_timed_out ? 1U : 0U);
        goto cleanup;
    }

    if(*out_scratch_vault == NULL) {
        furi_string_set_str(error, "The GZip payload did not produce any staged output.");
        goto cleanup;
    }

    if(!kdbx_vault_writer_finish(&scratch_ctx->writer, out_ref)) {
        flippass_log_event(
            app,
            "GZIP_STAGE_OUTPUT_FINISH_FAIL records=%lu vault_fail=%s storage_stage=%s free=%lu max=%lu",
            (unsigned long)(*out_scratch_vault != NULL ? kdbx_vault_record_count(*out_scratch_vault) : 0U),
            *out_scratch_vault != NULL ? kdbx_vault_failure_reason(*out_scratch_vault) : "-",
            *out_scratch_vault != NULL ? kdbx_vault_storage_stage(*out_scratch_vault) : "-",
            (unsigned long)memmgr_get_free_heap(),
            (unsigned long)memmgr_heap_get_max_free_block());
        furi_string_set_str(
            error, "The encrypted GZip scratch file could not be finalized safely.");
        goto cleanup;
    }

    flippass_log_event(app, "GZIP_STAGE_OUTPUT_FINISH_OK");
    flippass_log_event(
        app,
        "GZIP_STAGE_OUTPUT_SUMMARY chunks=%lu plain=%lu checkpoints=%lu last_index=%lu last_size=%lu last_free=%lu last_max=%lu last_plain=%lu last_records=%lu failed=%u alloc_failed=%u storage_failed=%u",
        (unsigned long)scratch_ctx->chunk_count,
        (unsigned long)scratch_ctx->plain_bytes,
        (unsigned long)scratch_ctx->checkpoint_count,
        (unsigned long)scratch_ctx->checkpoint_chunk_index,
        (unsigned long)scratch_ctx->checkpoint_chunk_size,
        (unsigned long)scratch_ctx->checkpoint_free_heap,
        (unsigned long)scratch_ctx->checkpoint_max_free_block,
        (unsigned long)scratch_ctx->checkpoint_plain_bytes,
        (unsigned long)scratch_ctx->checkpoint_record_count,
        scratch_ctx->failed ? 1U : 0U,
        scratch_ctx->alloc_failed ? 1U : 0U,
        scratch_ctx->storage_failed ? 1U : 0U);
    flippass_log_event(app, "GZIP_STAGE_POST_FLUSH");
    flippass_log_event(
        app,
        "GZIP_STAGE_INFLATE_OK path=%u out=%lu in=%lu cache=%lu timeout=%lu pages=%lu loops=%lu flushes=%lu yields=%lu stack=%lu",
        (unsigned)gzip_telemetry->inflate_path,
        (unsigned long)gzip_telemetry->actual_output_size,
        (unsigned long)gzip_telemetry->consumed_input_size,
        (unsigned long)gzip_telemetry->workspace_cache_pages,
        (unsigned long)gzip_telemetry->workspace_timeout_ms,
        (unsigned long)gzip_telemetry->workspace_pages_allocated,
        (unsigned long)gzip_telemetry->paged_loop_count,
        (unsigned long)gzip_telemetry->paged_flush_count,
        (unsigned long)gzip_telemetry->paged_yield_count,
        (unsigned long)furi_thread_get_stack_space(furi_thread_get_current_id()));

    flippass_log_event(
        app,
        "GZIP_STAGE_OK backend=%s bytes=%lu records=%lu",
        kdbx_vault_backend_label(kdbx_vault_get_backend(*out_scratch_vault)),
        (unsigned long)scratch_ctx->plain_bytes,
        (unsigned long)kdbx_vault_record_count(*out_scratch_vault));
    *out_plain_size = scratch_ctx->plain_bytes;
    success = true;

cleanup:
    if(member_collect_ctx != NULL) {
        kdbx_vault_writer_abort(&member_collect_ctx->writer);
        flippass_db_byte_buffer_free(&member_collect_ctx->ram_buffer);
    }
    if(scratch_ctx != NULL) {
        kdbx_vault_writer_abort(&scratch_ctx->writer);
    }
    if(member_vault != NULL) {
        kdbx_vault_free(member_vault);
        member_vault = NULL;
    }
    if(!success && *out_scratch_vault != NULL) {
        kdbx_vault_free(*out_scratch_vault);
        *out_scratch_vault = NULL;
        memset(out_ref, 0, sizeof(*out_ref));
    }
    flippass_db_gzip_stage_state_free(stage);
    return success;
}

static void flippass_db_prepare_fallback_message(
    FlipPassDbLoadContext* ctx,
    const char* stage,
    size_t request_size) {
    const size_t remaining_budget =
        (ctx != NULL && ctx->commit_limit > ctx->committed_bytes) ?
            (ctx->commit_limit - ctx->committed_bytes) :
            0U;
    const size_t max_free_block = memmgr_heap_get_max_free_block();

    if(ctx == NULL) {
        return;
    }

    if(ctx->app == NULL || ctx->vault == NULL ||
       kdbx_vault_get_backend(ctx->vault) != KDBXVaultBackendRam ||
       !kdbx_vault_backend_supported(KDBXVaultBackendFileExt)) {
        flippass_db_set_error(ctx, "Not enough RAM is available to keep this database open.");
        return;
    }

    ctx->app->pending_vault_fallback = true;
    flippass_log_event(
        ctx->app,
        "VAULT_FALLBACK_OFFER stage=%s remaining=%lu max=%lu request=%lu",
        stage != NULL ? stage : "-",
        (unsigned long)remaining_budget,
        (unsigned long)max_free_block,
        (unsigned long)request_size);

    if(ctx->app->rpc_mode) {
        flippass_db_set_error(
            ctx,
            "The encrypted RAM vault needs /ext to finish this database. Retry unlock with backend 'ext'.");
    } else {
        flippass_db_set_error(
            ctx,
            "FlipPass needs an encrypted /ext session file to finish opening this database.");
    }
}

static const char* flippass_db_field_log_name(uint32_t field_mask) {
    switch(field_mask) {
    case KDBXEntryFieldUsername:
        return "UserName";
    case KDBXEntryFieldPassword:
        return "Password";
    case KDBXEntryFieldUrl:
        return "URL";
    case KDBXEntryFieldNotes:
        return "Notes";
    case KDBXEntryFieldAutotype:
        return "AutoType";
    case KDBXEntryFieldUuid:
        return "UUID";
    default:
        return "Unknown";
    }
}

static bool flippass_db_is_supported_string_key(const char* key) {
    if(key == NULL || key[0] == '\0') {
        return false;
    }

    return true;
}

static bool flippass_db_commit_success(
    App* app,
    FlipPassDbLoadContext* ctx,
    KDBXVaultBackend backend) {
    app->db_arena = ctx->arena;
    app->vault = ctx->vault;
    app->root_group = ctx->root_group;
    app->current_group = ctx->root_group;
    app->current_entry = NULL;
    app->active_group = app->current_group;
    app->active_entry = NULL;
    app->database_loaded = true;
    app->active_vault_backend = backend;

    ctx->arena = NULL;
    ctx->vault = NULL;
    ctx->root_group = NULL;
    return true;
}

bool flippass_db_load_with_backend(App* app, KDBXVaultBackend backend, FuriString* error) {
    FlipPassDbLoadContext* ctx = NULL;
    const KDBXHeader* header = NULL;
    uint8_t cipher_key[32];
    uint8_t hmac_key[64];
    FlipPassDbPreflightSummary preflight_summary;
    KDBXVault* gzip_scratch_vault = NULL;
    KDBXVault* resume_gzip_scratch_vault = NULL;
    KDBXFieldRef gzip_scratch_ref;
    KDBXFieldRef resume_gzip_scratch_ref;
    size_t gzip_plain_size = 0U;
    size_t resume_gzip_plain_size = 0U;
    bool use_gzip_scratch = false;
    bool resume_from_gzip_scratch = false;
    bool ok = false;
    bool trace_capture_suspended = false;
    const bool allow_ext_promotion = app->allow_ext_vault_promotion;

    furi_assert(app);
    furi_assert(error);

    memset(cipher_key, 0, sizeof(cipher_key));
    memset(hmac_key, 0, sizeof(hmac_key));
    memset(&preflight_summary, 0, sizeof(preflight_summary));
    memset(&gzip_scratch_ref, 0, sizeof(gzip_scratch_ref));
    memset(&resume_gzip_scratch_ref, 0, sizeof(resume_gzip_scratch_ref));

    ctx = flippass_db_load_context_alloc(app, error);
    if(ctx == NULL) {
        furi_string_set_str(error, "Not enough RAM is available to start unlocking this database.");
        return false;
    }

    app->pending_vault_fallback = false;

    if(app->database_loaded && app->root_group != NULL) {
        flippass_db_load_context_free(ctx);
        return true;
    }

    if(app->master_password[0] == '\0') {
        furi_string_set_str(error, "Enter the database password to continue.");
        flippass_db_load_context_free(ctx);
        return false;
    }

    if(!kdbx_vault_backend_supported(backend)) {
        furi_string_set_str(error, kdbx_vault_backend_unavailable_reason(backend));
        flippass_clear_master_password(app);
        flippass_db_load_context_free(ctx);
        return false;
    }

    flippass_log_event(app, "UNLOCK_START");
    FLIPPASS_VERBOSE_LOG(
        app,
        "UNLOCK_EARLY path_len=%lu backend=%s free=%lu max=%lu stack=%lu",
        (unsigned long)furi_string_size(app->file_path),
        kdbx_vault_backend_label(backend),
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block(),
        (unsigned long)furi_thread_get_stack_space(furi_thread_get_current_id()));
#if FLIPPASS_ENABLE_SYSTEM_TRACE_CAPTURE
    flippass_log_event(app, "SYSTEM_TRACE_CAPTURE_PAUSE reason=unlock_hot_path");
#endif
    flippass_system_log_capture_suspend();
    trace_capture_suspended = true;
    resume_gzip_scratch_vault = app->pending_gzip_scratch_vault;
    resume_gzip_scratch_ref = app->pending_gzip_scratch_ref;
    resume_gzip_plain_size = app->pending_gzip_plain_size;
    resume_from_gzip_scratch =
        resume_gzip_scratch_vault != NULL && allow_ext_promotion;
    app->pending_gzip_scratch_vault = NULL;
    memset(&app->pending_gzip_scratch_ref, 0, sizeof(app->pending_gzip_scratch_ref));
    app->pending_gzip_plain_size = 0U;
    FLIPPASS_VERBOSE_LOG(app, "UNLOCK_STEP reset_begin");
    flippass_reset_database(app);
    app->requested_vault_backend = backend;
    app->allow_ext_vault_promotion = allow_ext_promotion;
    flippass_db_progress_update(app, "Reading Header", "", 3U);
    FLIPPASS_VERBOSE_LOG(
        app,
        "UNLOCK_STEP reset_ok free=%lu max=%lu stack=%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block(),
        (unsigned long)furi_thread_get_stack_space(furi_thread_get_current_id()));

    if(resume_from_gzip_scratch) {
        backend = KDBXVaultBackendFileExt;
        app->requested_vault_backend = backend;
        gzip_scratch_vault = resume_gzip_scratch_vault;
        resume_gzip_scratch_vault = NULL;
        gzip_scratch_ref = resume_gzip_scratch_ref;
        gzip_plain_size = resume_gzip_plain_size;
        use_gzip_scratch = true;
        ctx->xml_total_bytes_hint = gzip_plain_size;
        flippass_log_event(
            app,
            "GZIP_STAGE_RESUME bytes=%lu records=%lu",
            (unsigned long)gzip_plain_size,
            (unsigned long)gzip_scratch_ref.record_count);
        flippass_db_progress_update(app, "Continuing on /ext", "", 80U);
        goto model_alloc;
    }

    FLIPPASS_VERBOSE_LOG(app, "UNLOCK_STEP process_file_begin");
    if(!kdbx_parser_process_file(app->kdbx_parser, furi_string_get_cstr(app->file_path))) {
        flippass_log_event(app, "HEADER_FAIL");
        furi_string_set_str(error, "Failed to read the database header.");
        flippass_db_load_context_free(ctx);
        return false;
    }
    FLIPPASS_VERBOSE_LOG(
        app,
        "UNLOCK_STEP process_file_ok free=%lu max=%lu stack=%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block(),
        (unsigned long)furi_thread_get_stack_space(furi_thread_get_current_id()));
    flippass_db_progress_update(app, "Reading Header", "", 8U);

    header = kdbx_parser_get_header(app->kdbx_parser);
    FLIPPASS_VERBOSE_LOG(app, "UNLOCK_STEP validate_header_begin");
    if(!flippass_db_validate_header(header, error)) {
        flippass_log_event(app, "HEADER_FAIL");
        kdbx_parser_reset(app->kdbx_parser);
        flippass_clear_master_password(app);
        flippass_db_load_context_free(ctx);
        return false;
    }
    FLIPPASS_VERBOSE_LOG(app, "UNLOCK_STEP validate_header_ok");
    flippass_log_event(app, "HEADER_OK");
    flippass_db_progress_update(app, "Key Derivation", "", 10U);

    kdbx_parser_set_kdf_progress_callback(app->kdbx_parser, flippass_db_kdf_progress_callback, app);
    if(!kdbx_parser_derive_key(
           app->kdbx_parser,
           app->master_password,
           cipher_key,
           sizeof(cipher_key),
           hmac_key,
           sizeof(hmac_key))) {
        kdbx_parser_set_kdf_progress_callback(app->kdbx_parser, NULL, NULL);
        flippass_log_event(app, "KEY_DERIVE_FAIL");
        furi_string_set_str(error, "This database uses an unsupported or invalid KDF.");
        kdbx_parser_reset(app->kdbx_parser);
        flippass_clear_master_password(app);
        flippass_db_load_context_free(ctx);
        return false;
    }
    kdbx_parser_set_kdf_progress_callback(app->kdbx_parser, NULL, NULL);
    flippass_log_event(app, "KEY_DERIVE_OK");
    flippass_db_progress_update(app, "Decrypting", "", 38U);

    const bool can_preflight_ram_backend =
        backend == KDBXVaultBackendRam &&
        kdbx_vault_backend_supported(KDBXVaultBackendFileExt);

    if(header != NULL && header->compression_algorithm == KDBX_COMPRESSION_GZIP) {
        flippass_db_progress_update(app, "Decrypting", "", 45U);
        if(!flippass_db_stage_gzip_payload(
               app,
               backend,
               cipher_key,
               sizeof(cipher_key),
               hmac_key,
               sizeof(hmac_key),
               &gzip_scratch_vault,
               &gzip_scratch_ref,
               &gzip_plain_size,
               error)) {
            flippass_log_event(app, "DECRYPT_FAIL");
            goto cleanup;
        }

        use_gzip_scratch = true;
        ctx->xml_total_bytes_hint = gzip_plain_size;
        flippass_log_event(app, "DECRYPT_OK");
        if(can_preflight_ram_backend) {
            if(!flippass_db_run_preflight_from_vault(
                   app, gzip_scratch_vault, &gzip_scratch_ref, &preflight_summary, error)) {
                goto cleanup;
            }

            flippass_db_log_preflight_summary(app, &preflight_summary, "gzip_scratch");
            if(!flippass_db_apply_preflight_decision(
                   app, &preflight_summary, &backend, error)) {
                goto cleanup;
            }
        }
        flippass_db_progress_update(app, "Modeling", "", 82U);
        kdbx_parser_reset(app->kdbx_parser);
    } else if(can_preflight_ram_backend) {
        if(!flippass_db_run_preflight_from_payload(
               app,
               cipher_key,
               sizeof(cipher_key),
               hmac_key,
               sizeof(hmac_key),
               &preflight_summary,
               error)) {
            flippass_log_event(app, "DECRYPT_FAIL");
            goto cleanup;
        }

        flippass_db_log_preflight_summary(app, &preflight_summary, "payload");
        if(!flippass_db_apply_preflight_decision(app, &preflight_summary, &backend, error)) {
            goto cleanup;
        }

        kdbx_parser_reset(app->kdbx_parser);
        if(!kdbx_parser_process_file(app->kdbx_parser, furi_string_get_cstr(app->file_path))) {
            flippass_log_event(app, "HEADER_FAIL");
            furi_string_set_str(
                error, "Failed to reopen the database after sizing the XML model.");
            goto cleanup;
        }
    }

model_alloc:
    const bool reuse_scratch_vault = false;
    KDBXVault* replay_vault = NULL;
    const size_t scratch_vault_bytes = 0U;
    const size_t free_heap = memmgr_get_free_heap();
    if(free_heap <= FLIPPASS_DB_SAFETY_RESERVE_BYTES) {
        furi_string_set_str(error, "Not enough RAM is available to start unlocking this database.");
        goto cleanup;
    }

    ctx->committed_bytes = scratch_vault_bytes;
    ctx->commit_limit =
        scratch_vault_bytes + free_heap - FLIPPASS_DB_SAFETY_RESERVE_BYTES;
    FLIPPASS_VERBOSE_LOG(
        app,
        "UNLOCK_BUDGET free=%lu max=%lu reserve=%lu limit=%lu",
        (unsigned long)free_heap,
        (unsigned long)memmgr_heap_get_max_free_block(),
        (unsigned long)FLIPPASS_DB_SAFETY_RESERVE_BYTES,
        (unsigned long)ctx->commit_limit);
    ctx->arena = kdbx_arena_alloc(
        FLIPPASS_DB_ARENA_CHUNK_SIZE, &ctx->committed_bytes, ctx->commit_limit);
    FLIPPASS_VERBOSE_LOG(
        app,
        "UNLOCK_ALLOC_STEP step=arena ok=%u free=%lu max=%lu committed=%lu limit=%lu",
        ctx->arena != NULL ? 1U : 0U,
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block(),
        (unsigned long)ctx->committed_bytes,
        (unsigned long)ctx->commit_limit);
    ctx->vault = kdbx_vault_alloc(backend, &ctx->committed_bytes, ctx->commit_limit);
    FLIPPASS_VERBOSE_LOG(
        app,
        "UNLOCK_ALLOC_STEP step=vault ok=%u backend=%s free=%lu max=%lu committed=%lu limit=%lu",
        ctx->vault != NULL ? 1U : 0U,
        kdbx_vault_backend_label(backend),
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block(),
        (unsigned long)ctx->committed_bytes,
        (unsigned long)ctx->commit_limit);
    ctx->xml_parser = xml_parser_alloc();
    FLIPPASS_VERBOSE_LOG(
        app,
        "UNLOCK_ALLOC_STEP step=xml_parser ok=%u free=%lu max=%lu committed=%lu limit=%lu",
        ctx->xml_parser != NULL ? 1U : 0U,
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block(),
        (unsigned long)ctx->committed_bytes,
        (unsigned long)ctx->commit_limit);
    ctx->text_value = furi_string_alloc();
    if(ctx->text_value != NULL) {
        furi_string_reserve(ctx->text_value, 128U);
    }
    FLIPPASS_VERBOSE_LOG(
        app,
        "UNLOCK_ALLOC_STEP step=text_value ok=%u free=%lu max=%lu committed=%lu limit=%lu",
        ctx->text_value != NULL ? 1U : 0U,
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block(),
        (unsigned long)ctx->committed_bytes,
        (unsigned long)ctx->commit_limit);
    ctx->string_key = furi_string_alloc();
    if(ctx->string_key != NULL) {
        furi_string_reserve(ctx->string_key, 32U);
    }
    FLIPPASS_VERBOSE_LOG(
        app,
        "UNLOCK_ALLOC_STEP step=string_key ok=%u free=%lu max=%lu committed=%lu limit=%lu",
        ctx->string_key != NULL ? 1U : 0U,
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block(),
        (unsigned long)ctx->committed_bytes,
        (unsigned long)ctx->commit_limit);

    if(ctx->arena == NULL || ctx->vault == NULL || ctx->xml_parser == NULL || ctx->text_value == NULL ||
       ctx->string_key == NULL) {
        furi_string_set_str(error, "Not enough RAM is available to unlock this database.");
        goto cleanup;
    }

    if(kdbx_vault_storage_failed(ctx->vault)) {
        furi_string_set_str(
            error,
            "The encrypted session vault could not be created on the selected storage.");
        goto cleanup;
    }

    xml_parser_set_callback_context(ctx->xml_parser, ctx);
    xml_parser_set_element_handlers(ctx->xml_parser, flippass_db_start_element, flippass_db_end_element);
    xml_parser_set_character_data_handler(ctx->xml_parser, flippass_db_character_data);

    flippass_log_event(app, "VAULT_MODE backend=%s", kdbx_vault_backend_label(backend));
    FLIPPASS_DB_DEBUG_LOG_MEM(ctx, "unlock_ready");
    if(use_gzip_scratch) {
        replay_vault = reuse_scratch_vault ? ctx->vault : gzip_scratch_vault;
        FLIPPASS_VERBOSE_LOG(
            app,
            "GZIP_STAGE_REPLAY backend=%s records=%lu",
            kdbx_vault_backend_label(kdbx_vault_get_backend(replay_vault)),
            (unsigned long)gzip_scratch_ref.record_count);
        ok = kdbx_vault_stream_ref(
            replay_vault, &gzip_scratch_ref, flippass_db_payload_chunk_callback, ctx);
        if(!ok) {
            if(ctx->parse_failed && ctx->parse_error[0] != '\0') {
                furi_string_set_str(error, ctx->parse_error);
            } else {
                furi_string_set_str(
                    error, "The encrypted GZip scratch file could not be replayed safely.");
            }
            goto cleanup;
        }
        flippass_log_event(
            app,
            "XML_REPLAY_DONE xml=%lu groups=%lu entries=%lu records=%lu",
            (unsigned long)ctx->xml_bytes,
            (unsigned long)ctx->group_count,
            (unsigned long)ctx->entry_count,
            (unsigned long)kdbx_vault_record_count(ctx->vault));
    } else {
        flippass_db_progress_update(app, "Modeling", "", 70U);
        FLIPPASS_VERBOSE_LOG(
            app,
            "UNLOCK_STACK stage=payload_start stack=%lu",
            (unsigned long)furi_thread_get_stack_space(furi_thread_get_current_id()));
        ok = kdbx_parser_stream_payload(
            app->kdbx_parser,
            cipher_key,
            sizeof(cipher_key),
            hmac_key,
            sizeof(hmac_key),
            flippass_db_payload_chunk_callback,
            ctx);
        if(!ok) {
            flippass_log_event(app, "DECRYPT_FAIL");
            if(ctx->parse_failed && ctx->parse_error[0] != '\0') {
                furi_string_set_str(error, ctx->parse_error);
            } else {
                furi_string_set_str(
                    error,
                    kdbx_parser_get_last_error(app->kdbx_parser)[0] != '\0' ?
                        kdbx_parser_get_last_error(app->kdbx_parser) :
                        "Unable to decrypt or decompress the database payload.");
            }
            goto cleanup;
        }
        flippass_log_event(
            app,
            "XML_REPLAY_DONE xml=%lu groups=%lu entries=%lu records=%lu",
            (unsigned long)ctx->xml_bytes,
            (unsigned long)ctx->group_count,
            (unsigned long)ctx->entry_count,
            (unsigned long)kdbx_vault_record_count(ctx->vault));
        FURI_LOG_T(TAG, "payload stream return ok");
    }

    if(!ctx->inner_header_done) {
        flippass_log_event(
            app,
            "INNER_HEADER_FAIL prefix=%lu field=%lu remaining=%lu",
            (unsigned long)ctx->inner_header_prefix_len,
            (unsigned long)ctx->inner_field_id,
            (unsigned long)ctx->inner_field_remaining);
        furi_string_set_str(error, "The KDBX inner header could not be parsed.");
        goto cleanup;
    }
    FURI_LOG_T(TAG, "inner header ok");

    FURI_LOG_T(TAG, "xml finalize begin");
    flippass_db_progress_update(app, "Finalizing", "", 99U);
    if(!xml_parser_feed(ctx->xml_parser, NULL, 0U, true)) {
        furi_string_set_str(
            error,
            xml_parser_get_last_error(ctx->xml_parser) != NULL ?
                xml_parser_get_last_error(ctx->xml_parser) :
                "The XML payload could not be parsed.");
        goto cleanup;
    }
    FURI_LOG_T(TAG, "xml finalize ok");

    if(ctx->parse_failed) {
        furi_string_set_str(error, ctx->parse_error);
        goto cleanup;
    }

    if(ctx->root_group == NULL) {
        furi_string_set_str(error, "The decrypted XML payload did not contain any groups.");
        goto cleanup;
    }

    if(gzip_scratch_vault != NULL) {
        kdbx_vault_free(gzip_scratch_vault);
        gzip_scratch_vault = NULL;
    }

    FLIPPASS_DB_DEBUG_LOG_MEM(ctx, "parse_success");
    flippass_db_commit_success(app, ctx, backend);
    flippass_clear_master_password(app);
    kdbx_parser_reset(app->kdbx_parser);
    flippass_log_event(
        app,
        "PARSE_OK groups=%lu entries=%lu",
        (unsigned long)ctx->group_count,
        (unsigned long)ctx->entry_count);
    FLIPPASS_VERBOSE_LOG(
        app,
        "UNLOCK_RETURN_BEGIN free=%lu max=%lu stack=%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block(),
        (unsigned long)furi_thread_get_stack_space(furi_thread_get_current_id()));
    flippass_log_event(app, "DATABASE_READY");
    flippass_db_progress_update(app, "Ready", "", 100U);

cleanup:
    memzero(cipher_key, sizeof(cipher_key));
    memzero(hmac_key, sizeof(hmac_key));
    kdbx_parser_set_kdf_progress_callback(app->kdbx_parser, NULL, NULL);

    if(trace_capture_suspended) {
        flippass_system_log_capture_resume();
        trace_capture_suspended = false;
#if FLIPPASS_ENABLE_SYSTEM_TRACE_CAPTURE
        flippass_log_event(app, "SYSTEM_TRACE_CAPTURE_RESUME reason=unlock_hot_path");
#endif
    }

    if(!app->database_loaded && app->pending_vault_fallback && use_gzip_scratch &&
       gzip_scratch_vault != NULL && gzip_scratch_ref.record_count > 0U && ctx != NULL &&
       ctx->xml_bytes == 0U) {
        if(app->pending_gzip_scratch_vault != NULL) {
            kdbx_vault_free(app->pending_gzip_scratch_vault);
        }
        app->pending_gzip_scratch_vault = gzip_scratch_vault;
        app->pending_gzip_scratch_ref = gzip_scratch_ref;
        app->pending_gzip_plain_size = gzip_plain_size;
        gzip_scratch_vault = NULL;
        memset(&gzip_scratch_ref, 0, sizeof(gzip_scratch_ref));
        gzip_plain_size = 0U;
        flippass_log_event(
            app,
            "GZIP_STAGE_RESUME_CACHE bytes=%lu records=%lu",
            (unsigned long)app->pending_gzip_plain_size,
            (unsigned long)app->pending_gzip_scratch_ref.record_count);
    }

    if(!app->database_loaded) {
        if(gzip_scratch_vault != NULL && kdbx_vault_storage_failed(gzip_scratch_vault)) {
            flippass_log_event(
                app,
                "GZIP_STAGE_STORAGE_FAIL stage=%s records=%lu index=%lu",
                kdbx_vault_storage_stage(gzip_scratch_vault),
                (unsigned long)kdbx_vault_record_count(gzip_scratch_vault),
                (unsigned long)kdbx_vault_index_bytes(gzip_scratch_vault));
            furi_string_set_str(
                error,
                "The encrypted GZip scratch file could not be created on the selected storage.");
        }

        if(!app->pending_vault_fallback && !ctx->vault_promotion_attempted &&
           ((ctx->vault != NULL && kdbx_vault_budget_failed(ctx->vault)) ||
            (ctx->arena != NULL && kdbx_arena_budget_failed(ctx->arena)))) {
            FLIPPASS_DB_DEBUG_LOG_RAM(ctx, "cleanup_budget", "-", 0U);
            flippass_db_prepare_fallback_message(ctx, "cleanup_budget", 0U);
        } else if(ctx->vault != NULL && kdbx_vault_storage_failed(ctx->vault)) {
            flippass_log_event(
                app,
                "STORAGE_FAIL stage=%s records=%lu index=%lu",
                kdbx_vault_storage_stage(ctx->vault),
                (unsigned long)kdbx_vault_record_count(ctx->vault),
                (unsigned long)kdbx_vault_index_bytes(ctx->vault));
            furi_string_set_str(
                error,
                "The encrypted session vault could not be created on the selected storage.");
        }

        if(!app->pending_vault_fallback) {
            flippass_clear_master_password(app);
        }
        kdbx_parser_reset(app->kdbx_parser);
        if(furi_string_empty(error)) {
            furi_string_set_str(error, "Unable to unlock the selected database.");
        }
        flippass_log_event(app, "PARSE_FAIL reason=%s", furi_string_get_cstr(error));
    }

    if(gzip_scratch_vault != NULL) {
        kdbx_vault_free(gzip_scratch_vault);
        gzip_scratch_vault = NULL;
    }
    if(resume_gzip_scratch_vault != NULL) {
        kdbx_vault_free(resume_gzip_scratch_vault);
        resume_gzip_scratch_vault = NULL;
    }
    flippass_db_context_cleanup(ctx);
    flippass_db_load_context_free(ctx);
    return app->database_loaded;
}

bool flippass_db_load(App* app, FuriString* error) {
    KDBXVaultBackend backend = app->requested_vault_backend;
    if(backend == KDBXVaultBackendNone) {
        backend = KDBXVaultBackendRam;
    }

    app->requested_vault_backend = KDBXVaultBackendRam;
    return flippass_db_load_with_backend(app, backend, error);
}

void flippass_db_deactivate_entry(App* app) {
    furi_assert(app);

    if(app->active_entry != NULL) {
        flippass_log_event(app, "ENTRY_DEMATERIALIZE");
        kdbx_entry_clear_loaded_fields(app->active_entry);
    }

    app->active_entry = NULL;
}

static bool flippass_db_load_entry_field(
    App* app,
    KDBXEntry* entry,
    uint32_t field_mask,
    FuriString* error) {
    const KDBXFieldRef* ref = kdbx_entry_get_field_ref(entry, field_mask);
    char* plain = NULL;
    size_t plain_size = 0U;

    if(ref == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "The selected field is not available.");
        }
        return false;
    }

    if(!kdbx_vault_load_text(app->vault, ref, &plain, &plain_size)) {
        if(error != NULL) {
            furi_string_set_str(error, "The encrypted session vault could not be read.");
        }
        return false;
    }

    const bool ok = kdbx_entry_take_loaded_text(entry, field_mask, plain, plain_size);

    if(ok) {
        flippass_log_event(app, "FIELD_READY key=%s", flippass_db_field_log_name(field_mask));
    }

    if(!ok && error != NULL) {
        furi_string_set_str(error, "Not enough RAM is available to materialize this entry.");
    }

    if(!ok) {
        memzero(plain, plain_size + 1U);
        free(plain);
    }

    return ok;
}

static bool flippass_db_load_custom_field(
    App* app,
    KDBXCustomField* field,
    FuriString* error) {
    const KDBXFieldRef* ref = kdbx_custom_field_get_ref(field);
    char* plain = NULL;
    size_t plain_size = 0U;

    if(ref == NULL || kdbx_vault_ref_is_empty(ref)) {
        if(error != NULL) {
            furi_string_set_str(error, "The selected field is not available.");
        }
        return false;
    }

    if(!kdbx_vault_load_text(app->vault, ref, &plain, &plain_size)) {
        if(error != NULL) {
            furi_string_set_str(error, "The encrypted session vault could not be read.");
        }
        return false;
    }

    const bool ok = kdbx_custom_field_take_loaded_text(field, plain, plain_size);

    if(!ok && error != NULL) {
        furi_string_set_str(error, "Not enough RAM is available to materialize this entry.");
    }

    if(!ok) {
        memzero(plain, plain_size + 1U);
        free(plain);
    }

    return ok;
}

static bool flippass_db_load_entry_uuid(App* app, KDBXEntry* entry, FuriString* error) {
    const KDBXFieldRef* ref = kdbx_entry_get_uuid_ref(entry);
    char* plain = NULL;
    size_t plain_size = 0U;

    if(ref == NULL || kdbx_vault_ref_is_empty(ref)) {
        return true;
    }

    if(!kdbx_vault_load_text(app->vault, ref, &plain, &plain_size)) {
        if(error != NULL) {
            furi_string_set_str(error, "The encrypted session vault could not be read.");
        }
        return false;
    }

    const bool ok = kdbx_entry_take_loaded_text(entry, KDBXEntryFieldUuid, plain, plain_size);

    if(!ok && error != NULL) {
        furi_string_set_str(error, "Not enough RAM is available to materialize this entry.");
    }

    if(!ok) {
        memzero(plain, plain_size + 1U);
        free(plain);
    }

    return ok;
}

bool flippass_db_ensure_entry_field(
    App* app,
    KDBXEntry* entry,
    uint32_t field_mask,
    FuriString* error) {
    furi_assert(app);
    furi_assert(entry);

    if(!kdbx_entry_has_field(entry, field_mask)) {
        if(error != NULL) {
            furi_string_set_str(error, "The selected entry does not contain that field.");
        }
        return false;
    }

    if(entry != app->active_entry) {
        if(!flippass_db_activate_entry(
               app,
               entry,
               field_mask == KDBXEntryFieldNotes,
               error)) {
            return false;
        }
    }

    if(kdbx_entry_is_loaded(entry, field_mask)) {
        return true;
    }

    const bool ok = flippass_db_load_entry_field(app, entry, field_mask, error);
    if(ok && field_mask == KDBXEntryFieldNotes) {
        flippass_log_event(app, "ENTRY_NOTES_LOAD");
    }
    return ok;
}

bool flippass_db_ensure_custom_field(
    App* app,
    KDBXEntry* entry,
    KDBXCustomField* field,
    FuriString* error) {
    furi_assert(app);
    furi_assert(entry);
    furi_assert(field);

    if(entry != app->active_entry) {
        if(!flippass_db_activate_entry(app, entry, false, error)) {
            return false;
        }
    }

    if(kdbx_custom_field_is_loaded(field)) {
        return true;
    }

    return flippass_db_load_custom_field(app, field, error);
}

bool flippass_db_get_other_field_value(
    App* app,
    KDBXEntry* entry,
    uint32_t field_mask,
    KDBXCustomField* field,
    const char** out_value,
    FuriString* error) {
    furi_assert(app);
    furi_assert(entry);
    furi_assert(out_value);

    *out_value = NULL;

    if(field != NULL) {
        if(!flippass_db_ensure_custom_field(app, entry, field, error)) {
            return false;
        }

        *out_value = field->value;
        return true;
    }

    if(field_mask == 0U) {
        if(error != NULL) {
            furi_string_set_str(error, "The selected field is not available.");
        }
        return false;
    }

    if(!flippass_db_ensure_entry_field(app, entry, field_mask, error)) {
        return false;
    }

    switch(field_mask) {
    case KDBXEntryFieldUrl:
        *out_value = entry->url;
        return true;
    case KDBXEntryFieldNotes:
        *out_value = entry->notes;
        return true;
    case KDBXEntryFieldUsername:
        *out_value = entry->username;
        return true;
    case KDBXEntryFieldPassword:
        *out_value = entry->password;
        return true;
    case KDBXEntryFieldAutotype:
        *out_value = entry->autotype_sequence;
        return true;
    default:
        if(error != NULL) {
            furi_string_set_str(error, "The selected field is not available.");
        }
        return false;
    }
}

bool flippass_db_activate_entry(App* app, KDBXEntry* entry, bool load_notes, FuriString* error) {
    furi_assert(app);
    furi_assert(entry);

    if(app->vault == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "No database is currently unlocked.");
        }
        return false;
    }

    if(app->active_entry != entry) {
        flippass_db_deactivate_entry(app);
        app->active_entry = entry;
        flippass_log_event(app, "ENTRY_MATERIALIZE");
    }

    app->current_entry = entry;

    if(entry->uuid == NULL && !kdbx_vault_ref_is_empty(kdbx_entry_get_uuid_ref(entry)) &&
       !kdbx_entry_is_loaded(entry, KDBXEntryFieldUuid) &&
       !flippass_db_load_entry_uuid(app, entry, error)) {
        return false;
    }

    if(kdbx_entry_has_field(entry, KDBXEntryFieldUsername) &&
       !kdbx_entry_is_loaded(entry, KDBXEntryFieldUsername) &&
       !flippass_db_load_entry_field(app, entry, KDBXEntryFieldUsername, error)) {
        return false;
    }

    if(kdbx_entry_has_field(entry, KDBXEntryFieldPassword) &&
       !kdbx_entry_is_loaded(entry, KDBXEntryFieldPassword) &&
       !flippass_db_load_entry_field(app, entry, KDBXEntryFieldPassword, error)) {
        return false;
    }

    if(kdbx_entry_has_field(entry, KDBXEntryFieldUrl) &&
       !kdbx_entry_is_loaded(entry, KDBXEntryFieldUrl) &&
       !flippass_db_load_entry_field(app, entry, KDBXEntryFieldUrl, error)) {
        return false;
    }

    if(kdbx_entry_has_field(entry, KDBXEntryFieldAutotype) &&
       !kdbx_entry_is_loaded(entry, KDBXEntryFieldAutotype) &&
       !flippass_db_load_entry_field(app, entry, KDBXEntryFieldAutotype, error)) {
        return false;
    }

    if(load_notes && kdbx_entry_has_field(entry, KDBXEntryFieldNotes) &&
       !kdbx_entry_is_loaded(entry, KDBXEntryFieldNotes) &&
       !flippass_db_load_entry_field(app, entry, KDBXEntryFieldNotes, error)) {
        return false;
    }

    return true;
}

bool flippass_db_entry_has_field(const KDBXEntry* entry, uint32_t field_mask) {
    return kdbx_entry_has_field(entry, field_mask);
}

const KDBXCustomField* flippass_db_entry_get_custom_fields(const KDBXEntry* entry) {
    return kdbx_entry_get_custom_fields(entry);
}

static bool flippass_db_copy_ref_text(
    App* app,
    const KDBXFieldRef* ref,
    FuriString* out,
    FuriString* error) {
    char* plain = NULL;
    size_t plain_size = 0U;

    furi_assert(app);
    furi_assert(out);

    if(ref == NULL || kdbx_vault_ref_is_empty(ref)) {
        furi_string_reset(out);
        return true;
    }

    if(app->vault == NULL || !kdbx_vault_load_text(app->vault, ref, &plain, &plain_size)) {
        if(error != NULL) {
            furi_string_set_str(error, "The encrypted session vault could not be read.");
        }
        return false;
    }

    furi_string_set(out, plain);
    memzero(plain, plain_size + 1U);
    free(plain);
    return true;
}

bool flippass_db_copy_entry_uuid(App* app, const KDBXEntry* entry, FuriString* out, FuriString* error) {
    furi_assert(app);
    furi_assert(entry);
    furi_assert(out);

    if(entry->uuid != NULL) {
        furi_string_set(out, entry->uuid);
        return true;
    }

    return flippass_db_copy_ref_text(app, kdbx_entry_get_uuid_ref(entry), out, error);
}

bool flippass_db_copy_entry_title(App* app, const KDBXEntry* entry, FuriString* out, FuriString* error) {
    UNUSED(app);
    UNUSED(error);
    furi_assert(entry);
    furi_assert(out);

    furi_string_set_str(out, entry->title != NULL ? entry->title : "");
    return true;
}

KDBXVaultBackend flippass_db_parse_backend_hint(const char* text) {
    if(text == NULL || text[0] == '\0' || strcmp(text, "ram") == 0) {
        return KDBXVaultBackendRam;
    }

    if(strcmp(text, "int") == 0 || strcmp(text, "internal") == 0) {
        return KDBXVaultBackendFileInt;
    }

    if(strcmp(text, "ext") == 0 || strcmp(text, "external") == 0 || strcmp(text, "sd") == 0) {
        return KDBXVaultBackendFileExt;
    }

    return KDBXVaultBackendNone;
}

