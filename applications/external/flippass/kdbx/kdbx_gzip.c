#include "kdbx_gzip.h"

#include "memzero.h"
#include "miniz_tinfl.h"

#include <furi.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define KDBX_GZIP_HEADER_SIZE         10U
#define KDBX_GZIP_TRAILER_SIZE        8U
#define KDBX_GZIP_ID1                 0x1FU
#define KDBX_GZIP_ID2                 0x8BU
#define KDBX_GZIP_CM_DEFLATE          8U
#define KDBX_GZIP_FLAG_FHCRC          0x02U
#define KDBX_GZIP_FLAG_FEXTRA         0x04U
#define KDBX_GZIP_FLAG_FNAME          0x08U
#define KDBX_GZIP_FLAG_FCOMMENT       0x10U
#define KDBX_GZIP_FLAG_RESERVED       0xE0U
#define KDBX_GZIP_CRC32_POLY          0xEDB88320U
#define KDBX_GZIP_FILE_CACHE_PAGES    1U
#define KDBX_GZIP_FILE_MIN_CACHE_PAGES 1U
#define KDBX_GZIP_INPUT_CHUNK_BYTES   512U

typedef struct {
    KDBXGzipOutputCallback callback;
    void* context;
    uint32_t crc32;
    size_t output_size;
    size_t expected_output_size;
    bool callback_failed;
    bool output_limit_failed;
} KDBXGzipEmitState;

typedef struct {
    KDBXVaultReader reader;
    KDBXGzipTelemetry* telemetry;
    const KDBXGzipTraceConfig* trace_config;
    size_t skip_remaining;
    size_t remaining;
    size_t request_count;
    bool failed;
} KDBXGzipVaultReader;

typedef struct {
    const uint8_t* data;
    size_t remaining;
} KDBXGzipMemoryReader;

typedef struct {
    KDBXGzipTelemetry* telemetry;
    const KDBXGzipTraceConfig* user_trace;
    KDBXGzipInflatePath path;
} KDBXGzipTraceBridge;

static void kdbx_gzip_user_trace(
    const KDBXGzipTraceConfig* trace_config,
    KDBXGzipTelemetry* telemetry,
    const char* event);

static void kdbx_gzip_reset_telemetry(KDBXGzipTelemetry* telemetry) {
    if(telemetry == NULL) {
        return;
    }

    memset(telemetry, 0, sizeof(*telemetry));
    telemetry->status = KDBXGzipStatusInvalidArgument;
    telemetry->inflate_path = KDBXGzipInflatePathNone;
}

static uint16_t kdbx_gzip_read_u16_le(const uint8_t* data) {
    return ((uint16_t)data[0]) | ((uint16_t)data[1] << 8);
}

