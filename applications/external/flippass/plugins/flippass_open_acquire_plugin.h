#pragma once

#include <furi.h>
#include <flipper_application/flipper_application.h>

#include "../kdbx/kdbx_open_profile.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLIPPASS_OPEN_ACQUIRE_PLUGIN_APP_ID      "flippass_open_acquire"
#define FLIPPASS_OPEN_ACQUIRE_PLUGIN_API_VERSION 1u
#define FLIPPASS_OPEN_ACQUIRE_HOST_API_VERSION   1u

typedef struct {
    uint32_t api_version;
    const char* file_path;
    const char* password;
} FlipPassOpenAcquireRequestV1;

typedef struct {
    uint32_t api_version;
    void* context;
    void (*progress)(void* context, const char* stage, const char* detail, uint8_t percent);
    void (*log)(void* context, const char* message);
} FlipPassOpenAcquireHostApiV1;

typedef struct {
    uint32_t api_version;
    bool (*run)(
        const FlipPassOpenAcquireRequestV1* request,
        const FlipPassOpenAcquireHostApiV1* host_api,
        KDBXOpenProfile* out_profile,
        FuriString* error);
} FlipPassOpenAcquirePluginV1;

const FlipperAppPluginDescriptor* flippass_open_acquire_plugin_ep(void);

#ifdef __cplusplus
}
#endif
