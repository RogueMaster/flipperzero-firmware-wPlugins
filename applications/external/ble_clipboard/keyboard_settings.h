#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t layout;
    uint8_t target;
    bool override_saved;
    uint32_t revision;
} ClipboardKeyboardSettings;

typedef enum {
    ClipboardKeyboardOk,
    ClipboardKeyboardConflict,
    ClipboardKeyboardBusy,
    ClipboardKeyboardInvalid,
    ClipboardKeyboardUnavailable,
    ClipboardKeyboardSaveFailed,
} ClipboardKeyboardResult;
