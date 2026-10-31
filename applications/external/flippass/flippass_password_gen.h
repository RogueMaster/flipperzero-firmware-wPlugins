#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FLIPPASS_PASSWORD_GEN_MAX_LENGTH     255U
#define FLIPPASS_PASSWORD_GEN_DEFAULT_LENGTH 20U

typedef enum {
    FlipPassPasswordGenTargetNone = 0,
    FlipPassPasswordGenTargetEntryPassword,
    FlipPassPasswordGenTargetProtectedCustomFieldValue,
} FlipPassPasswordGenTarget;

typedef enum {
    FlipPassPasswordGenCharsetFull = 0,
    FlipPassPasswordGenCharsetAlnum,
    FlipPassPasswordGenCharsetAlpha,
    FlipPassPasswordGenCharsetSymbols,
    FlipPassPasswordGenCharsetNumeric,
    FlipPassPasswordGenCharsetHex,
    FlipPassPasswordGenCharsetUtf8,
    FlipPassPasswordGenCharsetOctal,
    FlipPassPasswordGenCharsetBinary,
    FlipPassPasswordGenCharsetLayout,
} FlipPassPasswordGenCharset;

#define FLIPPASS_PASSWORD_GEN_DEFAULT_CHARSET FlipPassPasswordGenCharsetFull
#define FLIPPASS_PASSWORD_GEN_CHARSET_MAX     FlipPassPasswordGenCharsetLayout

#ifdef __cplusplus
}
#endif
