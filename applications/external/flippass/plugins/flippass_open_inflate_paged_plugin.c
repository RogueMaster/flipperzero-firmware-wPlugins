#include "flippass_open_inflate_plugin.h"

#include "../kdbx/miniz_tinfl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLIPPASS_OPEN_MAX_XML_STREAM_BYTES  (2U * 1024U * 1024U)
#define FLIPPASS_OPEN_GZIP_TRAILER_SIZE     8U
#define FLIPPASS_OPEN_GZIP_FILE_CACHE_PAGES 1U
#define FLIPPASS_OPEN_GZIP_FILE_MIN_PAGES   1U

typedef struct {
    const FlipPassOpenInflateHostApiV1* host_api;
    FuriString* error;
    uint8_t progress_percent;
} FlipPassOpenInflateContext;

typedef struct {
    FlipPassOpenInflateContext* inflate_ctx;
    size_t plain_size;
    size_t last_progress_size;
    uint32_t expected_plain_size;
} FlipPassOpenInflateXmlStageContext;

typedef struct {
    const FlipPassOpenInflateHostApiV1* host_api;
    size_t skip_bytes;
    size_t remaining_bytes;
    bool failed;
    bool truncated;
} FlipPassOpenInflateReaderContext;

typedef struct {
    FlipPassOpenInflateXmlStageContext* stage;
    uint32_t crc32;
    size_t output_size;
    size_t expected_output_size;
    bool callback_failed;
    bool output_limit_failed;
} FlipPassOpenInflatePagedState;

static void fp_open_progress(
    FlipPassOpenInflateContext* ctx,
    const char* stage,
    const char* detail,
    uint8_t percent) {
    if(ctx == NULL || ctx->host_api == NULL || ctx->host_api->progress == NULL) {
        return;
    }

    ctx->progress_percent = percent;
    ctx->host_api->progress(ctx->host_api->context, stage, detail, percent);
}

static void fp_open_log(FlipPassOpenInflateContext* ctx, const char* message) {
    if(ctx != NULL && ctx->host_api != NULL && ctx->host_api->log != NULL && message != NULL) {
        ctx->host_api->log(ctx->host_api->context, message);
    }
}

static uint8_t fp_open_progress_percent(size_t completed, size_t total) {
    if(total == 0U) {
        return 0U;
    }

    if(completed >= total) {
        return 100U;
    }

    return (uint8_t)((completed * 100U) / total);
}

static uint32_t fp_open_crc32_update(uint32_t crc, const uint8_t* data, size_t data_size) {
    static const uint32_t poly = 0xEDB88320U;

    for(size_t i = 0; i < data_size; i++) {
        crc ^= data[i];
        for(uint8_t bit = 0; bit < 8U; bit++) {
            const uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (poly & mask);
        }
    }

    return crc;
}

static bool fp_open_stage_xml_output(const uint8_t* data, size_t data_size, void* context) {
    FlipPassOpenInflateXmlStageContext* stage = context;
    FlipPassOpenInflateContext* ctx = NULL;

    furi_assert(stage);
    ctx = stage->inflate_ctx;
    furi_assert(ctx);

    if(data == NULL) {
        return data_size == 0U;
    }

    if(!ctx->host_api->append_staged_xml(ctx->host_api->context, data, data_size, ctx->error)) {
        return false;
    }

    stage->plain_size += data_size;
    if(stage->expected_plain_size > 0U &&
       (stage->plain_size == data_size ||
        stage->plain_size >= (stage->last_progress_size + (stage->expected_plain_size / 8U)) ||
        stage->plain_size == stage->expected_plain_size)) {
        char detail[48];
        uint8_t percent =
            (uint8_t)(58U + ((stage->plain_size * 20U) / stage->expected_plain_size));
        if(percent > 78U) {
            percent = 78U;
        }
        snprintf(
            detail,
            sizeof(detail),
            "%u%% out",
            fp_open_progress_percent(stage->plain_size, stage->expected_plain_size));
        fp_open_progress(ctx, "Uncompressing", detail, percent);
        stage->last_progress_size = stage->plain_size;
    }

    return true;
}

