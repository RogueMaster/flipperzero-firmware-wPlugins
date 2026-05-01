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
#define FLIPPASS_PASSWORD_GEN_PLUGIN_API_VERSION 1u

typedef struct {
    uint32_t api_version;
    FlipPassPasswordGenTarget target;
    FlipPassPasswordGenCharset charset;
    uint16_t length;
    uint16_t harvest_seconds;
} FlipPassPasswordGenPluginRequestV1;

typedef struct {
    uint32_t tick;
    uint32_t sequence;
    uint8_t key;
    uint8_t type;
} FlipPassPasswordGenPluginInputRecordV1;

typedef struct {
    uint32_t input_events;
    uint32_t subghz_samples;
    uint32_t subghz_edges;
    bool subghz_active;
} FlipPassPasswordGenPluginStatusV1;

typedef struct {
    char password[FLIPPASS_PASSWORD_GEN_MAX_LENGTH + 1U];
    FlipPassPasswordGenPluginStatusV1 status;
} FlipPassPasswordGenPluginResultV1;

typedef struct {
    uint32_t api_version;
    bool (*begin)(const FlipPassPasswordGenPluginRequestV1* request, FuriString* error);
    bool (*record_input)(const FlipPassPasswordGenPluginInputRecordV1* record);
    bool (*poll)(uint32_t now_tick, FlipPassPasswordGenPluginStatusV1* status);
    bool (*finish)(FlipPassPasswordGenPluginResultV1* result, FuriString* error);
    void (*abort)(void);
} FlipPassPasswordGenPluginV1;

const FlipperAppPluginDescriptor* flippass_password_gen_plugin_ep(void);

#ifdef __cplusplus
}
#endif
