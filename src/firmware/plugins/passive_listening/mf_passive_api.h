#pragma once

#include <gui/canvas.h>

#include "mf_passive_types.h"

#define MF_PASSIVE_API_MAGIC 0x4D46504CUL
#define MF_PASSIVE_API_VERSION 1U

typedef struct {
    uint32_t magic;
    uint32_t api_version;
    uint32_t struct_size;
    void* (*alloc)(void);
    void (*free)(void* state);
    bool (*enter)(void* state, const MfPassiveEnterArgs* args, MfPassiveResult* initial);
    void (*leave)(void* state);
    MfPassiveResult (*input)(void* state, const InputEvent* event, uint32_t now_ms);
    MfPassiveResult (*tick)(void* state, uint32_t now_ms);
    void (*draw)(const void* state, Canvas* canvas);
} MfPassiveApi;
