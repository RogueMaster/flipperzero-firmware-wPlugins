#include "flippass_open_stream_plugin.h"

#include "../kdbx/kdbx_open_stream.h"
#include "../kdbx/memzero.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLIPPASS_OPEN_MAX_XML_STREAM_BYTES       (2U * 1024U * 1024U)
#define FLIPPASS_OPEN_GZIP_HEADER_SIZE           10U
#define FLIPPASS_OPEN_GZIP_TRAILER_SIZE          8U
#define FLIPPASS_OPEN_GZIP_MEMBER_PREFIX_BYTES   512U
#define FLIPPASS_OPEN_GZIP_EXPANDED_PREFIX_BYTES (16U * 1024U)
#define FLIPPASS_OPEN_GZIP_NONPAGED_LIMIT        (16U * 1024U)
#define FLIPPASS_OPEN_GZIP_NONPAGED_PLAIN_LIMIT  (16U * 1024U)
#define FLIPPASS_OPEN_GZIP_NONPAGED_DICT_BYTES   (32U * 1024U)
#define FLIPPASS_OPEN_GZIP_NONPAGED_MARGIN_BYTES (2U * 1024U)
#define FLIPPASS_OPEN_GZIP_ID1                   0x1FU
#define FLIPPASS_OPEN_GZIP_ID2                   0x8BU
#define FLIPPASS_OPEN_GZIP_CM_DEFLATE            8U
#define FLIPPASS_OPEN_GZIP_FLAG_FHCRC            0x02U
#define FLIPPASS_OPEN_GZIP_FLAG_FEXTRA           0x04U
#define FLIPPASS_OPEN_GZIP_FLAG_FNAME            0x08U
#define FLIPPASS_OPEN_GZIP_FLAG_FCOMMENT         0x10U
#define FLIPPASS_OPEN_GZIP_FLAG_RESERVED         0xE0U

typedef enum {
    FlipPassOpenGzipParseStatusOk = 0,
    FlipPassOpenGzipParseStatusTruncatedInput,
    FlipPassOpenGzipParseStatusInvalidHeader,
    FlipPassOpenGzipParseStatusReservedFlags,
    FlipPassOpenGzipParseStatusInvalidExtraField,
    FlipPassOpenGzipParseStatusInvalidNameField,
    FlipPassOpenGzipParseStatusInvalidCommentField,
    FlipPassOpenGzipParseStatusInvalidHeaderCrcField,
    FlipPassOpenGzipParseStatusInvalidBodyOffset,
    FlipPassOpenGzipParseStatusOutputTooLarge,
} FlipPassOpenGzipParseStatus;

typedef struct {
    const FlipPassOpenStreamHostApiV1* host_api;
    FuriString* error;
    uint8_t progress_percent;
} FlipPassOpenStreamContext;

typedef struct {
    FlipPassOpenStreamContext* stream_ctx;
    size_t plain_size;
} FlipPassOpenXmlStageContext;

typedef struct {
    FlipPassOpenStreamContext* stream_ctx;
    size_t total_bytes;
    size_t prefix_len;
    size_t trailer_len;
    uint8_t prefix[FLIPPASS_OPEN_GZIP_MEMBER_PREFIX_BYTES];
    uint8_t trailer[FLIPPASS_OPEN_GZIP_TRAILER_SIZE];
} FlipPassOpenGzipStageContext;

static void fp_open_progress(
    FlipPassOpenStreamContext* ctx,
    const char* stage,
    const char* detail,
    uint8_t percent) {
    if(ctx == NULL || ctx->host_api == NULL || ctx->host_api->progress == NULL) {
        return;
    }

    ctx->progress_percent = percent;
    ctx->host_api->progress(ctx->host_api->context, stage, detail, percent);
}

static void fp_open_log(FlipPassOpenStreamContext* ctx, const char* message) {
    if(ctx != NULL && ctx->host_api != NULL && ctx->host_api->log != NULL && message != NULL) {
        ctx->host_api->log(ctx->host_api->context, message);
    }
}

