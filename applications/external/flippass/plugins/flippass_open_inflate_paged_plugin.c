#include "flippass_open_inflate_plugin.h"
#include "flippass_open_inflate_reader.h"
#include "../flippass_build_config.h"

#include "../kdbx/memzero.h"
#include "../kdbx/miniz_tinfl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLIPPASS_OPEN_MAX_XML_STREAM_BYTES  (2U * 1024U * 1024U)
#define FLIPPASS_OPEN_GZIP_TRAILER_SIZE     8U
#define FLIPPASS_OPEN_GZIP_FILE_CACHE_PAGES 1U
#define FLIPPASS_OPEN_GZIP_FILE_MIN_PAGES   1U
#define FLIPPASS_OPEN_STORED_DEFLATE_CHUNK  128U

#ifndef FLIPPASS_OPEN_INFLATE_PAGED_ENABLE_RAM
#define FLIPPASS_OPEN_INFLATE_PAGED_ENABLE_RAM 1
#endif

#ifndef FLIPPASS_OPEN_INFLATE_PAGED_ENABLE_FILE
#define FLIPPASS_OPEN_INFLATE_PAGED_ENABLE_FILE 1
#endif

#if !FLIPPASS_OPEN_INFLATE_PAGED_ENABLE_RAM && !FLIPPASS_OPEN_INFLATE_PAGED_ENABLE_FILE
#error "At least one paged inflate backend must be enabled."
#endif

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

#if FLIPPASS_ENABLE_LOGS || FLIPPASS_ENABLE_PROBE_LOGS
static void fp_open_log(FlipPassOpenInflateContext* ctx, const char* message) {
    if(ctx != NULL && ctx->host_api != NULL && ctx->host_api->log != NULL && message != NULL) {
        ctx->host_api->log(ctx->host_api->context, message);
    }
}
#else
#define fp_open_log(ctx, message) ((void)(ctx))
#endif

static void fp_open_memory_log(
    FlipPassOpenInflateContext* ctx,
    const char* stage,
    size_t theoretical_bytes) {
#if FLIPPASS_ENABLE_MEMORY_DIAGNOSTICS && FLIPPASS_ENABLE_LOGS
    char log_line[176];
    snprintf(
        log_line,
        sizeof(log_line),
        "MEMORY stage=%s free=%lu max=%lu theoretical=%lu loaded=plugin:open_inflate_paged",
        stage != NULL ? stage : "open_inflate_paged",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block(),
        (unsigned long)theoretical_bytes);
    fp_open_log(ctx, log_line);
#else
    UNUSED(ctx);
    UNUSED(stage);
    UNUSED(theoretical_bytes);
#endif
}

