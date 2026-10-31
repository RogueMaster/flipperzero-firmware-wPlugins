#pragma once

#include "../flippass_password_gen.h"

#include <flipper_application/flipper_application.h>
#include <furi.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FLIPPASS_PASSWORD_GEN_PLUGIN_APP_ID      "flippass_password_gen"
#define FLIPPASS_PASSWORD_GEN_PLUGIN_API_VERSION 3u

typedef struct {
    uint32_t api_version;
    FlipPassPasswordGenTarget target;
    FlipPassPasswordGenCharset charset;
    uint16_t length;
    const char* keyboard_layout_path;
} FlipPassPasswordGenPluginRequestV1;

typedef struct {
    char password[FLIPPASS_PASSWORD_GEN_MAX_LENGTH + 1U];
} FlipPassPasswordGenPluginResultV1;

typedef struct {
    uint32_t api_version;
    bool (*begin)(const FlipPassPasswordGenPluginRequestV1* request, FuriString* error);
    bool (*finish)(FlipPassPasswordGenPluginResultV1* result, FuriString* error);
    void (*abort)(void);
} FlipPassPasswordGenPluginV1;

const FlipperAppPluginDescriptor* flippass_password_gen_plugin_ep(void);

#ifdef __cplusplus
}
#endif
