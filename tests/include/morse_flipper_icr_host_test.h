#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "plugins/icr/morse_flipper_icr_api.h"

#define APP_ASSETS_PATH(path) path
#define FuriWaitForever 0U

typedef struct {
    int unused;
} FuriMutex;

typedef struct {
    FuriMutex* mutex;
    const void* api;
    void* state;
    uint8_t owner;
} MorseFlipperPluginSlot;

typedef struct MorseFlipperApp {
    MorseFlipperPluginSlot plugin_slot;
    uint8_t scene;
    DialogsApp* dialogs;
} MorseFlipperApp;

enum {
    MorseFlipperPluginOwnerIcr = 2,
    MorseFlipperSceneMenuSettings = 5,
};

void furi_mutex_acquire(FuriMutex* mutex, uint32_t timeout);
void furi_mutex_release(FuriMutex* mutex);
bool morse_flipper_plugin_runtime_open_mapped_locked(
    MorseFlipperApp* app,
    uint8_t owner,
    uint8_t mode,
    const char* path,
    uint32_t api_version,
    uint32_t api_magic,
    uint32_t minimum_api_size,
    const void* enter_args,
    MorseFlipperMappedFalResult* initial);
bool morse_flipper_plugin_runtime_tick_locked(
    MorseFlipperApp* app,
    uint8_t owner,
    uint32_t now_ms,
    MorseFlipperMappedFalResult* result);
void morse_flipper_plugin_runtime_apply_result_locked(
    MorseFlipperApp* app,
    MorseFlipperMappedFalResult result,
    uint32_t now_ms);
void morse_flipper_plugin_feedback_expire_locked(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_update_sidetone(MorseFlipperApp* app);
void morse_flipper_view_dirty(MorseFlipperApp* app);
void morse_flipper_scene_back(MorseFlipperApp* app);
