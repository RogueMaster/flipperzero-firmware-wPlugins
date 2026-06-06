#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <lib/flipper_application/flipper_application.h>
#include <gui/scene_manager.h>

#define PROTOPIRATE_EMULATE_PLUGIN_APP_ID      "protopirate_emulate_plugin"
#define PROTOPIRATE_EMULATE_PLUGIN_API_VERSION 1U

typedef struct {
    bool (*radio_init)(void* app);
    bool (*apply_protocol_registry_for_preset_data)(
        void* app,
        const uint8_t* preset_data,
        size_t preset_data_size);
    void (*rx_stack_suspend_for_tx)(void* app);
    bool (*ensure_view_about)(void* app);
    void (*idle)(void* app);
    void (*history_release_scratch)(void* app);
    void (*storage_delete_temp)(void);
} ProtoPirateEmulateHostApi;

typedef struct {
    const char* plugin_name;
    void (*set_host_api)(const ProtoPirateEmulateHostApi* host_api);
    void (*on_enter)(void* app);
    bool (*on_event)(void* app, SceneManagerEvent event);
    void (*on_exit)(void* app);
    void (*context_release)(void* app);
} ProtoPirateEmulatePlugin;