static size_t fp_open_paged_theoretical_bytes(bool file_paged) {
    const size_t dict_window = file_paged ? (2U * TINFL_PAGED_LZ_DICT_PAGE_SIZE) :
                                            TINFL_LZ_DICT_SIZE;
    return sizeof(FlipPassOpenInflateContext) + sizeof(FlipPassOpenInflateXmlStageContext) +
           sizeof(FlipPassOpenInflateReaderContext) + sizeof(FlipPassOpenInflatePagedState) +
           sizeof(tinfl_paged_telemetry) + sizeof(tinfl_paged_file_config) +
           sizeof(tinfl_decompressor) + dict_window;
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

#if FLIPPASS_OPEN_INFLATE_PAGED_ENABLE_RAM
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
#if FLIPPASS_ENABLE_LOGS || FLIPPASS_ENABLE_PROBE_LOGS
    if(host_api->log != NULL) {
        char log_line[192];
        snprintf(
            log_line,
            sizeof(log_line),
            "STREAM_GZIP_RAM_PAGED_RESULT ok=%u status=%d consumed=%lu out=%lu loops=%lu flush=%lu storage=%d failed=%u truncated=%u",
            ok ? 1U : 0U,
            paged.last_status,
            (unsigned long)consumed_size,
            (unsigned long)state.output_size,
            (unsigned long)paged.loop_count,
            (unsigned long)paged.flush_count,
            paged.storage_failed,
            reader.failed ? 1U : 0U,
            reader.truncated ? 1U : 0U);
        host_api->log(host_api->context, log_line);
    }
#endif

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
#endif

#if FLIPPASS_OPEN_INFLATE_PAGED_ENABLE_FILE
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
#if !TINFL_FILE_PAGED_ENABLE_LOCAL_CRYPTO
    file_config.crypt_page = host_api->crypt_paged_window;
    file_config.crypt_context = host_api->context;
#endif

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
    if(!TINFL_FILE_PAGED_ENABLE_LOCAL_CRYPTO && host_api->clear_paged_window_crypto != NULL) {
        host_api->clear_paged_window_crypto(host_api->context);
    }
#if FLIPPASS_ENABLE_LOGS || FLIPPASS_ENABLE_PROBE_LOGS
    if(host_api->log != NULL) {
        char log_line[224];
        snprintf(
            log_line,
            sizeof(log_line),
            "STREAM_GZIP_FILE_PAGED_RESULT ok=%u status=%d consumed=%lu out=%lu loops=%lu flush=%lu storage=%d stage=%s failed=%u truncated=%u path=%s",
            ok ? 1U : 0U,
            paged.last_status,
            (unsigned long)consumed_size,
            (unsigned long)state.output_size,
            (unsigned long)paged.loop_count,
            (unsigned long)paged.flush_count,
            paged.storage_failed,
            paged.storage_stage != NULL ? paged.storage_stage : "-",
            reader.failed ? 1U : 0U,
            reader.truncated ? 1U : 0U,
            window_path != NULL ? window_path : "-");
        host_api->log(host_api->context, log_line);
    }
#endif

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
#endif

static bool fp_open_run_inflate_attempt(
    FlipPassOpenInflateContext* ctx,
    const FlipPassOpenInflateRequestV1* request,
    bool file_paged,
    const char* window_path) {
#if FLIPPASS_OPEN_INFLATE_PAGED_ENABLE_RAM
    const bool use_file_paged = file_paged;
#else
    UNUSED(file_paged);
    const bool use_file_paged = true;
#endif
    FlipPassOpenInflateXmlStageContext stage = {
        .inflate_ctx = ctx,
        .plain_size = 0U,
        .last_progress_size = 0U,
        .expected_plain_size = request->member_info.expected_output_size,
    };
    bool ok = false;

    fp_open_memory_log(
        ctx,
        use_file_paged ? "open_inflate_file_paged_attempt_begin" :
                         "open_inflate_ram_paged_attempt_begin",
        fp_open_paged_theoretical_bytes(use_file_paged));
    if(!ctx->host_api->begin_staged_payload_stream(ctx->host_api->context, ctx->error)) {
        return false;
    }

    if(!ctx->host_api->begin_staged_xml(
           ctx->host_api->context, request->preferred_backend, ctx->error)) {
        ctx->host_api->end_staged_payload_stream(ctx->host_api->context);
        return false;
    }

#if FLIPPASS_ENABLE_LOGS || FLIPPASS_ENABLE_PROBE_LOGS
    {
        char log_line[160];
        snprintf(
            log_line,
            sizeof(log_line),
            "STREAM_GZIP_INFLATE_BEGIN source=payload window=%s free=%lu max=%lu",
            use_file_paged ? "file_paged" : "ram_paged",
            (unsigned long)memmgr_get_free_heap(),
            (unsigned long)memmgr_heap_get_max_free_block());
        fp_open_log(ctx, log_line);
    }
#endif
    fp_open_progress(ctx, "Uncompressing", "", 58U);
    if(use_file_paged) {
#if FLIPPASS_OPEN_INFLATE_PAGED_ENABLE_FILE
        ok = fp_open_emit_file_paged(
            ctx->host_api, &request->member_info, &stage, window_path, ctx->error);
#else
        furi_string_set_str(ctx->error, "The file-backed paged GZip inflator is not enabled.");
        ok = false;
#endif
    } else {
#if FLIPPASS_OPEN_INFLATE_PAGED_ENABLE_RAM
        ok = fp_open_emit_ram_paged(ctx->host_api, &request->member_info, &stage, ctx->error);
#else
        furi_string_set_str(ctx->error, "The RAM-paged GZip inflator is not enabled.");
        ok = false;
#endif
    }
    ctx->host_api->end_staged_payload_stream(ctx->host_api->context);

    if(!ok) {
        ctx->host_api->clear_staged_xml(ctx->host_api->context);
        fp_open_memory_log(
            ctx,
            use_file_paged ? "open_inflate_file_paged_attempt_fail" :
                             "open_inflate_ram_paged_attempt_fail",
            fp_open_paged_theoretical_bytes(use_file_paged));
        return false;
    }

    if(!ctx->host_api->finish_staged_xml(ctx->host_api->context, stage.plain_size, ctx->error)) {
        ctx->host_api->clear_staged_xml(ctx->host_api->context);
        fp_open_memory_log(
            ctx,
            use_file_paged ? "open_inflate_file_paged_finish_fail" :
                             "open_inflate_ram_paged_finish_fail",
            fp_open_paged_theoretical_bytes(use_file_paged));
        return false;
    }

#if FLIPPASS_ENABLE_LOGS || FLIPPASS_ENABLE_PROBE_LOGS
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
#endif

    fp_open_memory_log(
        ctx,
        use_file_paged ? "open_inflate_file_paged_attempt_ok" :
                         "open_inflate_ram_paged_attempt_ok",
        fp_open_paged_theoretical_bytes(use_file_paged));
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
#if FLIPPASS_OPEN_INFLATE_PAGED_ENABLE_RAM
    const bool prefer_file_paged =
        request != NULL &&
        (request->member_info.expected_output_size > (32U * 1024U) ||
         memmgr_heap_get_max_free_block() < (TINFL_LZ_DICT_SIZE + (4U * 1024U)));
#else
    const bool prefer_file_paged = true;
#endif
#if FLIPPASS_OPEN_INFLATE_PAGED_ENABLE_FILE
    const KDBXVaultBackend primary_window_backend =
        (request != NULL) ? fp_open_primary_window_backend(request->preferred_backend) :
                            KDBXVaultBackendFileExt;
    const KDBXVaultBackend secondary_window_backend =
        fp_open_secondary_window_backend(primary_window_backend);
    const char* primary_window_path = fp_open_window_path(primary_window_backend);
    const char* secondary_window_path = (secondary_window_backend != primary_window_backend) ?
                                            fp_open_window_path(secondary_window_backend) :
                                            NULL;
#endif

    if(request == NULL || host_api == NULL || result == NULL || error == NULL ||
       request->api_version != FLIPPASS_OPEN_INFLATE_PLUGIN_API_VERSION ||
       host_api->api_version != FLIPPASS_OPEN_INFLATE_HOST_API_VERSION ||
       result->api_version != FLIPPASS_OPEN_INFLATE_PLUGIN_API_VERSION ||
       host_api->begin_staged_payload_stream == NULL ||
       host_api->read_staged_payload_stream == NULL ||
       host_api->end_staged_payload_stream == NULL || host_api->begin_staged_xml == NULL ||
       host_api->append_staged_xml == NULL || host_api->finish_staged_xml == NULL ||
       host_api->clear_staged_xml == NULL || host_api->backend_supported == NULL ||
       (!TINFL_FILE_PAGED_ENABLE_LOCAL_CRYPTO && host_api->crypt_paged_window == NULL)) {
        furi_string_set_str(error, "Open inflate ABI is unavailable or incompatible.");
        return false;
    }

    result->retry_with_paged = false;
    fp_open_memory_log(
        &ctx, "open_inflate_paged_entry", fp_open_paged_theoretical_bytes(prefer_file_paged));
    if(request->member_info.member_size < FLIPPASS_OPEN_GZIP_TRAILER_SIZE ||
       request->member_info.body_offset >
           request->member_info.member_size - FLIPPASS_OPEN_GZIP_TRAILER_SIZE ||
       request->member_info.compressed_size != request->member_info.member_size -
                                                   FLIPPASS_OPEN_GZIP_TRAILER_SIZE -
                                                   request->member_info.body_offset ||
       request->member_info.expected_output_size == 0U ||
       request->member_info.expected_output_size > FLIPPASS_OPEN_MAX_XML_STREAM_BYTES) {
        furi_string_set_str(error, "The staged GZip member metadata is invalid.");
        return false;
    }

    fp_open_log(&ctx, "OPEN_STAGE inflate_paged");
#if FLIPPASS_OPEN_INFLATE_PAGED_ENABLE_RAM
    if(!prefer_file_paged && fp_open_run_inflate_attempt(&ctx, request, false, NULL)) {
        return true;
    }
#endif

    if(prefer_file_paged || !furi_string_empty(error)) {
        furi_string_reset(error);
    }

#if FLIPPASS_OPEN_INFLATE_PAGED_ENABLE_FILE
    const bool primary_supported = host_api->backend_supported(primary_window_backend);
    const bool secondary_supported = host_api->backend_supported(secondary_window_backend);
    if(primary_supported &&
       fp_open_run_inflate_attempt(&ctx, request, true, primary_window_path)) {
        return true;
    }

    if(secondary_supported && secondary_window_path != NULL) {
        furi_string_reset(error);
        return fp_open_run_inflate_attempt(&ctx, request, true, secondary_window_path);
    }
    if(!primary_supported && !secondary_supported) {
        furi_string_set_str(
            error, "No supported encrypted storage backend is available for paged inflate.");
    }
#else
    furi_string_set_str(error, "The file-backed paged GZip inflator is not enabled.");
#endif

    fp_open_memory_log(&ctx, "open_inflate_paged_exit_fail", 0U);
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
