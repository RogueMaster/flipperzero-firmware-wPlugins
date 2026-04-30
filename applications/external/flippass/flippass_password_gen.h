#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FLIPPASS_PASSWORD_GEN_MAX_LENGTH 255U

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
} FlipPassPasswordGenCharset;

#ifdef __cplusplus
}
#endif
