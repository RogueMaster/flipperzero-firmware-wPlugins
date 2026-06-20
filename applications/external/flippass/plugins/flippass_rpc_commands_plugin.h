#pragma once

#include <furi.h>
#include <flipper_application/flipper_application.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FLIPPASS_RPC_COMMANDS_PLUGIN_APPID       "flippass_rpc_commands"
#define FLIPPASS_RPC_COMMANDS_PLUGIN_API_VERSION 1u
#define FLIPPASS_RPC_COMMANDS_HOST_API_VERSION   1u

typedef enum {
    FlipPassRpcTransportUsb = 0,
    FlipPassRpcTransportBluetooth = 1,
} FlipPassRpcTransport;

typedef enum {
    FlipPassRpcCommandsErrorBadCommand = 1,
    FlipPassRpcCommandsErrorMissingArgument,
    FlipPassRpcCommandsErrorInvalidState,
    FlipPassRpcCommandsErrorInvalidIndex,
    FlipPassRpcCommandsErrorUnsupportedTransport,
    FlipPassRpcCommandsErrorOperationFailed,
} FlipPassRpcCommandsError;

typedef struct {
    char* raw;
    size_t raw_size;
    char* part[4];
    size_t count;
} FlipPassRpcCommandsRequestV1;

typedef struct {
    uint32_t api_version;
    void* context;
    bool (*status)(void* context, FuriString* response);
    bool (*load_file)(void* context, const char* path, FuriString* response);
    bool (*unlock)(void* context, const char* password, const char* backend, FuriString* response);
    bool (*list)(void* context, FuriString* response);
    bool (*cd_parent)(void* context, FuriString* response);
    bool (*cd_index)(void* context, uint32_t index, FuriString* response);
    bool (*entry_index)(void* context, uint32_t index, FuriString* response);
    bool (*show_entry)(void* context, FuriString* response);
    bool (*show_field)(void* context, const char* field, FuriString* response);
    bool (*type_field)(
        void* context,
        const char* field,
        FlipPassRpcTransport transport,
        FuriString* response);
} FlipPassRpcCommandsHostApiV1;

typedef struct {
    uint32_t api_version;
    bool (*execute_bytes)(
        const FlipPassRpcCommandsHostApiV1* host_api,
        const uint8_t* data,
        size_t data_size,
        FuriString* response,
        uint32_t* error_code,
        FuriString* error_text);
    bool (*execute_request)(
        const FlipPassRpcCommandsHostApiV1* host_api,
        const FlipPassRpcCommandsRequestV1* request,
        FuriString* response,
        uint32_t* error_code,
        FuriString* error_text);
} FlipPassRpcCommandsPluginV1;

bool flip_pass_rpc_commands_request_parse(
    const uint8_t* data,
    size_t data_size,
    FlipPassRpcCommandsRequestV1* request);
void flip_pass_rpc_commands_request_free(FlipPassRpcCommandsRequestV1* request);

const FlipperAppPluginDescriptor* flippass_rpc_commands_plugin_ep(void);

#ifdef __cplusplus
}
#endif
