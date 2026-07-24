#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <flipper_application/plugins/plugin_manager.h>

typedef struct MorseFlipperApp MorseFlipperApp;

typedef enum {
    MorseFlipperPluginOwnerNone = 0,
    MorseFlipperPluginOwnerContent,
    MorseFlipperPluginOwnerIcr,
    MorseFlipperPluginOwnerRxPractice,
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
    bool prompt_visible;
    uint8_t prompt_char;
    uint8_t start_hold_mask;
} MorseFlipperPluginSlot;

typedef struct {
    uint8_t owner;
    uint8_t mode;
    uint8_t phase;
    bool active;
    bool playback_active;
    bool playback_mark;
    bool prompt_visible;
    uint8_t prompt_char;
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
bool morse_flipper_plugin_runtime_publish_locked(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner,
    PluginManager* manager,
    const void* api,
    void* state);
void morse_flipper_plugin_runtime_release_claim_locked(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner);
bool morse_flipper_plugin_runtime_snapshot(
    const MorseFlipperApp* app,
    MorseFlipperPluginSnapshot* out);
void morse_flipper_plugin_runtime_unload_current(MorseFlipperApp* app);