static size_t fp_open_payload_reader_read(void* out, size_t capacity, void* context) {
    FlipPassOpenInflateReaderContext* reader = context;
    size_t out_size = 0U;

    if(reader == NULL || out == NULL || capacity == 0U || reader->failed) {
        return 0U;
    }

    while(reader->skip_bytes > 0U) {
        const size_t discard_capacity = (reader->skip_bytes < capacity) ? reader->skip_bytes :
                                                                          capacity;
        size_t discarded = 0U;

        if(!reader->host_api->read_staged_payload_stream(
               reader->host_api->context, out, discard_capacity, &discarded)) {
            reader->failed = true;
            return 0U;
        }
        if(discarded == 0U) {
            reader->failed = true;
            reader->truncated = true;
            return 0U;
        }

        reader->skip_bytes -= discarded;
    }

    if(reader->remaining_bytes == 0U) {
        return 0U;
    }

    const size_t request = (reader->remaining_bytes < capacity) ? reader->remaining_bytes :
                                                                  capacity;
    if(!reader->host_api->read_staged_payload_stream(
           reader->host_api->context, out, request, &out_size)) {
        reader->failed = true;
        return 0U;
    }
    if(out_size > reader->remaining_bytes) {
        reader->failed = true;
        return 0U;
    }
    if(out_size == 0U) {
        reader->failed = true;
        reader->truncated = true;
        return 0U;
    }

    reader->remaining_bytes -= out_size;

    return out_size;
}

static int fp_open_paged_output_callback(const void* data, int len, void* context) {
    FlipPassOpenInflatePagedState* state = context;
    const uint8_t* bytes = data;

    if(state == NULL || len < 0) {
        return 0;
    }
    if(len == 0) {
        return 1;
    }

    const size_t size = (size_t)len;
    const size_t next_output_size = state->output_size + size;
    if(next_output_size > state->expected_output_size) {
        state->output_limit_failed = true;
        return 0;
    }

    state->crc32 = fp_open_crc32_update(state->crc32, bytes, size);
    if(!fp_open_stage_xml_output(bytes, size, state->stage)) {
        state->callback_failed = true;
        return 0;
    }

    state->output_size = next_output_size;
    return 1;
}

static bool fp_open_emit_ram_paged(
    const FlipPassOpenInflateHostApiV1* host_api,
    const KDBXGzipMemberInfo* member_info,
    FlipPassOpenInflateXmlStageContext* stage,
    FuriString* error) {
    FlipPassOpenInflateReaderContext reader = {
        .host_api = host_api,
        .skip_bytes = member_info->body_offset,
        .remaining_bytes = member_info->compressed_size,
        .failed = false,
        .truncated = false,
    };
    FlipPassOpenInflatePagedState state;
    tinfl_paged_telemetry paged;
    size_t consumed_size = 0U;
    bool ok = false;

    memset(&state, 0, sizeof(state));
    state.stage = stage;
    state.crc32 = 0xFFFFFFFFU;
    state.expected_output_size = member_info->expected_output_size;

    memset(&paged, 0, sizeof(paged));
    ok = tinfl_decompress_reader_to_callback_paged_ex(
             fp_open_payload_reader_read,
             &reader,
             &consumed_size,
             fp_open_paged_output_callback,
             &state,
             0,
             &paged) != 0;

    if(!ok || reader.failed) {
        if(!reader.failed && state.output_limit_failed) {
            furi_string_set_str(
                error, "The decompressed XML payload did not match the GZip trailer.");
        } else if(state.callback_failed) {
            if(furi_string_empty(error)) {
                furi_string_set_str(
                    error, "The staged XML scratch rejected the inflated payload.");
            }
        } else if(reader.truncated) {
            if(furi_string_empty(error)) {
                furi_string_set_str(error, "The staged GZip member is truncated.");
            }
        } else if(reader.failed) {
            if(furi_string_empty(error)) {
                furi_string_set_str(error, "The staged GZip member could not be read safely.");
            }
        } else if(furi_string_empty(error)) {
            furi_string_set_str(error, "Unable to inflate the staged GZip payload.");
        }
        return false;
    }

    state.crc32 = ~state.crc32;
    if(consumed_size != member_info->compressed_size ||
       state.output_size != member_info->expected_output_size) {
        furi_string_set_str(error, "The decompressed XML payload did not match the GZip trailer.");
        return false;
    }

    if(state.crc32 != member_info->expected_crc32) {
        furi_string_set_str(error, "The decompressed XML CRC did not match the GZip trailer.");
        return false;
    }

    return true;
}

static const char* fp_open_window_path(KDBXVaultBackend backend) {
    switch(backend) {
    case KDBXVaultBackendFileInt:
        return KDBX_VAULT_WINDOW_INT_PATH;
    case KDBXVaultBackendFileExt:
        return KDBX_VAULT_WINDOW_EXT_PATH;
    default:
        return NULL;
    }
}

static KDBXVaultBackend fp_open_primary_window_backend(KDBXVaultBackend preferred_backend) {
    if(preferred_backend == KDBXVaultBackendFileInt) {
        return KDBXVaultBackendFileInt;
    }
    if(preferred_backend == KDBXVaultBackendFileExt) {
        return KDBXVaultBackendFileExt;
    }

    return KDBXVaultBackendFileExt;
}

