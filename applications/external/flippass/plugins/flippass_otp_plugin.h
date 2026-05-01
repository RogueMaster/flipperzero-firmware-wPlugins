#pragma once

#include "../flippass_otp.h"

#include <flipper_application/flipper_application.h>
#include <furi.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FLIPPASS_OTP_PLUGIN_APP_ID      "flippass_otp"
#define FLIPPASS_OTP_PLUGIN_API_VERSION 1u

typedef struct {
    uint32_t api_version;
    FlipPassOtpKind kind;
    FlipPassOtpSecretEncoding secret_encoding;
    FlipPassOtpAlgorithm algorithm;
    const char* secret;
    uint8_t digits;
    uint32_t period;
    uint64_t counter;
    uint64_t unix_time;
    int32_t time_zone_offset_seconds;
} FlipPassOtpPluginRequestV1;

typedef struct {
    char code[FLIPPASS_OTP_CODE_MAX_CHARS + 1U];
    uint64_t next_counter;
} FlipPassOtpPluginResultV1;

typedef struct {
    uint32_t api_version;
    bool (*generate)(
        const FlipPassOtpPluginRequestV1* request,
        FlipPassOtpPluginResultV1* result,
        FuriString* error);
} FlipPassOtpPluginV1;

const FlipperAppPluginDescriptor* flippass_otp_plugin_ep(void);

#ifdef __cplusplus
}
#endif