static uint32_t kdbx_gzip_read_u32_le(const uint8_t* data) {
    return ((uint32_t)data[0]) | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static uint32_t kdbx_gzip_crc32_update(uint32_t crc, const uint8_t* data, size_t data_size) {
    for(size_t i = 0; i < data_size; i++) {
        crc ^= data[i];
        for(uint8_t bit = 0; bit < 8U; bit++) {
            const uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (KDBX_GZIP_CRC32_POLY & mask);
        }
    }

    return crc;
}

static bool kdbx_gzip_skip_zero_terminated(
    const uint8_t* data,
    size_t data_size,
    size_t* offset) {
    while(*offset < data_size) {
        if(data[*offset] == '\0') {
            (*offset)++;
            return true;
        }
        (*offset)++;
    }

    return false;
}

static bool kdbx_gzip_prepare_member(
    const uint8_t* data,
    size_t data_size,
    size_t max_output_size,
    KDBXGzipTelemetry* telemetry,
    size_t* body_offset,
    size_t* compressed_size,
    uint32_t* expected_crc32,
    uint32_t* expected_output_size) {
    kdbx_gzip_reset_telemetry(telemetry);

    if(data == NULL || body_offset == NULL || compressed_size == NULL || expected_crc32 == NULL ||
       expected_output_size == NULL) {
        return false;
    }

    if(data_size < (KDBX_GZIP_HEADER_SIZE + KDBX_GZIP_TRAILER_SIZE)) {
        if(telemetry != NULL) {
            telemetry->status = KDBXGzipStatusTruncatedInput;
        }
        return false;
    }

    if(data[0] != KDBX_GZIP_ID1 || data[1] != KDBX_GZIP_ID2 || data[2] != KDBX_GZIP_CM_DEFLATE) {
        if(telemetry != NULL) {
            telemetry->status = KDBXGzipStatusInvalidHeader;
        }
        return false;
    }

    const uint8_t flags = data[3];
    if((flags & KDBX_GZIP_FLAG_RESERVED) != 0U) {
        if(telemetry != NULL) {
            telemetry->status = KDBXGzipStatusReservedFlags;
        }
        return false;
    }

    size_t offset = KDBX_GZIP_HEADER_SIZE;
    const size_t trailer_offset = data_size - KDBX_GZIP_TRAILER_SIZE;

    if(flags & KDBX_GZIP_FLAG_FEXTRA) {
        if(offset + 2U > trailer_offset) {
            if(telemetry != NULL) {
                telemetry->status = KDBXGzipStatusInvalidExtraField;
            }
            return false;
        }

        const size_t extra_size = kdbx_gzip_read_u16_le(data + offset);
        offset += 2U;
        if(offset + extra_size > trailer_offset) {
            if(telemetry != NULL) {
                telemetry->status = KDBXGzipStatusInvalidExtraField;
            }
            return false;
        }
        offset += extra_size;
    }

    if((flags & KDBX_GZIP_FLAG_FNAME) &&
       !kdbx_gzip_skip_zero_terminated(data, trailer_offset, &offset)) {
        if(telemetry != NULL) {
            telemetry->status = KDBXGzipStatusInvalidNameField;
        }
        return false;
    }

    if((flags & KDBX_GZIP_FLAG_FCOMMENT) &&
       !kdbx_gzip_skip_zero_terminated(data, trailer_offset, &offset)) {
        if(telemetry != NULL) {
            telemetry->status = KDBXGzipStatusInvalidCommentField;
        }
        return false;
    }

    if(flags & KDBX_GZIP_FLAG_FHCRC) {
        if(offset + 2U > trailer_offset) {
            if(telemetry != NULL) {
                telemetry->status = KDBXGzipStatusInvalidHeaderCrcField;
            }
            return false;
        }
        offset += 2U;
    }

    if(offset >= trailer_offset) {
        if(telemetry != NULL) {
            telemetry->status = KDBXGzipStatusInvalidBodyOffset;
        }
        return false;
    }

    *expected_crc32 = kdbx_gzip_read_u32_le(data + trailer_offset);
    *expected_output_size = kdbx_gzip_read_u32_le(data + trailer_offset + 4U);
    *body_offset = offset;
    *compressed_size = trailer_offset - offset;

    if(telemetry != NULL) {
        telemetry->expected_output_size = *expected_output_size;
        telemetry->free_heap = memmgr_get_free_heap();
        telemetry->max_free_block = memmgr_heap_get_max_free_block();
    }

    if(*expected_output_size == 0U || *expected_output_size > max_output_size) {
        if(telemetry != NULL) {
            telemetry->status = KDBXGzipStatusOutputTooLarge;
        }
        return false;
    }

    return true;
}

static const char* kdbx_gzip_window_path(void) {
    if(kdbx_vault_backend_supported(KDBXVaultBackendFileExt)) {
        return KDBX_VAULT_WINDOW_EXT_PATH;
    }

    if(kdbx_vault_backend_supported(KDBXVaultBackendFileInt)) {
        return KDBX_VAULT_WINDOW_INT_PATH;
    }

    return NULL;
}

size_t kdbx_gzip_file_paged_workspace_size(void) {
    return sizeof(tinfl_decompressor);
}

static bool kdbx_gzip_vault_reader_skip(KDBXGzipVaultReader* reader) {
    uint8_t discard[64];

    while(reader->skip_remaining > 0U) {
        if(reader->request_count <= 2U) {
            kdbx_gzip_user_trace(reader->trace_config, reader->telemetry, "vault_skip_request");
        }
        const size_t request =
            reader->skip_remaining < sizeof(discard) ? reader->skip_remaining : sizeof(discard);
        size_t out_size = 0U;
        if(!kdbx_vault_reader_read(&reader->reader, discard, request, &out_size) ||
           out_size != request) {
            memzero(discard, sizeof(discard));
            reader->failed = true;
            kdbx_gzip_user_trace(reader->trace_config, reader->telemetry, "vault_skip_fail");
            return false;
        }
        reader->skip_remaining -= out_size;
        if(reader->request_count <= 2U) {
            kdbx_gzip_user_trace(reader->trace_config, reader->telemetry, "vault_skip_ready");
        }
    }

    memzero(discard, sizeof(discard));
    return true;
}

static size_t kdbx_gzip_vault_reader_read(void* out, size_t capacity, void* context) {
    KDBXGzipVaultReader* reader = context;
    size_t out_size = 0U;

    if(reader == NULL || out == NULL || capacity == 0U || reader->failed) {
        return 0U;
    }

    const size_t request = (reader->remaining < capacity) ? reader->remaining : capacity;
    reader->request_count++;
    if(reader->request_count <= 4U) {
        kdbx_gzip_user_trace(reader->trace_config, reader->telemetry, "vault_input_request");
    }

    if(reader->skip_remaining > 0U && !kdbx_gzip_vault_reader_skip(reader)) {
        return 0U;
    }

    if(request == 0U) {
        if(reader->request_count <= 4U) {
            kdbx_gzip_user_trace(reader->trace_config, reader->telemetry, "vault_input_eof");
        }
        return 0U;
    }

    if(!kdbx_vault_reader_read(&reader->reader, out, request, &out_size) ||
       (out_size == 0U && reader->remaining > 0U)) {
        reader->failed = true;
        kdbx_gzip_user_trace(reader->trace_config, reader->telemetry, "vault_input_fail");
        return 0U;
    }

    if(out_size > reader->remaining) {
        reader->failed = true;
        kdbx_gzip_user_trace(reader->trace_config, reader->telemetry, "vault_input_overshoot");
        return 0U;
    }

    reader->remaining -= out_size;
    if(reader->request_count <= 4U) {
        kdbx_gzip_user_trace(reader->trace_config, reader->telemetry, "vault_input_ready");
    }
    return out_size;
}

static size_t kdbx_gzip_memory_reader_read(void* out, size_t capacity, void* context) {
    KDBXGzipMemoryReader* reader = context;
    const size_t out_size =
        (reader->remaining < capacity) ? reader->remaining : capacity;

    if(reader == NULL || out == NULL || capacity == 0U || reader->data == NULL) {
        return 0U;
    }

    if(out_size == 0U) {
        return 0U;
    }

    memcpy(out, reader->data, out_size);
    reader->data += out_size;
    reader->remaining -= out_size;
    return out_size;
}

static int kdbx_gzip_output_callback(const void* data, int len, void* context) {
    KDBXGzipEmitState* state = context;
    const uint8_t* bytes = data;

    if(state == NULL || len < 0) {
        return 0;
    }

    if(len == 0) {
        return 1;
    }

    const size_t size = (size_t)len;
    if(state->output_size + size > state->expected_output_size) {
        state->output_limit_failed = true;
        return 0;
    }

    state->crc32 = kdbx_gzip_crc32_update(state->crc32, bytes, size);
    state->output_size += size;

    if(state->callback != NULL && !state->callback(bytes, size, state->context)) {
        state->callback_failed = true;
        return 0;
    }

    return 1;
}

static void kdbx_gzip_sync_paged_telemetry(
    KDBXGzipTelemetry* telemetry,
    const tinfl_paged_telemetry* paged,
    KDBXGzipInflatePath path) {
    if(telemetry == NULL || paged == NULL) {
        return;
    }

    telemetry->inflate_path = path;
    telemetry->workspace_page_size = paged->page_size;
    telemetry->workspace_cache_pages = paged->cache_pages;
    telemetry->workspace_timeout_ms = paged->timeout_ms;
    telemetry->workspace_pages_allocated = paged->pages_allocated;
    telemetry->workspace_failed_page_index =
        (paged->failed_page_index != (size_t)-1) ? paged->failed_page_index : 0U;
    telemetry->workspace_storage_stage = paged->storage_stage;
    telemetry->paged_loop_count = paged->loop_count;
    telemetry->paged_flush_count = paged->flush_count;
    telemetry->paged_yield_count = paged->yield_count;
    telemetry->paged_no_progress_count = paged->no_progress_count;
    telemetry->paged_input_offset = paged->input_offset;
    telemetry->paged_last_input_advance = paged->last_input_advance;
    telemetry->paged_last_output_advance = paged->last_output_advance;
    telemetry->paged_last_dict_offset = paged->last_dict_offset;
    telemetry->paged_last_status = paged->last_status;
    telemetry->paged_timed_out = paged->timed_out != 0;
    telemetry->workspace_total_size =
        (path == KDBXGzipInflatePathPagedCallback) ?
            (sizeof(tinfl_decompressor) + (paged->page_size * paged->page_count)) :
            (sizeof(tinfl_decompressor) + KDBX_GZIP_INPUT_CHUNK_BYTES +
             (paged->page_size * (paged->cache_pages + 1U)));
}

static void kdbx_gzip_trace_bridge(
    const char* event,
    const tinfl_paged_telemetry* paged,
    void* context) {
    KDBXGzipTraceBridge* bridge = context;

    if(bridge == NULL) {
        return;
    }

    kdbx_gzip_sync_paged_telemetry(bridge->telemetry, paged, bridge->path);

    if(bridge->user_trace != NULL && bridge->user_trace->callback != NULL) {
        bridge->user_trace->callback(event, bridge->telemetry, bridge->user_trace->context);
    }
}

static void kdbx_gzip_user_trace(
    const KDBXGzipTraceConfig* trace_config,
    KDBXGzipTelemetry* telemetry,
    const char* event) {
    if(trace_config == NULL || trace_config->callback == NULL || telemetry == NULL || event == NULL) {
        return;
    }

    telemetry->free_heap = memmgr_get_free_heap();
    telemetry->max_free_block = memmgr_heap_get_max_free_block();
    trace_config->callback(event, telemetry, trace_config->context);
}

static KDBXGzipStatus kdbx_gzip_map_paged_status(
    const tinfl_paged_telemetry* paged,
    const KDBXGzipEmitState* emit_state,
    bool input_failed) {
    if(emit_state != NULL) {
        if(emit_state->output_limit_failed) {
            return KDBXGzipStatusOutputSizeMismatch;
        }
        if(emit_state->callback_failed) {
            return KDBXGzipStatusOutputRejected;
        }
    }

    if(input_failed) {
        return KDBXGzipStatusInputSizeMismatch;
    }

    if(paged == NULL) {
        return KDBXGzipStatusInflateFailed;
    }

    if(paged->timed_out) {
        return KDBXGzipStatusPagedTimeLimit;
    }

    if(paged->no_progress_count > 0U) {
        return KDBXGzipStatusPagedNoProgress;
    }

    if(paged->failed_page_index != (size_t)-1) {
        return KDBXGzipStatusWorkspacePageAllocFailed;
    }

    if(paged->storage_stage != NULL) {
        if(strcmp(paged->storage_stage, "window_verify") == 0) {
            return KDBXGzipStatusWorkspaceVerifyFailed;
        }

        if(strcmp(paged->storage_stage, "window_cache_alloc") == 0 ||
           strcmp(paged->storage_stage, "window_cache") == 0 ||
           strcmp(paged->storage_stage, "window_dict_alloc") == 0 ||
           strcmp(paged->storage_stage, "file_decomp_alloc") == 0 ||
           strcmp(paged->storage_stage, "file_input_alloc") == 0 ||
           strcmp(paged->storage_stage, "window_file_alloc") == 0) {
            return (paged->failed_page_index != (size_t)-1) ? KDBXGzipStatusWorkspacePageAllocFailed :
                                                              KDBXGzipStatusWorkspaceAllocFailed;
        }

        if(strcmp(paged->storage_stage, "window_open_create") == 0 ||
           strcmp(paged->storage_stage, "window_open_write") == 0 ||
           strcmp(paged->storage_stage, "window_open_read") == 0 ||
           strcmp(paged->storage_stage, "window_write") == 0 ||
           strcmp(paged->storage_stage, "window_read") == 0 ||
           strcmp(paged->storage_stage, "window_storage") == 0 ||
           strcmp(paged->storage_stage, "window_mkdir") == 0 ||
           strcmp(paged->storage_stage, "window_cleanup") == 0) {
            return KDBXGzipStatusWorkspaceStorageFailed;
        }
    }

    return KDBXGzipStatusInflateFailed;
}

static bool kdbx_gzip_can_try_exact_output(
    size_t expected_output_size,
    KDBXGzipTelemetry* telemetry) {
    const size_t free_heap = memmgr_get_free_heap();
    const size_t max_free = memmgr_heap_get_max_free_block();

    if(telemetry != NULL) {
        telemetry->free_heap = free_heap;
        telemetry->max_free_block = max_free;
        telemetry->workspace_total_size = expected_output_size;
        telemetry->workspace_page_size = 0U;
        telemetry->workspace_cache_pages = 0U;
    }

    return expected_output_size > 0U && free_heap >= expected_output_size &&
           max_free >= expected_output_size;
}

static bool kdbx_gzip_emit_exact_output_memory(
    const uint8_t* compressed_data,
    size_t compressed_size,
    uint32_t expected_crc32,
    uint32_t expected_output_size,
    KDBXGzipOutputCallback callback,
    void* context,
    KDBXGzipTelemetry* telemetry) {
    uint8_t* output = NULL;
    size_t actual_output_size = 0U;
    KDBXGzipEmitState emit_state;

    if(compressed_data == NULL || callback == NULL || telemetry == NULL) {
        kdbx_gzip_reset_telemetry(telemetry);
        if(telemetry != NULL) {
            telemetry->status = KDBXGzipStatusInvalidArgument;
        }
        return false;
    }

    memset(&emit_state, 0, sizeof(emit_state));
    emit_state.callback = callback;
    emit_state.context = context;
    emit_state.crc32 = 0xFFFFFFFFU;
    emit_state.expected_output_size = expected_output_size;

    telemetry->inflate_path = KDBXGzipInflatePathExactOutput;
    telemetry->expected_output_size = expected_output_size;
    telemetry->workspace_total_size = expected_output_size;
    telemetry->workspace_page_size = 0U;
    telemetry->workspace_cache_pages = 0U;
    telemetry->free_heap = memmgr_get_free_heap();
    telemetry->max_free_block = memmgr_heap_get_max_free_block();

    output = malloc(expected_output_size);
    if(output == NULL) {
        telemetry->status =
            (telemetry->max_free_block < expected_output_size) ? KDBXGzipStatusOutputHeapFragmented :
                                                                 KDBXGzipStatusOutputAllocFailed;
        return false;
    }

    actual_output_size = tinfl_decompress_mem_to_mem(
        output, expected_output_size, compressed_data, compressed_size, 0);
    telemetry->consumed_input_size = compressed_size;
    telemetry->actual_output_size =
        (actual_output_size == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED) ? 0U : actual_output_size;
    telemetry->inflate_status =
        (actual_output_size == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED) ? TINFL_STATUS_FAILED :
                                                                     TINFL_STATUS_DONE;
    telemetry->free_heap = memmgr_get_free_heap();
    telemetry->max_free_block = memmgr_heap_get_max_free_block();

    if(actual_output_size == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED) {
        telemetry->status = KDBXGzipStatusInflateFailed;
        memzero(output, expected_output_size);
        free(output);
        return false;
    }

    if(actual_output_size != expected_output_size) {
        telemetry->status = KDBXGzipStatusOutputSizeMismatch;
        memzero(output, expected_output_size);
        free(output);
        return false;
    }

    if(!kdbx_gzip_output_callback(output, (int)actual_output_size, &emit_state)) {
        telemetry->status =
            emit_state.callback_failed ? KDBXGzipStatusOutputRejected :
                                         KDBXGzipStatusOutputSizeMismatch;
        memzero(output, expected_output_size);
        free(output);
        return false;
    }

    emit_state.crc32 = ~emit_state.crc32;
    if(emit_state.crc32 != expected_crc32) {
        telemetry->status = KDBXGzipStatusCrcMismatch;
        memzero(output, expected_output_size);
        free(output);
        return false;
    }

    telemetry->status = KDBXGzipStatusOk;
    memzero(output, expected_output_size);
    free(output);
    return true;
}

static bool kdbx_gzip_can_try_ram_paged(KDBXGzipTelemetry* telemetry) {
    const size_t required_total = sizeof(tinfl_decompressor) + TINFL_LZ_DICT_SIZE;
    const size_t free_heap = memmgr_get_free_heap();
    const size_t max_free = memmgr_heap_get_max_free_block();

    if(telemetry != NULL) {
        telemetry->free_heap = free_heap;
        telemetry->max_free_block = max_free;
        telemetry->workspace_total_size = required_total;
        telemetry->workspace_page_size = TINFL_PAGED_LZ_DICT_PAGE_SIZE;
        telemetry->workspace_cache_pages = TINFL_PAGED_LZ_DICT_PAGE_COUNT;
    }

    return free_heap >= required_total;
}

static bool kdbx_gzip_emit_ram_paged_reader(
    tinfl_get_buf_func_ptr reader_callback,
    void* reader_context,
    uint32_t expected_crc32,
    uint32_t expected_output_size,
    KDBXGzipOutputCallback callback,
    void* context,
    KDBXGzipTelemetry* telemetry,
    const KDBXGzipTraceConfig* trace_config) {
    KDBXGzipEmitState emit_state;
    KDBXGzipTraceBridge bridge;
    tinfl_paged_telemetry paged;
    size_t consumed_size = 0U;

    if(reader_callback == NULL || callback == NULL || telemetry == NULL) {
        kdbx_gzip_reset_telemetry(telemetry);
        if(telemetry != NULL) {
            telemetry->status = KDBXGzipStatusInvalidArgument;
        }
        return false;
    }

    memset(&emit_state, 0, sizeof(emit_state));
    emit_state.callback = callback;
    emit_state.context = context;
    emit_state.crc32 = 0xFFFFFFFFU;
    emit_state.expected_output_size = expected_output_size;

    memset(&bridge, 0, sizeof(bridge));
    bridge.telemetry = telemetry;
    bridge.user_trace = trace_config;
    bridge.path = KDBXGzipInflatePathPagedCallback;

    memset(&paged, 0, sizeof(paged));
    paged.trace_interval_bytes = trace_config != NULL ? trace_config->interval_bytes : 0U;
    paged.trace_callback = kdbx_gzip_trace_bridge;
    paged.trace_context = &bridge;

    telemetry->inflate_path = KDBXGzipInflatePathPagedCallback;
    telemetry->expected_output_size = expected_output_size;
    telemetry->free_heap = memmgr_get_free_heap();
    telemetry->max_free_block = memmgr_heap_get_max_free_block();

    const bool ok = tinfl_decompress_reader_to_callback_paged_ex(
                        reader_callback,
                        reader_context,
                        &consumed_size,
                        kdbx_gzip_output_callback,
                        &emit_state,
                        0,
                        &paged) != 0;

    kdbx_gzip_sync_paged_telemetry(telemetry, &paged, KDBXGzipInflatePathPagedCallback);
    telemetry->consumed_input_size = consumed_size;
    telemetry->actual_output_size = emit_state.output_size;
    telemetry->inflate_status = paged.last_status;
    telemetry->free_heap = memmgr_get_free_heap();
    telemetry->max_free_block = memmgr_heap_get_max_free_block();

    if(!ok) {
        telemetry->status = kdbx_gzip_map_paged_status(&paged, &emit_state, false);
        return false;
    }

    emit_state.crc32 = ~emit_state.crc32;
    if(emit_state.output_size != expected_output_size) {
        telemetry->status = KDBXGzipStatusOutputSizeMismatch;
        return false;
    }

    if(emit_state.crc32 != expected_crc32) {
        telemetry->status = KDBXGzipStatusCrcMismatch;
        return false;
    }

    telemetry->status = KDBXGzipStatusOk;
    return true;
}

static bool kdbx_gzip_emit_ram_paged_memory(
    const uint8_t* compressed_data,
    size_t compressed_size,
    uint32_t expected_crc32,
    uint32_t expected_output_size,
    KDBXGzipOutputCallback callback,
    void* context,
    KDBXGzipTelemetry* telemetry,
    const KDBXGzipTraceConfig* trace_config) {
    KDBXGzipMemoryReader reader;

    if(compressed_data == NULL || callback == NULL || telemetry == NULL) {
        kdbx_gzip_reset_telemetry(telemetry);
        if(telemetry != NULL) {
            telemetry->status = KDBXGzipStatusInvalidArgument;
        }
        return false;
    }

    reader.data = compressed_data;
    reader.remaining = compressed_size;
    return kdbx_gzip_emit_ram_paged_reader(
        kdbx_gzip_memory_reader_read,
        &reader,
        expected_crc32,
        expected_output_size,
        callback,
        context,
        telemetry,
        trace_config);
}

static bool kdbx_gzip_emit_paged_reader(
    tinfl_get_buf_func_ptr reader_callback,
    void* reader_context,
    uint32_t expected_crc32,
    uint32_t expected_output_size,
    KDBXGzipOutputCallback callback,
    void* context,
    Storage* storage,
    KDBXGzipTelemetry* telemetry,
    const KDBXGzipTraceConfig* trace_config) {
    KDBXGzipEmitState emit_state;
    KDBXGzipTraceBridge bridge;
    tinfl_paged_telemetry paged;
    tinfl_paged_file_config file_config;
    size_t consumed_size = 0U;
    const char* window_path = kdbx_gzip_window_path();

    if(reader_callback == NULL || callback == NULL || telemetry == NULL || window_path == NULL) {
        kdbx_gzip_reset_telemetry(telemetry);
        if(telemetry != NULL) {
            telemetry->status = (window_path == NULL) ? KDBXGzipStatusWorkspaceStorageFailed :
                                                        KDBXGzipStatusInvalidArgument;
        }
        return false;
    }

    memset(&emit_state, 0, sizeof(emit_state));
    emit_state.callback = callback;
    emit_state.context = context;
    emit_state.crc32 = 0xFFFFFFFFU;
    emit_state.expected_output_size = expected_output_size;

    memset(&bridge, 0, sizeof(bridge));
    bridge.telemetry = telemetry;
    bridge.user_trace = trace_config;
    bridge.path = KDBXGzipInflatePathFilePagedCallback;

    memset(&paged, 0, sizeof(paged));
    paged.trace_interval_bytes = trace_config != NULL ? trace_config->interval_bytes : 0U;
    paged.trace_callback = kdbx_gzip_trace_bridge;
    paged.trace_context = &bridge;

    memset(&file_config, 0, sizeof(file_config));
    file_config.file_path = window_path;
    file_config.storage = storage;
    file_config.preferred_cache_pages = KDBX_GZIP_FILE_CACHE_PAGES;
    file_config.minimum_cache_pages = KDBX_GZIP_FILE_MIN_CACHE_PAGES;

    telemetry->inflate_path = KDBXGzipInflatePathFilePagedCallback;
    telemetry->expected_output_size = expected_output_size;
    telemetry->free_heap = memmgr_get_free_heap();
    telemetry->max_free_block = memmgr_heap_get_max_free_block();

    const bool ok = tinfl_decompress_reader_to_callback_file_paged_ex(
                        reader_callback,
                        reader_context,
                        &consumed_size,
                        kdbx_gzip_output_callback,
                        &emit_state,
                        0,
                        &file_config,
                        trace_config != NULL ? (tinfl_decompressor*)trace_config->inflate_workspace :
                                               NULL,
                        &paged) != 0;

    kdbx_gzip_sync_paged_telemetry(telemetry, &paged, KDBXGzipInflatePathFilePagedCallback);
    telemetry->consumed_input_size = consumed_size;
    telemetry->actual_output_size = emit_state.output_size;
    telemetry->inflate_status = paged.last_status;
    telemetry->free_heap = memmgr_get_free_heap();
    telemetry->max_free_block = memmgr_heap_get_max_free_block();

    if(!ok) {
        telemetry->status = kdbx_gzip_map_paged_status(&paged, &emit_state, false);
        return false;
    }

    emit_state.crc32 = ~emit_state.crc32;
    if(emit_state.output_size != expected_output_size) {
        telemetry->status = KDBXGzipStatusOutputSizeMismatch;
        return false;
    }

    if(emit_state.crc32 != expected_crc32) {
        telemetry->status = KDBXGzipStatusCrcMismatch;
        return false;
    }

    telemetry->status = KDBXGzipStatusOk;
    return true;
}

static bool kdbx_gzip_emit_paged_memory(
    const uint8_t* compressed_data,
    size_t compressed_size,
    uint32_t expected_crc32,
    uint32_t expected_output_size,
    KDBXGzipOutputCallback callback,
    void* context,
    KDBXGzipTelemetry* telemetry,
    const KDBXGzipTraceConfig* trace_config) {
    KDBXGzipMemoryReader reader;

    if(compressed_data == NULL || callback == NULL || telemetry == NULL) {
        kdbx_gzip_reset_telemetry(telemetry);
        if(telemetry != NULL) {
            telemetry->status = KDBXGzipStatusInvalidArgument;
        }
        return false;
    }

    if(trace_config != NULL && trace_config->callback != NULL) {
        telemetry->inflate_path = KDBXGzipInflatePathFilePagedCallback;
        telemetry->expected_output_size = expected_output_size;
        telemetry->free_heap = memmgr_get_free_heap();
        telemetry->max_free_block = memmgr_heap_get_max_free_block();
        trace_config->callback("memory_file_attempt", telemetry, trace_config->context);
    }

    reader.data = compressed_data;
    reader.remaining = compressed_size;
    const bool ok = kdbx_gzip_emit_paged_reader(
        kdbx_gzip_memory_reader_read,
        &reader,
        expected_crc32,
        expected_output_size,
        callback,
        context,
        NULL,
        telemetry,
        trace_config);

    if(trace_config != NULL && trace_config->callback != NULL) {
        trace_config->callback("memory_file_return", telemetry, trace_config->context);
    }

    return ok;
}

bool kdbx_gzip_emit_stream(
    const uint8_t* data,
    size_t data_size,
    size_t max_output_size,
    KDBXGzipOutputCallback callback,
    void* context,
    KDBXGzipTelemetry* telemetry) {
    return kdbx_gzip_emit_stream_ex(
        data, data_size, max_output_size, callback, context, telemetry, NULL);
}

bool kdbx_gzip_emit_stream_ex(
    const uint8_t* data,
    size_t data_size,
    size_t max_output_size,
    KDBXGzipOutputCallback callback,
    void* context,
    KDBXGzipTelemetry* telemetry,
    const KDBXGzipTraceConfig* trace_config) {
    size_t body_offset = 0U;
    size_t compressed_size = 0U;
    uint32_t expected_crc32 = 0U;
    uint32_t expected_output_size = 0U;
    if(callback == NULL) {
        kdbx_gzip_reset_telemetry(telemetry);
        return false;
    }

    if(!kdbx_gzip_prepare_member(
           data,
           data_size,
           max_output_size,
           telemetry,
           &body_offset,
           &compressed_size,
           &expected_crc32,
           &expected_output_size)) {
        return false;
    }

    if(kdbx_gzip_can_try_exact_output(expected_output_size, telemetry)) {
        if(kdbx_gzip_emit_exact_output_memory(
               data + body_offset,
               compressed_size,
               expected_crc32,
               expected_output_size,
               callback,
               context,
               telemetry)) {
            return true;
        }

        if(telemetry == NULL ||
           (telemetry->status != KDBXGzipStatusOutputHeapFragmented &&
            telemetry->status != KDBXGzipStatusOutputAllocFailed)) {
            return false;
        }
    }

    if(trace_config != NULL && trace_config->prefer_file_paged) {
        return kdbx_gzip_emit_paged_memory(
            data + body_offset,
            compressed_size,
            expected_crc32,
            expected_output_size,
            callback,
            context,
            telemetry,
            trace_config);
    }

    if(kdbx_gzip_can_try_ram_paged(telemetry)) {
        if(kdbx_gzip_emit_ram_paged_memory(
               data + body_offset,
               compressed_size,
               expected_crc32,
               expected_output_size,
               callback,
               context,
               telemetry,
               trace_config)) {
            return true;
        }

        if(telemetry == NULL ||
           (telemetry->status != KDBXGzipStatusWorkspaceAllocFailed &&
            telemetry->status != KDBXGzipStatusWorkspacePageAllocFailed &&
            telemetry->status != KDBXGzipStatusWorkspaceTotalTooSmall)) {
            return false;
        }
    }

    return kdbx_gzip_emit_paged_memory(
        data + body_offset,
        compressed_size,
        expected_crc32,
        expected_output_size,
        callback,
        context,
        telemetry,
        trace_config);
}

bool kdbx_gzip_parse_member_info(
    const uint8_t* prefix,
    size_t prefix_size,
    const uint8_t trailer[8],
    size_t member_size,
    size_t max_output_size,
    KDBXGzipTelemetry* telemetry,
    KDBXGzipMemberInfo* out_info) {
    if(out_info == NULL) {
        kdbx_gzip_reset_telemetry(telemetry);
        return false;
    }

    memset(out_info, 0, sizeof(*out_info));
    kdbx_gzip_reset_telemetry(telemetry);

    if(prefix == NULL || trailer == NULL || prefix_size < KDBX_GZIP_HEADER_SIZE ||
       member_size < (KDBX_GZIP_HEADER_SIZE + KDBX_GZIP_TRAILER_SIZE)) {
        if(telemetry != NULL) {
            telemetry->status = KDBXGzipStatusTruncatedInput;
        }
        return false;
    }

    if(prefix[0] != KDBX_GZIP_ID1 || prefix[1] != KDBX_GZIP_ID2 ||
       prefix[2] != KDBX_GZIP_CM_DEFLATE) {
        if(telemetry != NULL) {
            telemetry->status = KDBXGzipStatusInvalidHeader;
        }
        return false;
    }

    const uint8_t flags = prefix[3];
    if((flags & KDBX_GZIP_FLAG_RESERVED) != 0U) {
        if(telemetry != NULL) {
            telemetry->status = KDBXGzipStatusReservedFlags;
        }
        return false;
    }

    size_t offset = KDBX_GZIP_HEADER_SIZE;
    const size_t trailer_offset = member_size - KDBX_GZIP_TRAILER_SIZE;

    if(flags & KDBX_GZIP_FLAG_FEXTRA) {
        if(offset + 2U > prefix_size || offset + 2U > trailer_offset) {
            if(telemetry != NULL) {
                telemetry->status = KDBXGzipStatusInvalidExtraField;
            }
            return false;
        }

        const size_t extra_size = kdbx_gzip_read_u16_le(prefix + offset);
        offset += 2U;
        if(offset + extra_size > prefix_size || offset + extra_size > trailer_offset) {
            if(telemetry != NULL) {
                telemetry->status = KDBXGzipStatusInvalidExtraField;
            }
            return false;
        }
        offset += extra_size;
    }

    if((flags & KDBX_GZIP_FLAG_FNAME) &&
       (!kdbx_gzip_skip_zero_terminated(prefix, prefix_size, &offset) || offset > trailer_offset)) {
        if(telemetry != NULL) {
            telemetry->status = KDBXGzipStatusInvalidNameField;
        }
        return false;
    }

    if((flags & KDBX_GZIP_FLAG_FCOMMENT) &&
       (!kdbx_gzip_skip_zero_terminated(prefix, prefix_size, &offset) || offset > trailer_offset)) {
        if(telemetry != NULL) {
            telemetry->status = KDBXGzipStatusInvalidCommentField;
        }
        return false;
    }

    if(flags & KDBX_GZIP_FLAG_FHCRC) {
        if(offset + 2U > prefix_size || offset + 2U > trailer_offset) {
            if(telemetry != NULL) {
                telemetry->status = KDBXGzipStatusInvalidHeaderCrcField;
            }
            return false;
        }
        offset += 2U;
    }

    if(offset >= trailer_offset) {
        if(telemetry != NULL) {
            telemetry->status = KDBXGzipStatusInvalidBodyOffset;
        }
        return false;
    }

    out_info->member_size = member_size;
    out_info->body_offset = offset;
    out_info->compressed_size = trailer_offset - offset;
    out_info->expected_crc32 = kdbx_gzip_read_u32_le(trailer);
    out_info->expected_output_size = kdbx_gzip_read_u32_le(trailer + 4U);

    if(telemetry != NULL) {
        telemetry->expected_output_size = out_info->expected_output_size;
        telemetry->free_heap = memmgr_get_free_heap();
        telemetry->max_free_block = memmgr_heap_get_max_free_block();
    }

    if(out_info->expected_output_size == 0U || out_info->expected_output_size > max_output_size) {
        if(telemetry != NULL) {
            telemetry->status = KDBXGzipStatusOutputTooLarge;
        }
        return false;
    }

    if(telemetry != NULL) {
        telemetry->status = KDBXGzipStatusOk;
    }
    return true;
}

bool kdbx_gzip_emit_vault_stream(
    KDBXVault* vault,
    const KDBXFieldRef* ref,
    const KDBXGzipMemberInfo* member_info,
    size_t max_output_size,
    KDBXGzipOutputCallback callback,
    void* context,
    KDBXGzipTelemetry* telemetry,
    const KDBXGzipTraceConfig* trace_config) {
    KDBXGzipVaultReader* reader = NULL;

    if(vault == NULL || ref == NULL || member_info == NULL || callback == NULL) {
        kdbx_gzip_reset_telemetry(telemetry);
        return false;
    }

    if(member_info->expected_output_size == 0U || member_info->expected_output_size > max_output_size) {
        kdbx_gzip_reset_telemetry(telemetry);
        if(telemetry != NULL) {
            telemetry->expected_output_size = member_info->expected_output_size;
            telemetry->status = KDBXGzipStatusOutputTooLarge;
        }
        return false;
    }

    reader = malloc(sizeof(*reader));
    if(reader == NULL) {
        kdbx_gzip_reset_telemetry(telemetry);
        if(telemetry != NULL) {
            telemetry->status = KDBXGzipStatusWorkspaceAllocFailed;
            telemetry->inflate_path = KDBXGzipInflatePathFilePagedCallback;
            telemetry->free_heap = memmgr_get_free_heap();
            telemetry->max_free_block = memmgr_heap_get_max_free_block();
        }
        return false;
    }

    memset(reader, 0, sizeof(*reader));
    kdbx_vault_reader_reset(&reader->reader, vault, ref);
    reader->telemetry = telemetry;
    reader->trace_config = trace_config;
    reader->skip_remaining = member_info->body_offset;
    reader->remaining = member_info->compressed_size;

    const bool prefer_file_paged = trace_config != NULL && trace_config->prefer_file_paged;
    const bool can_try_ram = !prefer_file_paged && kdbx_gzip_can_try_ram_paged(telemetry);
    bool ok = false;
    if(can_try_ram) {
        ok = kdbx_gzip_emit_ram_paged_reader(
            kdbx_gzip_vault_reader_read,
            reader,
            member_info->expected_crc32,
            member_info->expected_output_size,
            callback,
            context,
            telemetry,
            trace_config);
    }

    if(!ok &&
       (!can_try_ram ||
        (telemetry != NULL &&
         (telemetry->status == KDBXGzipStatusWorkspaceAllocFailed ||
         telemetry->status == KDBXGzipStatusWorkspacePageAllocFailed ||
         telemetry->status == KDBXGzipStatusWorkspaceTotalTooSmall)))) {
        kdbx_vault_reader_reset(&reader->reader, vault, ref);
        reader->skip_remaining = member_info->body_offset;
        reader->remaining = member_info->compressed_size;
        reader->request_count = 0U;
        reader->failed = false;
        ok = kdbx_gzip_emit_paged_reader(
            kdbx_gzip_vault_reader_read,
            reader,
            member_info->expected_crc32,
            member_info->expected_output_size,
            callback,
            context,
            NULL,
            telemetry,
            trace_config);
    }

    if(reader->failed && telemetry != NULL) {
        telemetry->status = KDBXGzipStatusInputSizeMismatch;
    } else if(
        ok && telemetry != NULL && telemetry->consumed_input_size > member_info->compressed_size) {
        telemetry->status = KDBXGzipStatusInputSizeMismatch;
        memzero(reader, sizeof(*reader));
        free(reader);
        return false;
    }

    const bool result = ok && !reader->failed;
    memzero(reader, sizeof(*reader));
    free(reader);
    return result;
}