static KDBXVaultBackend fp_open_secondary_window_backend(KDBXVaultBackend primary_backend) {
    return (primary_backend == KDBXVaultBackendFileInt) ? KDBXVaultBackendFileExt :
                                                          KDBXVaultBackendFileInt;
}

static bool fp_open_emit_file_paged(
    const FlipPassOpenInflateHostApiV1* host_api,
    const KDBXGzipMemberInfo* member_info,
    FlipPassOpenInflateXmlStageContext* stage,
    const char* window_path,
    FuriString* error) {
    FlipPassOpenInflateReaderContext reader = {
        .host_api = host_api,
        .skip_bytes = member_info->body_offset,
        .remaining_bytes = member_info->compressed_size,
        .failed = false,
        .truncated = false,
    };
    FlipPassOpenInflatePagedState state;
    tinfl_paged_telemetry paged;
    tinfl_paged_file_config file_config;
    size_t consumed_size = 0U;
    bool ok = false;

    if(window_path == NULL || window_path[0] == '\0') {
        furi_string_set_str(
            error, "No encrypted storage backend is available for paged inflate workspace.");
        return false;
    }

    memset(&state, 0, sizeof(state));
    state.stage = stage;
    state.crc32 = 0xFFFFFFFFU;
    state.expected_output_size = member_info->expected_output_size;

    memset(&paged, 0, sizeof(paged));
    memset(&file_config, 0, sizeof(file_config));
    file_config.file_path = window_path;
    file_config.storage = NULL;
    file_config.preferred_cache_pages = FLIPPASS_OPEN_GZIP_FILE_CACHE_PAGES;
    file_config.minimum_cache_pages = FLIPPASS_OPEN_GZIP_FILE_MIN_PAGES;

    ok = tinfl_decompress_reader_to_callback_file_paged_ex(
             fp_open_payload_reader_read,
             &reader,
             &consumed_size,
             fp_open_paged_output_callback,
             &state,
             0,
             &file_config,
             NULL,
             &paged) != 0;

    if(!ok || reader.failed) {
        if(!reader.failed && state.output_limit_failed) {
            furi_string_set_str(
                error, "The decompressed XML payload did not match the GZip trailer.");
        } else if(state.callback_failed) {
            if(furi_string_empty(error)) {
                furi_string_set_str(
                    error, "The staged XML scratch rejected the inflated payload.");
            }
        } else if(reader.truncated) {
            if(furi_string_empty(error)) {
                furi_string_set_str(error, "The staged GZip member is truncated.");
            }
        } else if(reader.failed) {
            if(furi_string_empty(error)) {
                furi_string_set_str(error, "The staged GZip member could not be read safely.");
            }
        } else if(furi_string_empty(error)) {
            furi_string_set_str(error, "Unable to inflate the staged GZip payload.");
        }
        return false;
    }

    state.crc32 = ~state.crc32;
    if(consumed_size != member_info->compressed_size ||
       state.output_size != member_info->expected_output_size) {
        furi_string_set_str(error, "The decompressed XML payload did not match the GZip trailer.");
        return false;
    }

    if(state.crc32 != member_info->expected_crc32) {
        furi_string_set_str(error, "The decompressed XML CRC did not match the GZip trailer.");
        return false;
    }

    return true;
}

static bool fp_open_run_inflate_attempt(
    FlipPassOpenInflateContext* ctx,
    const FlipPassOpenInflateRequestV1* request,
    bool file_paged,
    const char* window_path) {
    FlipPassOpenInflateXmlStageContext stage = {
        .inflate_ctx = ctx,
        .plain_size = 0U,
        .last_progress_size = 0U,
        .expected_plain_size = request->member_info.expected_output_size,
    };
    bool ok = false;

    if(!ctx->host_api->begin_staged_payload_stream(ctx->host_api->context, ctx->error)) {
        return false;
    }

    if(!ctx->host_api->begin_staged_xml(
           ctx->host_api->context, request->preferred_backend, ctx->error)) {
        ctx->host_api->end_staged_payload_stream(ctx->host_api->context);
        return false;
    }

    {
        char log_line[160];
        snprintf(
            log_line,
            sizeof(log_line),
            "STREAM_GZIP_INFLATE_BEGIN source=payload window=%s free=%lu max=%lu",
            file_paged ? "file_paged" : "ram_paged",
            (unsigned long)memmgr_get_free_heap(),
            (unsigned long)memmgr_heap_get_max_free_block());
        fp_open_log(ctx, log_line);
    }
    fp_open_progress(ctx, "Uncompressing", "", 58U);
    ok = file_paged ?
             fp_open_emit_file_paged(
                 ctx->host_api, &request->member_info, &stage, window_path, ctx->error) :
             fp_open_emit_ram_paged(ctx->host_api, &request->member_info, &stage, ctx->error);
    ctx->host_api->end_staged_payload_stream(ctx->host_api->context);

    if(!ok) {
        ctx->host_api->clear_staged_xml(ctx->host_api->context);
        return false;
    }

    if(!ctx->host_api->finish_staged_xml(ctx->host_api->context, stage.plain_size, ctx->error)) {
        ctx->host_api->clear_staged_xml(ctx->host_api->context);
        return false;
    }

    {
        char log_line[160];
        snprintf(
            log_line,
            sizeof(log_line),
            "STREAM_GZIP_INFLATE_OK out=%lu free=%lu max=%lu",
            (unsigned long)stage.plain_size,
            (unsigned long)memmgr_get_free_heap(),
            (unsigned long)memmgr_heap_get_max_free_block());
        fp_open_log(ctx, log_line);
    }

    return true;
}

