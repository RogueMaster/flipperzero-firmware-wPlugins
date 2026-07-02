#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <gui/scene_manager.h>
#include <lib/flipper_application/flipper_application.h>
#include <lib/subghz/devices/devices.h>

#include "../../defines.h"
#include "../../protopirate_history.h"
#include "../../views/protopirate_receiver.h"

#define PROTOPIRATE_TOOL_SCENE_PLUGIN_APP_ID      "protopirate_tool_scene_plugins"
#define PROTOPIRATE_TOOL_SCENE_PLUGIN_API_VERSION 1U

typedef enum {
    ProtoPirateToolScenePluginKindSubDecode = 0,
#ifdef ENABLE_TIMING_TUNER_SCENE
    ProtoPirateToolScenePluginKindTimingTuner,
#endif
} ProtoPirateToolScenePluginKind;

typedef struct {
    bool (*ensure_receiver_view)(void* app);
    bool (*ensure_widget)(void* app);
    bool (*ensure_view_about)(void* app);
    bool (*radio_init)(void* app);
    void (*rx_stack_resume_after_tx)(void* app);
    void (*preset_init)(
        void* app,
        const char* preset_name,
        uint32_t frequency,
        uint8_t* preset_data,
        size_t preset_data_size);
    bool (*refresh_protocol_registry)(void* app, bool ensure_receiver_ready);
    bool (*apply_protocol_registry_for_context)(
        void* app,
        const char* preset_name,
        uint32_t frequency,
        const uint8_t* preset_data,
        size_t preset_data_size,
        const char* protocol_name);
    void (*begin)(void* app, uint8_t* preset_data);
    uint32_t (*rx)(void* app, uint32_t frequency);
    void (*rx_end)(void* app);
    void (*get_frequency_modulation_str)(
        void* app,
        char* frequency,
        size_t frequency_size,
        char* modulation,
        size_t modulation_size);

    void (*history_release_scratch)(ProtoPirateHistory* history);
    bool (*radio_device_is_external)(const SubGhzDevice* radio_device);

    void (*receiver_add_data_statusbar)(
        ProtoPirateReceiver* receiver,
        const char* frequency_str,
        const char* preset_str,
        const char* history_stat_str,
        bool external_radio);
    uint16_t (*receiver_get_idx_menu)(ProtoPirateReceiver* receiver);
    void (*receiver_set_idx_menu)(ProtoPirateReceiver* receiver, uint16_t idx);
    void (*receiver_set_callback)(
        ProtoPirateReceiver* receiver,
        ProtoPirateReceiverCallback callback,
        void* context);
    void (*receiver_set_sub_decode_mode)(ProtoPirateReceiver* receiver, bool sub_decode_mode);
    void (*receiver_set_sub_decode_progress)(ProtoPirateReceiver* receiver, uint8_t progress);
    void (*receiver_reset_menu)(ProtoPirateReceiver* receiver);
    void (*receiver_sync_menu_from_history)(
        ProtoPirateReceiver* receiver,
        ProtoPirateHistory* history);

    bool (*psa_bf_plugin_ensure_loaded)(void* app);
    void (*psa_bf_context_release)(void* app);
} ProtoPirateToolSceneHostApi;

typedef struct {
    const char* plugin_name;
    ProtoPirateToolScenePluginKind kind;
    void (*set_host_api)(const ProtoPirateToolSceneHostApi* host_api);
    void (*on_enter)(void* app);
    bool (*on_event)(void* app, SceneManagerEvent event);
    void (*on_exit)(void* app);
    void (*release)(void* app);
} ProtoPirateToolScenePlugin;