static uint16_t fp_open_gzip_read_u16_le(const uint8_t* data) {
    return ((uint16_t)data[0]) | ((uint16_t)data[1] << 8);
}

static uint32_t fp_open_gzip_read_u32_le(const uint8_t* data) {
    return ((uint32_t)data[0]) | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static bool
    fp_open_gzip_skip_zero_terminated(const uint8_t* data, size_t data_size, size_t* offset) {
    while(*offset < data_size) {
        if(data[*offset] == '\0') {
            (*offset)++;
            return true;
        }
        (*offset)++;
    }

    return false;
}

static FlipPassOpenGzipParseStatus fp_open_gzip_parse_member_info(
    const uint8_t* prefix,
    size_t prefix_size,
    const uint8_t trailer[FLIPPASS_OPEN_GZIP_TRAILER_SIZE],
    size_t member_size,
    KDBXGzipMemberInfo* out_info) {
    uint8_t flags = 0U;
    size_t offset = 0U;
    size_t trailer_offset = 0U;

    furi_assert(out_info);
    memset(out_info, 0, sizeof(*out_info));

    if(prefix == NULL || trailer == NULL || prefix_size < FLIPPASS_OPEN_GZIP_HEADER_SIZE ||
       member_size < (FLIPPASS_OPEN_GZIP_HEADER_SIZE + FLIPPASS_OPEN_GZIP_TRAILER_SIZE)) {
        return FlipPassOpenGzipParseStatusTruncatedInput;
    }

    if(prefix[0] != FLIPPASS_OPEN_GZIP_ID1 || prefix[1] != FLIPPASS_OPEN_GZIP_ID2 ||
       prefix[2] != FLIPPASS_OPEN_GZIP_CM_DEFLATE) {
        return FlipPassOpenGzipParseStatusInvalidHeader;
    }

    flags = prefix[3];
    if((flags & FLIPPASS_OPEN_GZIP_FLAG_RESERVED) != 0U) {
        return FlipPassOpenGzipParseStatusReservedFlags;
    }

    offset = FLIPPASS_OPEN_GZIP_HEADER_SIZE;
    trailer_offset = member_size - FLIPPASS_OPEN_GZIP_TRAILER_SIZE;

    if(flags & FLIPPASS_OPEN_GZIP_FLAG_FEXTRA) {
        size_t extra_size = 0U;
        if(offset + 2U > prefix_size || offset + 2U > trailer_offset) {
            return FlipPassOpenGzipParseStatusInvalidExtraField;
        }

        extra_size = fp_open_gzip_read_u16_le(prefix + offset);
        offset += 2U;
        if(offset + extra_size > prefix_size || offset + extra_size > trailer_offset) {
            return FlipPassOpenGzipParseStatusInvalidExtraField;
        }
        offset += extra_size;
    }

    if((flags & FLIPPASS_OPEN_GZIP_FLAG_FNAME) &&
       (!fp_open_gzip_skip_zero_terminated(prefix, prefix_size, &offset) ||
        offset > trailer_offset)) {
        return FlipPassOpenGzipParseStatusInvalidNameField;
    }

    if((flags & FLIPPASS_OPEN_GZIP_FLAG_FCOMMENT) &&
       (!fp_open_gzip_skip_zero_terminated(prefix, prefix_size, &offset) ||
        offset > trailer_offset)) {
        return FlipPassOpenGzipParseStatusInvalidCommentField;
    }

    if(flags & FLIPPASS_OPEN_GZIP_FLAG_FHCRC) {
        if(offset + 2U > prefix_size || offset + 2U > trailer_offset) {
            return FlipPassOpenGzipParseStatusInvalidHeaderCrcField;
        }
        offset += 2U;
    }

    if(offset >= trailer_offset) {
        return FlipPassOpenGzipParseStatusInvalidBodyOffset;
    }

    out_info->member_size = member_size;
    out_info->body_offset = offset;
    out_info->compressed_size = trailer_offset - offset;
    out_info->expected_crc32 = fp_open_gzip_read_u32_le(trailer);
    out_info->expected_output_size = fp_open_gzip_read_u32_le(trailer + 4U);
    if(out_info->expected_output_size == 0U ||
       out_info->expected_output_size > FLIPPASS_OPEN_MAX_XML_STREAM_BYTES) {
        memset(out_info, 0, sizeof(*out_info));
        return FlipPassOpenGzipParseStatusOutputTooLarge;
    }

    return FlipPassOpenGzipParseStatusOk;
}

static bool fp_open_gzip_parse_can_retry_with_expanded_prefix(FlipPassOpenGzipParseStatus status) {
    return status == FlipPassOpenGzipParseStatusTruncatedInput ||
           status == FlipPassOpenGzipParseStatusInvalidExtraField ||
           status == FlipPassOpenGzipParseStatusInvalidNameField ||
           status == FlipPassOpenGzipParseStatusInvalidCommentField ||
           status == FlipPassOpenGzipParseStatusInvalidHeaderCrcField;
}

static const char* fp_open_gzip_parse_error_message(FlipPassOpenGzipParseStatus status) {
    switch(status) {
    case FlipPassOpenGzipParseStatusOutputTooLarge:
        return "The decompressed XML payload exceeds FlipPass's safe limit.";
    case FlipPassOpenGzipParseStatusInvalidHeader:
    case FlipPassOpenGzipParseStatusReservedFlags:
    case FlipPassOpenGzipParseStatusInvalidExtraField:
    case FlipPassOpenGzipParseStatusInvalidNameField:
    case FlipPassOpenGzipParseStatusInvalidCommentField:
    case FlipPassOpenGzipParseStatusInvalidHeaderCrcField:
    case FlipPassOpenGzipParseStatusInvalidBodyOffset:
    case FlipPassOpenGzipParseStatusTruncatedInput:
    default:
        return "The GZip member could not be analyzed safely.";
    }
}

static FlipPassOpenInflateKind
    fp_open_select_inflate_kind(size_t member_size, uint32_t expected_output_size) {
    const size_t max_free_block = memmgr_heap_get_max_free_block();
    const size_t nonpaged_required_max = member_size + FLIPPASS_OPEN_GZIP_NONPAGED_DICT_BYTES +
                                         FLIPPASS_OPEN_GZIP_NONPAGED_MARGIN_BYTES;

    if(member_size == 0U || expected_output_size == 0U ||
       member_size > FLIPPASS_OPEN_GZIP_NONPAGED_LIMIT ||
       expected_output_size > FLIPPASS_OPEN_GZIP_NONPAGED_PLAIN_LIMIT) {
        return FlipPassOpenInflateKindPaged;
    }

    return (max_free_block >= nonpaged_required_max) ? FlipPassOpenInflateKindNonPaged :
                                                       FlipPassOpenInflateKindPaged;
}

static bool fp_open_stage_xml_output(const uint8_t* data, size_t data_size, void* context) {
    FlipPassOpenXmlStageContext* stage = context;
    FlipPassOpenStreamContext* ctx = NULL;

    furi_assert(stage);
    ctx = stage->stream_ctx;
    furi_assert(ctx);

    if(data == NULL) {
        return data_size == 0U;
    }

    if(!ctx->host_api->append_staged_xml(ctx->host_api->context, data, data_size, ctx->error)) {
        return false;
    }

    stage->plain_size += data_size;
    return true;
}

static bool fp_open_gzip_member_collect(const uint8_t* data, size_t data_size, void* context) {
    FlipPassOpenGzipStageContext* stage = context;
    FlipPassOpenStreamContext* ctx = NULL;

    furi_assert(stage);
    ctx = stage->stream_ctx;
    furi_assert(ctx);

    if(data == NULL) {
        return data_size == 0U;
    }
    if(data_size == 0U) {
        return true;
    }

    if(stage->prefix_len < sizeof(stage->prefix)) {
        const size_t copy = (sizeof(stage->prefix) - stage->prefix_len) < data_size ?
                                (sizeof(stage->prefix) - stage->prefix_len) :
                                data_size;
        memcpy(stage->prefix + stage->prefix_len, data, copy);
        stage->prefix_len += copy;
    }

    if(data_size >= sizeof(stage->trailer)) {
        memcpy(stage->trailer, data + data_size - sizeof(stage->trailer), sizeof(stage->trailer));
        stage->trailer_len = sizeof(stage->trailer);
    } else {
        const size_t keep = (stage->trailer_len + data_size > sizeof(stage->trailer)) ?
                                (sizeof(stage->trailer) - data_size) :
                                stage->trailer_len;
        if(keep > 0U) {
            memmove(stage->trailer, stage->trailer + (stage->trailer_len - keep), keep);
        }
        memcpy(stage->trailer + keep, data, data_size);
        stage->trailer_len = keep + data_size;
    }

    if(!ctx->host_api->append_staged_payload(ctx->host_api->context, data, data_size, ctx->error)) {
        return false;
    }

    stage->total_bytes += data_size;
    return true;
}

static bool fp_open_read_staged_payload_prefix(
    FlipPassOpenStreamContext* ctx,
    uint8_t* prefix,
    size_t prefix_capacity,
    size_t* out_size) {
    size_t total = 0U;
    bool ok = false;

    furi_assert(ctx);
    furi_assert(prefix);
    furi_assert(out_size);

    *out_size = 0U;
    if(!ctx->host_api->begin_staged_payload_stream(ctx->host_api->context, ctx->error)) {
        return false;
    }

    while(total < prefix_capacity) {
        size_t chunk_size = 0U;
        if(!ctx->host_api->read_staged_payload_stream(
               ctx->host_api->context, prefix + total, prefix_capacity - total, &chunk_size)) {
            if(furi_string_empty(ctx->error)) {
                furi_string_set_str(
                    ctx->error, "The staged payload scratch could not be read safely.");
            }
            goto cleanup;
        }
        if(chunk_size == 0U) {
            break;
        }
        total += chunk_size;
    }

    *out_size = total;
    ok = true;

cleanup:
    ctx->host_api->end_staged_payload_stream(ctx->host_api->context);
    return ok;
}

static bool fp_open_stage_plain_payload(
    FlipPassOpenStreamContext* ctx,
    const char* file_path,
    const KDBXOpenProfile* open_profile,
    KDBXVaultBackend preferred_backend,
    char* stream_error,
    size_t stream_error_size) {
    FlipPassOpenXmlStageContext stage = {
        .stream_ctx = ctx,
        .plain_size = 0U,
    };

    furi_assert(ctx);

    if(!ctx->host_api->begin_staged_xml(ctx->host_api->context, preferred_backend, ctx->error)) {
        return false;
    }

    fp_open_progress(ctx, "Decrypting", "", 50U);
    if(!kdbx_open_stream_outer_payload(
           file_path,
           open_profile,
           fp_open_stage_xml_output,
           &stage,
           stream_error,
           stream_error_size)) {
        if(furi_string_empty(ctx->error)) {
            furi_string_set_str(
                ctx->error,
                (stream_error != NULL && stream_error[0] != '\0') ?
                    stream_error :
                    "Unable to stage the decrypted XML payload.");
        }
        ctx->host_api->clear_staged_xml(ctx->host_api->context);
        return false;
    }

    if(!ctx->host_api->finish_staged_xml(ctx->host_api->context, stage.plain_size, ctx->error)) {
        ctx->host_api->clear_staged_xml(ctx->host_api->context);
        return false;
    }

    return true;
}

static bool fp_open_stage_gzip_member(
    FlipPassOpenStreamContext* ctx,
    const char* file_path,
    const KDBXOpenProfile* open_profile,
    KDBXVaultBackend preferred_backend,
    FlipPassOpenStreamResultV2* result,
    char* stream_error,
    size_t stream_error_size) {
    FlipPassOpenGzipStageContext stage = {
        .stream_ctx = ctx,
        .total_bytes = 0U,
        .prefix_len = 0U,
        .trailer_len = 0U,
    };
    KDBXGzipMemberInfo member_info;
    FlipPassOpenGzipParseStatus parse_status = FlipPassOpenGzipParseStatusInvalidHeader;

    furi_assert(ctx);
    furi_assert(result);

    if(!ctx->host_api->begin_staged_payload(
           ctx->host_api->context, preferred_backend, ctx->error)) {
        return false;
    }

    fp_open_log(ctx, "STREAM_GZIP_BRANCH");
    fp_open_progress(ctx, "Decrypting", "", 50U);
    fp_open_log(ctx, "STREAM_GZIP_OUTER_BEGIN");
    if(!kdbx_open_stream_outer_payload(
           file_path,
           open_profile,
           fp_open_gzip_member_collect,
           &stage,
           stream_error,
           stream_error_size)) {
        if(furi_string_empty(ctx->error)) {
            furi_string_set_str(
                ctx->error,
                (stream_error != NULL && stream_error[0] != '\0') ?
                    stream_error :
                    "Unable to stage the decrypted GZip member.");
        }
        ctx->host_api->clear_staged_payload(ctx->host_api->context);
        return false;
    }

    if(!ctx->host_api->finish_staged_payload(
           ctx->host_api->context, stage.total_bytes, ctx->error)) {
        ctx->host_api->clear_staged_payload(ctx->host_api->context);
        return false;
    }

    if(stage.trailer_len < sizeof(stage.trailer)) {
        furi_string_set_str(ctx->error, "The GZip member could not be analyzed safely.");
        ctx->host_api->clear_staged_payload(ctx->host_api->context);
        return false;
    }

    parse_status = fp_open_gzip_parse_member_info(
        stage.prefix, stage.prefix_len, stage.trailer, stage.total_bytes, &member_info);
    if(parse_status != FlipPassOpenGzipParseStatusOk &&
       fp_open_gzip_parse_can_retry_with_expanded_prefix(parse_status) &&
       stage.total_bytes > stage.prefix_len) {
        uint8_t* expanded_prefix = NULL;
        size_t expanded_size = stage.total_bytes - sizeof(stage.trailer);
        if(expanded_size > FLIPPASS_OPEN_GZIP_EXPANDED_PREFIX_BYTES) {
            expanded_size = FLIPPASS_OPEN_GZIP_EXPANDED_PREFIX_BYTES;
        }

        expanded_prefix = malloc(expanded_size);
        if(expanded_prefix != NULL) {
            size_t read_size = 0U;
            if(fp_open_read_staged_payload_prefix(
                   ctx, expanded_prefix, expanded_size, &read_size)) {
                parse_status = fp_open_gzip_parse_member_info(
                    expanded_prefix, read_size, stage.trailer, stage.total_bytes, &member_info);
            }
            memzero(expanded_prefix, expanded_size);
            free(expanded_prefix);
        }
    }

    if(parse_status != FlipPassOpenGzipParseStatusOk) {
        if(furi_string_empty(ctx->error)) {
            furi_string_set_str(ctx->error, fp_open_gzip_parse_error_message(parse_status));
        }
        ctx->host_api->clear_staged_payload(ctx->host_api->context);
        return false;
    }

    result->output_kind = FlipPassOpenStreamOutputKindGzipMember;
    result->staged_payload_size = stage.total_bytes;
    result->gzip_member_info = member_info;
    result->suggested_inflate_kind =
        fp_open_select_inflate_kind(stage.total_bytes, member_info.expected_output_size);

    {
        char log_line[192];
        snprintf(
            log_line,
            sizeof(log_line),
            "STREAM_GZIP_OUTER_OK bytes=%lu inflate=%s free=%lu max=%lu",
            (unsigned long)stage.total_bytes,
            result->suggested_inflate_kind == FlipPassOpenInflateKindNonPaged ? "nonpaged" :
                                                                                "paged",
            (unsigned long)memmgr_get_free_heap(),
            (unsigned long)memmgr_heap_get_max_free_block());
        fp_open_log(ctx, log_line);
    }
    {
        char log_line[192];
        snprintf(
            log_line,
            sizeof(log_line),
            "STREAM_GZIP_MEMBER_INFO_OK out=%lu in=%lu free=%lu max=%lu",
            (unsigned long)member_info.expected_output_size,
            (unsigned long)member_info.compressed_size,
            (unsigned long)memmgr_get_free_heap(),
            (unsigned long)memmgr_heap_get_max_free_block());
        fp_open_log(ctx, log_line);
    }

    return true;
}

static bool fp_open_stream_run(
    const FlipPassOpenStreamRequestV1* request,
    const FlipPassOpenStreamHostApiV1* host_api,
    FlipPassOpenStreamResultV2* result,
    FuriString* error) {
    FlipPassOpenStreamContext ctx = {
        .host_api = host_api,
        .error = error,
        .progress_percent = 0U,
    };
    char stream_error[160] = {0};

    if(request == NULL || host_api == NULL || result == NULL || error == NULL ||
       request->api_version != FLIPPASS_OPEN_STREAM_PLUGIN_API_VERSION ||
       host_api->api_version != FLIPPASS_OPEN_STREAM_HOST_API_VERSION ||
       result->api_version != FLIPPASS_OPEN_STREAM_PLUGIN_API_VERSION ||
       request->file_path == NULL || request->file_path[0] == '\0' ||
       request->open_profile == NULL || host_api->begin_staged_payload == NULL ||
       host_api->append_staged_payload == NULL || host_api->finish_staged_payload == NULL ||
       host_api->clear_staged_payload == NULL || host_api->begin_staged_payload_stream == NULL ||
       host_api->read_staged_payload_stream == NULL ||
       host_api->end_staged_payload_stream == NULL || host_api->begin_staged_xml == NULL ||
       host_api->append_staged_xml == NULL || host_api->finish_staged_xml == NULL ||
       host_api->clear_staged_xml == NULL) {
        furi_string_set_str(error, "Open stream ABI is unavailable or incompatible.");
        return false;
    }

    result->output_kind = FlipPassOpenStreamOutputKindNone;
    result->staged_payload_size = 0U;
    memset(&result->gzip_member_info, 0, sizeof(result->gzip_member_info));
    result->suggested_inflate_kind = FlipPassOpenInflateKindNone;

    if(!kdbx_open_profile_validate_for_stream(
           request->open_profile, stream_error, sizeof(stream_error))) {
        furi_string_set_str(error, stream_error);
        return false;
    }

    fp_open_log(&ctx, "OPEN_STAGE stream");
    if(request->open_profile->compression_algorithm == KDBX_COMPRESSION_GZIP) {
        if(!fp_open_stage_gzip_member(
               &ctx,
               request->file_path,
               request->open_profile,
               request->preferred_backend,
               result,
               stream_error,
               sizeof(stream_error))) {
            return false;
        }
    } else {
        if(!fp_open_stage_plain_payload(
               &ctx,
               request->file_path,
               request->open_profile,
               request->preferred_backend,
               stream_error,
               sizeof(stream_error))) {
            return false;
        }
        result->output_kind = FlipPassOpenStreamOutputKindXml;
    }

    return true;
}

static const FlipPassOpenStreamPluginV1 flippass_open_stream_plugin = {
    .api_version = FLIPPASS_OPEN_STREAM_PLUGIN_API_VERSION,
    .run = fp_open_stream_run,
};

static const FlipperAppPluginDescriptor flippass_open_stream_descriptor = {
    .appid = FLIPPASS_OPEN_STREAM_PLUGIN_APP_ID,
    .ep_api_version = FLIPPASS_OPEN_STREAM_PLUGIN_API_VERSION,
    .entry_point = &flippass_open_stream_plugin,
};

const FlipperAppPluginDescriptor* flippass_open_stream_plugin_ep(void) {
    return &flippass_open_stream_descriptor;
}
