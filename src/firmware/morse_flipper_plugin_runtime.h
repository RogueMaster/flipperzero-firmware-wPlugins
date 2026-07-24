#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <flipper_application/plugins/plugin_manager.h>

#include "morse_flipper_mapped_fal.h"

typedef struct MorseFlipperApp MorseFlipperApp;

typedef enum {
    MorseFlipperPluginOwnerNone = 0,
    MorseFlipperPluginOwnerContent,
    MorseFlipperPluginOwnerIcr,
    MorseFlipperPluginOwnerRxPractice,
    MorseFlipperPluginOwnerPassive,
} MorseFlipperPluginOwner;

typedef enum {
    MorseFlipperPluginErrorNone = 0,
    MorseFlipperPluginErrorBusy,
    MorseFlipperPluginErrorLoad,
    MorseFlipperPluginErrorHostId,
    MorseFlipperPluginErrorApiVersion,
    MorseFlipperPluginErrorTable,
    MorseFlipperPluginErrorState,
} MorseFlipperPluginError;

typedef struct {
    FuriMutex* mutex;
    PluginManager* manager;
    const void* api;
    void* state;
    uint8_t owner;
    uint8_t error;
    uint8_t mode;
    uint8_t phase;
    bool playback_active;
    bool playback_mark;
    uint8_t start_hold_mask;
} MorseFlipperPluginSlot;

typedef struct {
    uint8_t owner;
    uint8_t mode;
    uint8_t phase;
    bool active;
    bool playback_active;
    bool playback_mark;
    bool start_holdoff;
} MorseFlipperPluginSnapshot;

/* Own the app-lifetime lock shared by all embedded FAL hosts. */
bool morse_flipper_plugin_runtime_init(MorseFlipperApp* app);
void morse_flipper_plugin_runtime_deinit(MorseFlipperApp* app);
bool morse_flipper_plugin_runtime_claim_locked(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner,
    uint8_t mode);
MorseFlipperPluginError morse_flipper_plugin_runtime_load_locked(
    const char* path,
    uint32_t api_version,
    PluginManager** manager_out,
    const void** entry_out);
bool morse_flipper_plugin_runtime_open_mapped_locked(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner,
    uint8_t mode,
    const char* path,
    uint32_t api_version,
    uint32_t api_magic,
    uint32_t minimum_api_size,
    const void* enter_args,
    MorseFlipperMappedFalResult* initial);
bool morse_flipper_plugin_runtime_publish_locked(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner,
    PluginManager* manager,
    const void* api,
    void* state);
void morse_flipper_plugin_runtime_release_claim_locked(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner);
void morse_flipper_plugin_runtime_detach_locked(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner);
bool morse_flipper_plugin_runtime_tick_locked(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner,
    uint32_t now_ms,
    MorseFlipperMappedFalResult* result);
void morse_flipper_plugin_runtime_apply_result_locked(
    MorseFlipperApp* app,
    MorseFlipperMappedFalResult result,
    uint32_t now_ms);
void morse_flipper_plugin_runtime_draw(MorseFlipperApp* app, Canvas* canvas, uint32_t now_ms);
void morse_flipper_plugin_feedback_locked(
    MorseFlipperApp* app,
    uint8_t feedback,
    uint32_t now_ms);
void morse_flipper_plugin_feedback_expire_locked(
    MorseFlipperApp* app,
    uint32_t now_ms);
bool morse_flipper_plugin_runtime_snapshot(
    const MorseFlipperApp* app,
    MorseFlipperPluginSnapshot* out);
void morse_flipper_plugin_runtime_unload_current(MorseFlipperApp* app);
