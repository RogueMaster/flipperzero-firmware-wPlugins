#pragma once

#include <furi.h>
#include <flipper_application/flipper_application.h>

#include "../kdbx/kdbx_data.h"
#include "../kdbx/kdbx_vault.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLIPPASS_OPEN_MODEL_PLUGIN_APP_ID       "flippass_open_model"
#define FLIPPASS_OPEN_MODEL_PLUGIN_API_VERSION  1u
#define FLIPPASS_OPEN_MODEL_HOST_API_VERSION    1u
#define FLIPPASS_OPEN_MODEL_BUILDER_API_VERSION 3u

typedef bool (*FlipPassOpenChunkCallback)(const uint8_t* data, size_t data_size, void* context);

typedef struct {
    uint32_t api_version;
    KDBXVaultBackend requested_backend;
    bool allow_ext_promotion;
    size_t staged_payload_plain_size;
} FlipPassOpenModelRequestV1;

typedef struct {
    uint32_t api_version;
    void* context;
    void (*progress)(void* context, const char* stage, const char* detail, uint8_t percent);
    void (*log)(void* context, const char* message);
    bool (*stream_staged_xml)(
        void* context,
        FlipPassOpenChunkCallback callback,
        void* callback_context,
        FuriString* error);
} FlipPassOpenModelHostApiV1;

typedef struct {
    uint32_t api_version;
    void* context;
    bool (*begin_session)(
        void* context,
        KDBXVaultBackend backend,
        bool allow_ext_promotion,
        FuriString* error);
    void (*cancel_session)(void* context);
    bool (*begin_group)(void* context, FuriString* error);
    bool (*end_group)(void* context, FuriString* error);
    bool (*begin_entry)(void* context, FuriString* error);
    bool (*end_entry)(void* context, FuriString* error);
    bool (*set_group_name)(void* context, const char* value, size_t value_len, FuriString* error);
    bool (*set_entry_title)(void* context, const char* value, size_t value_len, FuriString* error);
    bool (*set_entry_uuid)(void* context, const char* value, size_t value_len, FuriString* error);
    bool (*set_entry_standard_field)(
        void* context,
        uint32_t field_mask,
        const char* value,
        size_t value_len,
        FuriString* error);
    bool (*add_custom_field)(
        void* context,
        const char* key,
        const char* value,
        size_t value_len,
        FuriString* error);
    bool (*should_stream_string_value)(void* context, const char* key);
    bool (*prepare_string_value_stream)(
        void* context,
        const char* key,
        size_t buffered_size,
        FuriString* error);
    bool (*begin_streamed_value)(void* context, const char* key, FuriString* error);
    bool (*write_streamed_value_chunk)(
        void* context,
        const char* key,
        const uint8_t* data,
        size_t data_size,
        FuriString* error);
    bool (*commit_streamed_value)(void* context, const char* key, FuriString* error);
    void (*abort_streamed_value)(void* context);
    bool (
        *finish_session)(void* context, size_t group_count, size_t entry_count, FuriString* error);
} FlipPassOpenBuilderApiV1;

typedef struct {
    uint32_t api_version;
    bool (*run)(
        const FlipPassOpenModelRequestV1* request,
        const FlipPassOpenModelHostApiV1* host_api,
        const FlipPassOpenBuilderApiV1* builder_api,
        FuriString* error);
} FlipPassOpenModelPluginV1;

const FlipperAppPluginDescriptor* flippass_open_model_plugin_ep(void);

#ifdef __cplusplus
}
#endif
