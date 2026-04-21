#pragma once

#include <furi.h>
#include <flipper_application/flipper_application.h>

#include "../kdbx/kdbx_gzip.h"
#include "../kdbx/kdbx_vault.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLIPPASS_OPEN_INFLATE_NONPAGED_PLUGIN_APP_ID "flippass_open_inflate_nonpaged"
#define FLIPPASS_OPEN_INFLATE_PAGED_PLUGIN_APP_ID    "flippass_open_inflate_paged"
#define FLIPPASS_OPEN_INFLATE_PLUGIN_API_VERSION     1u
#define FLIPPASS_OPEN_INFLATE_HOST_API_VERSION       1u

typedef enum {
    FlipPassOpenInflateKindNone = 0,
    FlipPassOpenInflateKindNonPaged,
    FlipPassOpenInflateKindPaged,
} FlipPassOpenInflateKind;

typedef struct {
    uint32_t api_version;
    KDBXVaultBackend preferred_backend;
    KDBXGzipMemberInfo member_info;
} FlipPassOpenInflateRequestV1;

typedef struct {
    uint32_t api_version;
    bool retry_with_paged;
} FlipPassOpenInflateResultV1;

typedef struct {
    uint32_t api_version;
    void* context;
    void (*progress)(void* context, const char* stage, const char* detail, uint8_t percent);
    void (*log)(void* context, const char* message);
    bool (*begin_staged_payload_stream)(void* context, FuriString* error);
    bool (*read_staged_payload_stream)(
        void* context,
        uint8_t* out,
        size_t capacity,
        size_t* out_size);
    void (*end_staged_payload_stream)(void* context);
    bool (*begin_staged_xml)(void* context, KDBXVaultBackend preferred_backend, FuriString* error);
    bool (*append_staged_xml)(
        void* context,
        const uint8_t* data,
        size_t data_size,
        FuriString* error);
    bool (*finish_staged_xml)(void* context, size_t plain_size, FuriString* error);
    void (*clear_staged_xml)(void* context);
} FlipPassOpenInflateHostApiV1;

typedef struct {
    uint32_t api_version;
    bool (*run)(
        const FlipPassOpenInflateRequestV1* request,
        const FlipPassOpenInflateHostApiV1* host_api,
        FlipPassOpenInflateResultV1* result,
        FuriString* error);
} FlipPassOpenInflatePluginV1;

const FlipperAppPluginDescriptor* flippass_open_inflate_nonpaged_plugin_ep(void);
const FlipperAppPluginDescriptor* flippass_open_inflate_paged_plugin_ep(void);

#ifdef __cplusplus
}
#endif
