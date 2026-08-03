#pragma once

#include <gui/canvas.h>

#include "../../morse_flipper_mapped_fal.h"
#include "mf_passive_types.h"

#define MF_PASSIVE_API_MAGIC          0x4D46504CUL
#define MF_PASSIVE_SETTINGS_API_MAGIC 0x4D465053UL
#define MF_PASSIVE_API_VERSION        8U

typedef struct {
    MorseFlipperMappedFalApi mapped;
    bool (*enter)(void* state, const MfPassiveEnterArgs* args, MfPassiveResult* initial);
    MfPassiveResult (*input)(void* state, const InputEvent* event, uint32_t now_ms);
} MfPassiveApi;
