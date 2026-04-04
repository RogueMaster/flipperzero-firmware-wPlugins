#pragma once

#include "kdbx_vault.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    KDBXGzipStatusOk = 0,
    KDBXGzipStatusInvalidArgument,
    KDBXGzipStatusTruncatedInput,
    KDBXGzipStatusInvalidHeader,
    KDBXGzipStatusReservedFlags,
    KDBXGzipStatusInvalidExtraField,
    KDBXGzipStatusInvalidNameField,
    KDBXGzipStatusInvalidCommentField,
    KDBXGzipStatusInvalidHeaderCrcField,
    KDBXGzipStatusInvalidBodyOffset,
    KDBXGzipStatusOutputTooLarge,
    KDBXGzipStatusOutputHeapFragmented,
    KDBXGzipStatusOutputAllocFailed,
    KDBXGzipStatusOutputRejected,
    KDBXGzipStatusWorkspaceAllocFailed,
    KDBXGzipStatusWorkspaceTotalTooSmall,
    KDBXGzipStatusWorkspacePageAllocFailed,
    KDBXGzipStatusWorkspaceStorageFailed,
    KDBXGzipStatusWorkspaceVerifyFailed,
    KDBXGzipStatusInflateFailed,
    KDBXGzipStatusPagedNoProgress,
    KDBXGzipStatusPagedTimeLimit,
    KDBXGzipStatusInputSizeMismatch,
    KDBXGzipStatusOutputSizeMismatch,
    KDBXGzipStatusCrcMismatch,
} KDBXGzipStatus;

typedef enum {
    KDBXGzipInflatePathNone = 0,
    KDBXGzipInflatePathExactOutput,
    KDBXGzipInflatePathContiguousCallback,
    KDBXGzipInflatePathPagedCallback,
    KDBXGzipInflatePathFilePagedCallback,
} KDBXGzipInflatePath;

typedef struct {
    size_t member_size;
    size_t body_offset;
    size_t compressed_size;
    uint32_t expected_crc32;
    uint32_t expected_output_size;
} KDBXGzipMemberInfo;

typedef struct {
    KDBXGzipStatus status;
    size_t expected_output_size;
    size_t free_heap;
    size_t max_free_block;
    int inflate_status;
    size_t consumed_input_size;
    size_t actual_output_size;
    KDBXGzipInflatePath inflate_path;
    size_t workspace_total_size;
    size_t workspace_page_size;
    size_t workspace_cache_pages;
    size_t workspace_timeout_ms;
    size_t workspace_pages_allocated;
    size_t workspace_failed_page_index;
    const char* workspace_storage_stage;
    size_t paged_loop_count;
    size_t paged_flush_count;
    size_t paged_yield_count;
    size_t paged_no_progress_count;
    size_t paged_input_offset;
    size_t paged_last_input_advance;
    size_t paged_last_output_advance;
    size_t paged_last_dict_offset;
    int paged_last_status;
    bool paged_timed_out;
} KDBXGzipTelemetry;

typedef bool (*KDBXGzipOutputCallback)(const uint8_t* data, size_t data_size, void* context);
typedef void (*KDBXGzipTraceCallback)(
    const char* event,
    const KDBXGzipTelemetry* telemetry,
    void* context);

typedef struct {
    KDBXGzipTraceCallback callback;
    void* context;
    size_t interval_bytes;
    void* inflate_workspace;
    bool prefer_file_paged;
} KDBXGzipTraceConfig;

size_t kdbx_gzip_file_paged_workspace_size(void);

bool kdbx_gzip_emit_stream(
    const uint8_t* data,
    size_t data_size,
    size_t max_output_size,
    KDBXGzipOutputCallback callback,
    void* context,
    KDBXGzipTelemetry* telemetry);
bool kdbx_gzip_emit_stream_ex(
    const uint8_t* data,
    size_t data_size,
    size_t max_output_size,
    KDBXGzipOutputCallback callback,
    void* context,
    KDBXGzipTelemetry* telemetry,
    const KDBXGzipTraceConfig* trace_config);
bool kdbx_gzip_parse_member_info(
    const uint8_t* prefix,
    size_t prefix_size,
    const uint8_t trailer[8],
    size_t member_size,
    size_t max_output_size,
    KDBXGzipTelemetry* telemetry,
    KDBXGzipMemberInfo* out_info);
bool kdbx_gzip_emit_vault_stream(
    KDBXVault* vault,
    const KDBXFieldRef* ref,
    const KDBXGzipMemberInfo* member_info,
    size_t max_output_size,
    KDBXGzipOutputCallback callback,
    void* context,
    KDBXGzipTelemetry* telemetry,
    const KDBXGzipTraceConfig* trace_config);
