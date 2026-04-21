#pragma once

#include <furi.h>
#include <flipper_application/flipper_application.h>

#include "flippass_open_inflate_plugin.h"

#include "../kdbx/kdbx_gzip.h"
#include "../kdbx/kdbx_open_profile.h"
#include "../kdbx/kdbx_vault.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLIPPASS_OPEN_STREAM_PLUGIN_APP_ID      "flippass_open_stream"
#define FLIPPASS_OPEN_STREAM_PLUGIN_API_VERSION 2u
#define FLIPPASS_OPEN_STREAM_HOST_API_VERSION   2u

typedef struct {
    uint32_t api_version;
    const char* file_path;
    const KDBXOpenProfile* open_profile;
    KDBXVaultBackend preferred_backend;
} FlipPassOpenStreamRequestV1;

typedef enum {
    FlipPassOpenStreamOutputKindNone = 0,
    FlipPassOpenStreamOutputKindXml,
    FlipPassOpenStreamOutputKindGzipMember,
} FlipPassOpenStreamOutputKind;

typedef struct {
    uint32_t api_version;
    FlipPassOpenStreamOutputKind output_kind;
    size_t staged_payload_size;
    KDBXGzipMemberInfo gzip_member_info;
    FlipPassOpenInflateKind suggested_inflate_kind;
} FlipPassOpenStreamResultV2;

typedef struct {
    uint32_t api_version;
    void* context;
    void (*progress)(void* context, const char* stage, const char* detail, uint8_t percent);
    void (*log)(void* context, const char* message);
    bool (*begin_staged_payload)(
        void* context,
        KDBXVaultBackend preferred_backend,
        FuriString* error);
    bool (*append_staged_payload)(
        void* context,
        const uint8_t* data,
        size_t data_size,
        FuriString* error);
    bool (*finish_staged_payload)(void* context, size_t payload_size, FuriString* error);
    void (*clear_staged_payload)(void* context);
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
} FlipPassOpenStreamHostApiV1;

typedef struct {
    uint32_t api_version;
    bool (*run)(
        const FlipPassOpenStreamRequestV1* request,
        const FlipPassOpenStreamHostApiV1* host_api,
        FlipPassOpenStreamResultV2* result,
        FuriString* error);
} FlipPassOpenStreamPluginV1;

const FlipperAppPluginDescriptor* flippass_open_stream_plugin_ep(void);

#ifdef __cplusplus
}
#endif
