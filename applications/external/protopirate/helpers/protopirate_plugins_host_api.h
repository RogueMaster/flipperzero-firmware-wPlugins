#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <gui/scene_manager.h>
#include <lib/subghz/devices/devices.h>

#include "../defines.h"
#include "../protopirate_history.h"
#include "../views/protopirate_receiver.h"

#include "protopirate_settings.h"
#include "../protocols/bruteforce_types.h"

typedef struct ProtoPirateApp ProtoPirateApp;
typedef struct Widget Widget;
typedef union ProtoPiratePlugin ProtoPiratePlugin;
typedef enum ProtoPirateSharedPluginIDs ProtoPirateSharedPluginIDs;

typedef struct ProtoPirateSharedPluginHostApi {
    const char* fap_version;
    bool (*ensure_receiver_view)(void* app);
    bool (*ensure_widget)(void* app);
    Widget* (*get_widget)(ProtoPirateApp* app);
    bool (*ensure_view_about)(void* app);
    bool (*ensure_text_input)(ProtoPirateApp* app);
    void (*free_text_input)(ProtoPirateApp* app);
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

    void (*settings_load)(ProtoPirateSettings* settings);
    void (*settings_save)(ProtoPirateSettings* settings);
    bool (*plugin_load)(
        FlipperApplication** flipper_application_pointer,
        ProtoPiratePlugin* plugin_pointer,
        ProtoPirateSharedPluginIDs plugin_type,
        const char* txrx_path);
    void (*plugin_unload)(
        FlipperApplication** flipper_application_pointer,
        ProtoPiratePlugin* plugin_pointer);
    bool (*bruteforce_plugin_ensure_loaded)(ProtoPirateApp* app);
    void (*bruteforce_plugin_unload_if_idle)(ProtoPirateApp* app);
    void (*bruteforce_context_release)(ProtoPirateApp* app);
    bool (*protocol_catalog_can_tx)(const char* protocol_name);
    bool (*storage_delete_file)(const char* file_path);
    bool (*protocol_catalog_offers_bruteforce)(const char* protocol_name);
    void (*rx_stack_suspend_for_tx)(ProtoPirateApp* app);
    void (*idle)(ProtoPirateApp* app);
    void (*storage_delete_temp)(void);
    FlipperFormat* (*get_history_flipper_format)(ProtoPirateApp* app);
    uint16_t (*get_history_index)(ProtoPirateApp* app);
    void (*set_history_index)(ProtoPirateApp* app, uint16_t idx);
    ProtoPirateHistory* (*get_history)(ProtoPirateApp* app);
    void (*history_set_item_str)(ProtoPirateApp* app, uint16_t idx, const char* str);
    void (*patch_flipper_format_on_success)(FlipperFormat* ff, const BruteForceState* state);
    void (*send_custom_event)(ProtoPirateApp* app, uint32_t event);
    void (*notification_error)(ProtoPirateApp* app);
    void (*notification_success)(ProtoPirateApp* app);
    void (*receiver_info_rebuild_widget)(ProtoPirateApp* app);
    void (*saved_info_rebuild_widget)(ProtoPirateApp* app);
    void (*subdecode_signal_info_refresh)(ProtoPirateApp* app);
    void (*scene_previous)(ProtoPirateApp* app);
    const char* (*get_loaded_file_path)(ProtoPirateApp* app);
} ProtoPirateSharedPluginHostApi;

extern const ProtoPirateSharedPluginHostApi protopirate_shared_plugin_host_api;
