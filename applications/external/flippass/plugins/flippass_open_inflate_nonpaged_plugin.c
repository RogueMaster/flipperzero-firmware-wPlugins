#include "flippass_open_inflate_plugin.h"

#include "../kdbx/miniz_tinfl.h"
#include "../kdbx/memzero.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLIPPASS_OPEN_MAX_XML_STREAM_BYTES       (2U * 1024U * 1024U)
#define FLIPPASS_OPEN_GZIP_TRAILER_SIZE          8U
#define FLIPPASS_OPEN_GZIP_NONPAGED_MARGIN_BYTES (2U * 1024U)
#define FLIPPASS_OPEN_GZIP_NONPAGED_PLAIN_LIMIT  (16U * 1024U)

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
} FlipPassOpenInflateNonPagedState;

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

static void fp_open_memory_log(
    FlipPassOpenInflateContext* ctx,
    const char* stage,
    size_t theoretical_bytes) {
#if FLIPPASS_ENABLE_MEMORY_DIAGNOSTICS && FLIPPASS_ENABLE_LOGS
    char log_line[176];
    snprintf(
        log_line,
        sizeof(log_line),
        "MEMORY stage=%s free=%lu max=%lu theoretical=%lu loaded=plugin:open_inflate_nonpaged",
        stage != NULL ? stage : "open_inflate_nonpaged",
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

static size_t fp_open_nonpaged_theoretical_bytes(const KDBXGzipMemberInfo* member_info) {
    const size_t member_size = (member_info != NULL) ? member_info->member_size : 0U;
    return sizeof(FlipPassOpenInflateContext) + sizeof(FlipPassOpenInflateXmlStageContext) +
           sizeof(FlipPassOpenInflateNonPagedState) + member_size + TINFL_LZ_DICT_SIZE +
           sizeof(tinfl_decompressor);
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

static bool fp_open_nonpaged_member_window_fits(size_t member_size) {
    const size_t required_max =
        member_size + TINFL_LZ_DICT_SIZE + FLIPPASS_OPEN_GZIP_NONPAGED_MARGIN_BYTES;
    return memmgr_heap_get_max_free_block() >= required_max;
}

static bool fp_open_nonpaged_dict_window_fits(void) {
    const size_t required_max = TINFL_LZ_DICT_SIZE + FLIPPASS_OPEN_GZIP_NONPAGED_MARGIN_BYTES;
    return memmgr_heap_get_max_free_block() >= required_max;
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
        char detail[32];
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

static bool fp_open_load_payload_to_heap(
    FlipPassOpenInflateContext* ctx,
    size_t payload_size,
    uint8_t** out_data) {
    uint8_t* data = NULL;
    size_t offset = 0U;
    bool ok = false;

    furi_assert(ctx);
    furi_assert(out_data);
    *out_data = NULL;

    data = malloc(payload_size);
    if(data == NULL) {
        if(furi_string_empty(ctx->error)) {
            furi_string_set_str(
                ctx->error, "Not enough RAM is available to load the staged GZip member.");
        }
        return false;
    }

    if(!ctx->host_api->begin_staged_payload_stream(ctx->host_api->context, ctx->error)) {
        goto cleanup;
    }

    while(offset < payload_size) {
        size_t chunk_size = 0U;
        if(!ctx->host_api->read_staged_payload_stream(
               ctx->host_api->context, data + offset, payload_size - offset, &chunk_size)) {
            if(furi_string_empty(ctx->error)) {
                furi_string_set_str(
                    ctx->error, "The staged GZip member could not be read safely.");
            }
            ctx->host_api->end_staged_payload_stream(ctx->host_api->context);
            goto cleanup;
        }
        if(chunk_size == 0U) {
            break;
        }
        offset += chunk_size;
    }

    ctx->host_api->end_staged_payload_stream(ctx->host_api->context);
    if(offset != payload_size) {
        if(furi_string_empty(ctx->error)) {
            furi_string_set_str(ctx->error, "The staged GZip member is truncated.");
        }
        goto cleanup;
    }

    *out_data = data;
    data = NULL;
    ok = true;

cleanup:
    if(data != NULL) {
        memzero(data, payload_size);
        free(data);
    }
    return ok;
}

static int fp_open_nonpaged_output_callback(const void* data, int len, void* context) {
    FlipPassOpenInflateNonPagedState* state = context;
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

static bool fp_open_emit_nonpaged_deflate(
    const uint8_t* compressed_data,
    size_t compressed_size,
    uint32_t expected_crc32,
    uint32_t expected_output_size,
    FlipPassOpenInflateXmlStageContext* stage,
    bool* out_resource_failure,
    FuriString* error) {
    tinfl_decompressor* decomp = NULL;
    uint8_t* dict = NULL;
    FlipPassOpenInflateNonPagedState state;
    size_t input_offset = 0U;
    size_t dict_offset = 0U;
    bool ok = false;

    furi_assert(stage);
    furi_assert(out_resource_failure);
    furi_assert(error);
    *out_resource_failure = false;

    decomp = malloc(sizeof(*decomp));
    if(decomp == NULL) {
        *out_resource_failure = true;
        return false;
    }
    memset(decomp, 0, sizeof(*decomp));
    tinfl_init(decomp);

    dict = malloc(TINFL_LZ_DICT_SIZE);
    if(dict == NULL) {
        *out_resource_failure = true;
        goto cleanup;
    }
    memset(dict, 0, TINFL_LZ_DICT_SIZE);

    memset(&state, 0, sizeof(state));
    state.stage = stage;
    state.crc32 = 0xFFFFFFFFU;
    state.expected_output_size = expected_output_size;

    for(;;) {
        size_t input_size = compressed_size - input_offset;
        size_t output_size = TINFL_LZ_DICT_SIZE - dict_offset;
        const tinfl_status status = tinfl_decompress(
            decomp,
            compressed_data + input_offset,
            &input_size,
            dict,
            dict + dict_offset,
            &output_size,
            0);

        input_offset += input_size;
        if(output_size > 0U &&
           !fp_open_nonpaged_output_callback(dict + dict_offset, (int)output_size, &state)) {
            break;
        }

        if(status != TINFL_STATUS_HAS_MORE_OUTPUT) {
            ok = (status == TINFL_STATUS_DONE);
            break;
        }
        dict_offset = (dict_offset + output_size) & (TINFL_LZ_DICT_SIZE - 1U);
    }

    if(!ok) {
        if(state.output_limit_failed) {
            furi_string_set_str(
                error, "The decompressed XML payload did not match the GZip trailer.");
        } else if(state.callback_failed) {
            if(furi_string_empty(error)) {
                furi_string_set_str(
                    error, "The staged XML scratch rejected the inflated payload.");
            }
        } else if(furi_string_empty(error)) {
            furi_string_set_str(error, "Unable to inflate the staged GZip payload.");
        }
        goto cleanup;
    }

    state.crc32 = ~state.crc32;
    if(input_offset != compressed_size || state.output_size != expected_output_size) {
        furi_string_set_str(error, "The decompressed XML payload did not match the GZip trailer.");
        goto cleanup;
    }

    if(state.crc32 != expected_crc32) {
        furi_string_set_str(error, "The decompressed XML CRC did not match the GZip trailer.");
        goto cleanup;
    }

    ok = true;

cleanup:
    if(dict != NULL) {
        memzero(dict, TINFL_LZ_DICT_SIZE);
        free(dict);
    }
    if(decomp != NULL) {
        memzero(decomp, sizeof(*decomp));
        free(decomp);
    }
    return ok;
}

static bool fp_open_inflate_nonpaged_run(
    const FlipPassOpenInflateRequestV1* request,
    const FlipPassOpenInflateHostApiV1* host_api,
    FlipPassOpenInflateResultV1* result,
    FuriString* error) {
    FlipPassOpenInflateContext ctx = {
        .host_api = host_api,
        .error = error,
        .progress_percent = 0U,
    };
    FlipPassOpenInflateXmlStageContext stage = {
        .inflate_ctx = &ctx,
        .plain_size = 0U,
        .last_progress_size = 0U,
        .expected_plain_size = 0U,
    };
    uint8_t* member_data = NULL;
    bool resource_failure = false;
    bool ok = false;

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
    fp_open_memory_log(
        &ctx,
        "open_inflate_nonpaged_entry",
        fp_open_nonpaged_theoretical_bytes(request != NULL ? &request->member_info : NULL));
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

    fp_open_log(&ctx, "OPEN_STAGE inflate_nonpaged");
    if(request->member_info.expected_output_size > FLIPPASS_OPEN_GZIP_NONPAGED_PLAIN_LIMIT) {
        result->retry_with_paged = true;
        if(furi_string_empty(error)) {
            furi_string_set_str(
                error, "The staged GZip payload exceeds the safe nonpaged inflate window.");
        }
        return false;
    }

    if(!fp_open_nonpaged_member_window_fits(request->member_info.member_size)) {
        fp_open_memory_log(
            &ctx,
            "open_inflate_nonpaged_window_too_small",
            fp_open_nonpaged_theoretical_bytes(&request->member_info));
        result->retry_with_paged = true;
        if(furi_string_empty(error)) {
            furi_string_set_str(
                error, "The staged GZip member no longer fits the nonpaged inflate window.");
        }
        return false;
    }

    fp_open_progress(&ctx, "Loading GZip", "", 54U);
    fp_open_memory_log(
        &ctx,
        "open_inflate_nonpaged_payload_load_before",
        fp_open_nonpaged_theoretical_bytes(&request->member_info));
    if(!fp_open_load_payload_to_heap(&ctx, request->member_info.member_size, &member_data)) {
        result->retry_with_paged = true;
        return false;
    }
    fp_open_memory_log(
        &ctx,
        "open_inflate_nonpaged_payload_load_after",
        fp_open_nonpaged_theoretical_bytes(&request->member_info));

    if(!fp_open_nonpaged_dict_window_fits()) {
        fp_open_memory_log(
            &ctx,
            "open_inflate_nonpaged_dict_too_small",
            fp_open_nonpaged_theoretical_bytes(&request->member_info));
        result->retry_with_paged = true;
        if(furi_string_empty(error)) {
            furi_string_set_str(
                error,
                "The staged GZip member no longer leaves room for the nonpaged dictionary.");
        }
        goto cleanup;
    }

    if(!host_api->begin_staged_xml(host_api->context, request->preferred_backend, error)) {
        result->retry_with_paged = true;
        goto cleanup;
    }

    stage.expected_plain_size = request->member_info.expected_output_size;
    {
        char log_line[160];
        snprintf(
            log_line,
            sizeof(log_line),
            "STREAM_GZIP_INFLATE_BEGIN source=payload window=nonpaged free=%lu max=%lu",
            (unsigned long)memmgr_get_free_heap(),
            (unsigned long)memmgr_heap_get_max_free_block());
        fp_open_log(&ctx, log_line);
    }
    fp_open_progress(&ctx, "Uncompressing", "", 58U);
    fp_open_memory_log(
        &ctx,
        "open_inflate_nonpaged_emit_before",
        fp_open_nonpaged_theoretical_bytes(&request->member_info));
    ok = fp_open_emit_nonpaged_deflate(
        member_data + request->member_info.body_offset,
        request->member_info.compressed_size,
        request->member_info.expected_crc32,
        request->member_info.expected_output_size,
        &stage,
        &resource_failure,
        error);
    if(!ok) {
        if(resource_failure) {
            result->retry_with_paged = true;
        }
        host_api->clear_staged_xml(host_api->context);
        goto cleanup;
    }
    fp_open_memory_log(
        &ctx,
        "open_inflate_nonpaged_emit_after",
        fp_open_nonpaged_theoretical_bytes(&request->member_info));

    if(!host_api->finish_staged_xml(host_api->context, stage.plain_size, error)) {
        result->retry_with_paged = true;
        host_api->clear_staged_xml(host_api->context);
        ok = false;
        goto cleanup;
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
        fp_open_log(&ctx, log_line);
    }

cleanup:
    if(member_data != NULL) {
        memzero(member_data, request->member_info.member_size);
        free(member_data);
    }
    fp_open_memory_log(&ctx, "open_inflate_nonpaged_exit", 0U);
    return ok;
}

static const FlipPassOpenInflatePluginV1 flippass_open_inflate_nonpaged_plugin = {
    .api_version = FLIPPASS_OPEN_INFLATE_PLUGIN_API_VERSION,
    .run = fp_open_inflate_nonpaged_run,
};

static const FlipperAppPluginDescriptor flippass_open_inflate_nonpaged_descriptor = {
    .appid = FLIPPASS_OPEN_INFLATE_NONPAGED_PLUGIN_APP_ID,
    .ep_api_version = FLIPPASS_OPEN_INFLATE_PLUGIN_API_VERSION,
    .entry_point = &flippass_open_inflate_nonpaged_plugin,
};

const FlipperAppPluginDescriptor* flippass_open_inflate_nonpaged_plugin_ep(void) {
    return &flippass_open_inflate_nonpaged_descriptor;
}
