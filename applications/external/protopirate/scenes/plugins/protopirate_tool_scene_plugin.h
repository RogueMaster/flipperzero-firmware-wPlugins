#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <gui/scene_manager.h>
#include <lib/subghz/devices/devices.h>

#include "../../defines.h"
#include "../../protopirate_history.h"
#include "../../views/protopirate_receiver.h"

#define PROTOPIRATE_TOOL_SCENE_PLUGIN_APP_ID      "protopirate_tool_scene_plugins"
// Derived from the layout: this struct is a de-facto ABI, and a hand-bumped number is
// exactly what got missed when a member was once inserted mid-struct.
#define PROTOPIRATE_TOOL_SCENE_PLUGIN_API_VERSION ((uint32_t)sizeof(ProtoPirateToolSceneHostApi))

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
        size_t frequency_size,
        const char* preset_str,
        size_t preset_size,
        const char* history_stat_str,
        size_t history_stat_size,
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

    // Host-resident helpers, so a tool-scene plugin need not compile a second copy of
    // protocol_items.c, protocols_common.c, protopirate_history.c or protopirate_storage.c.
    bool (*catalog_can_tx)(const char* protocol_name);
    bool (*catalog_offers_bruteforce)(const char* protocol_name);
    bool (*catalog_needs_bruteforce)(FlipperFormat* ff);

    const char* (*get_short_preset_name)(const char* preset_name);
    bool (*preset_name_is_custom_marker)(const char* preset_name);

    ProtoPirateHistory* (*history_alloc)(void);
    void (*history_free)(ProtoPirateHistory* history);
    void (*history_reset)(ProtoPirateHistory* history);
    uint16_t (*history_get_item)(ProtoPirateHistory* history);
    bool (*history_add_to_history_at)(
        ProtoPirateHistory* history,
        void* context,
        SubGhzRadioPreset* preset,
        uint32_t update_timestamp);
    FlipperFormat* (*history_get_raw_data)(ProtoPirateHistory* history, uint16_t idx);
    void (*history_get_text_item_detail)(
        ProtoPirateHistory* history,
        uint16_t idx,
        FuriString* output,
        SubGhzEnvironment* environment);

    bool (*storage_save_capture_to_path)(FlipperFormat* flipper_format, const char* full_path);
    bool (*storage_get_next_filename)(
        const char* protocol_name,
        FuriString* out_filename,
        bool dont_add_zero);
    bool (*storage_get_capture_display_protocol)(
        FlipperFormat* flipper_format,
        FuriString* protocol_name);
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