static bool fp_open_inflate_paged_run(
    const FlipPassOpenInflateRequestV1* request,
    const FlipPassOpenInflateHostApiV1* host_api,
    FlipPassOpenInflateResultV1* result,
    FuriString* error) {
    FlipPassOpenInflateContext ctx = {
        .host_api = host_api,
        .error = error,
        .progress_percent = 0U,
    };
    const bool prefer_file_paged = request != NULL &&
                                   request->member_info.expected_output_size > (32U * 1024U);
    const KDBXVaultBackend primary_window_backend =
        (request != NULL) ? fp_open_primary_window_backend(request->preferred_backend) :
                            KDBXVaultBackendFileExt;
    const KDBXVaultBackend secondary_window_backend =
        fp_open_secondary_window_backend(primary_window_backend);
    const char* primary_window_path = fp_open_window_path(primary_window_backend);
    const char* secondary_window_path = (secondary_window_backend != primary_window_backend) ?
                                            fp_open_window_path(secondary_window_backend) :
                                            NULL;

    if(request == NULL || host_api == NULL || result == NULL || error == NULL ||
       request->api_version != FLIPPASS_OPEN_INFLATE_PLUGIN_API_VERSION ||
       host_api->api_version != FLIPPASS_OPEN_INFLATE_HOST_API_VERSION ||
       result->api_version != FLIPPASS_OPEN_INFLATE_PLUGIN_API_VERSION ||
       host_api->begin_staged_payload_stream == NULL ||
       host_api->read_staged_payload_stream == NULL ||
       host_api->end_staged_payload_stream == NULL || host_api->begin_staged_xml == NULL ||
       host_api->append_staged_xml == NULL || host_api->finish_staged_xml == NULL ||
       host_api->clear_staged_xml == NULL) {
        furi_string_set_str(error, "Open inflate ABI is unavailable or incompatible.");
        return false;
    }

    result->retry_with_paged = false;
    if(request->member_info.member_size <
           (request->member_info.body_offset + FLIPPASS_OPEN_GZIP_TRAILER_SIZE) ||
       request->member_info.body_offset + request->member_info.compressed_size +
               FLIPPASS_OPEN_GZIP_TRAILER_SIZE !=
           request->member_info.member_size ||
       request->member_info.expected_output_size == 0U ||
       request->member_info.expected_output_size > FLIPPASS_OPEN_MAX_XML_STREAM_BYTES) {
        furi_string_set_str(error, "The staged GZip member metadata is invalid.");
        return false;
    }

    fp_open_log(&ctx, "OPEN_STAGE inflate_paged");
    if(!prefer_file_paged && fp_open_run_inflate_attempt(&ctx, request, false, NULL)) {
        return true;
    }

    if(prefer_file_paged || !furi_string_empty(error)) {
        furi_string_reset(error);
    }

    if(fp_open_run_inflate_attempt(&ctx, request, true, primary_window_path)) {
        return true;
    }

    if(secondary_window_path != NULL) {
        furi_string_reset(error);
        return fp_open_run_inflate_attempt(&ctx, request, true, secondary_window_path);
    }

    return false;
}

static const FlipPassOpenInflatePluginV1 flippass_open_inflate_paged_plugin = {
    .api_version = FLIPPASS_OPEN_INFLATE_PLUGIN_API_VERSION,
    .run = fp_open_inflate_paged_run,
};

static const FlipperAppPluginDescriptor flippass_open_inflate_paged_descriptor = {
    .appid = FLIPPASS_OPEN_INFLATE_PAGED_PLUGIN_APP_ID,
    .ep_api_version = FLIPPASS_OPEN_INFLATE_PLUGIN_API_VERSION,
    .entry_point = &flippass_open_inflate_paged_plugin,
};

const FlipperAppPluginDescriptor* flippass_open_inflate_paged_plugin_ep(void) {
    return &flippass_open_inflate_paged_descriptor;
}
