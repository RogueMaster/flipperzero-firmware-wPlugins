#pragma once
#include <furi.h>
typedef enum {
    InputKeyUp,
    InputKeyDown,
    InputKeyRight,
    InputKeyLeft,
    InputKeyOk,
    InputKeyBack,
    InputKeyMAX
} InputKey;
typedef enum {
    InputTypePress,
    InputTypeRelease,
    InputTypeShort,
    InputTypeLong,
    InputTypeRepeat,
    InputTypeMAX
} InputType;
typedef struct {
    uint32_t sequence;
    InputKey key;
    InputType type;
} InputEvent;
