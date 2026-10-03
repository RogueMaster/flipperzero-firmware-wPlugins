#include "../protopirate_app_i.h"
#include "protopirate_psa_bf_host.h"
#include "radio_device_loader.h"
#include "../protocols/protocol_bf_probe.h"
#include "../protocols/protocol_items.h"
#include "../protocols/protocols_common.h"
#include "protopirate_storage.h"

#include <notification/notification_messages.h>

#define TAG "PPToolScene"

static bool host_ensure_receiver_view(void* app) {
    return protopirate_ensure_receiver_view((ProtoPirateApp*)app);
}

static bool host_ensure_widget(void* app) {
    return protopirate_ensure_widget((ProtoPirateApp*)app);
}

static bool host_ensure_view_about(void* app) {
    return protopirate_ensure_view_about((ProtoPirateApp*)app);
}

static bool host_radio_init(void* app) {
    return protopirate_radio_init((ProtoPirateApp*)app);
}

static void host_rx_stack_resume_after_tx(void* app) {
    protopirate_rx_stack_resume_after_tx((ProtoPirateApp*)app);
}

static void host_preset_init(
    void* app,
    const char* preset_name,
    uint32_t frequency,
    uint8_t* preset_data,
    size_t preset_data_size) {
    protopirate_preset_init(app, preset_name, frequency, preset_data, preset_data_size);
}

static bool host_refresh_protocol_registry(void* app, bool ensure_receiver_ready) {
    return protopirate_refresh_protocol_registry((ProtoPirateApp*)app, ensure_receiver_ready);
}

static bool host_apply_protocol_registry_for_context(
    void* app,
    const char* preset_name,
    uint32_t frequency,
    const uint8_t* preset_data,
    size_t preset_data_size,
    const char* protocol_name) {
    return protopirate_apply_protocol_registry_for_context(
        (ProtoPirateApp*)app, preset_name, frequency, preset_data, preset_data_size, protocol_name);
}

static void host_begin(void* app, uint8_t* preset_data) {
    protopirate_begin((ProtoPirateApp*)app, preset_data);
}

static uint32_t host_rx(void* app, uint32_t frequency) {
    return protopirate_rx((ProtoPirateApp*)app, frequency);
}

static void host_rx_end(void* app) {
    protopirate_rx_end((ProtoPirateApp*)app);
}

static void host_get_frequency_modulation_str(
    void* app,
    char* frequency,
    size_t frequency_size,
    char* modulation,
    size_t modulation_size) {
    protopirate_get_frequency_modulation_str(
        (ProtoPirateApp*)app, frequency, frequency_size, modulation, modulation_size);
}

static bool host_psa_bf_plugin_ensure_loaded(void* app) {
    return protopirate_psa_bf_plugin_ensure_loaded((ProtoPirateApp*)app);
}

static void host_psa_bf_context_release(void* app) {
    protopirate_psa_bf_context_release((ProtoPirateApp*)app);
}

static const ProtoPirateToolSceneHostApi protopirate_tool_scene_host_api = {
    .ensure_receiver_view = host_ensure_receiver_view,
    .ensure_widget = host_ensure_widget,
    .ensure_view_about = host_ensure_view_about,
    .radio_init = host_radio_init,
    .rx_stack_resume_after_tx = host_rx_stack_resume_after_tx,
    .preset_init = host_preset_init,
    .refresh_protocol_registry = host_refresh_protocol_registry,
    .apply_protocol_registry_for_context = host_apply_protocol_registry_for_context,
    .begin = host_begin,
    .rx = host_rx,
    .rx_end = host_rx_end,
    .get_frequency_modulation_str = host_get_frequency_modulation_str,
    .history_release_scratch = protopirate_history_release_scratch,
    .radio_device_is_external = radio_device_loader_is_external,
    .receiver_add_data_statusbar = protopirate_view_receiver_add_data_statusbar,
    .receiver_get_idx_menu = protopirate_view_receiver_get_idx_menu,
    .receiver_set_idx_menu = protopirate_view_receiver_set_idx_menu,
    .receiver_set_callback = protopirate_view_receiver_set_callback,
    .receiver_set_sub_decode_mode = protopirate_view_receiver_set_sub_decode_mode,
    .receiver_set_sub_decode_progress = protopirate_view_receiver_set_sub_decode_progress,
    .receiver_reset_menu = protopirate_view_receiver_reset_menu,
    .receiver_sync_menu_from_history = protopirate_view_receiver_sync_menu_from_history,
    .psa_bf_plugin_ensure_loaded = host_psa_bf_plugin_ensure_loaded,
    .psa_bf_context_release = host_psa_bf_context_release,

    .catalog_can_tx = protopirate_protocol_catalog_can_tx,
    .catalog_offers_bruteforce = protopirate_protocol_catalog_offers_bruteforce,
    .catalog_needs_bruteforce = protopirate_bf_probe_needs_bruteforce,

    .get_short_preset_name = pp_get_short_preset_name,
    .preset_name_is_custom_marker = pp_preset_name_is_custom_marker,

    .history_alloc = protopirate_history_alloc,
    .history_free = protopirate_history_free,
    .history_reset = protopirate_history_reset,
    .history_get_item = protopirate_history_get_item,
    .history_add_to_history_at = protopirate_history_add_to_history_at,
    .history_get_raw_data = protopirate_history_get_raw_data,
    .history_get_text_item_detail = protopirate_history_get_text_item_detail,

    .storage_save_capture_to_path = protopirate_storage_save_capture_to_path,
    .storage_get_next_filename = protopirate_storage_get_next_filename,
    .storage_get_capture_display_protocol = protopirate_storage_get_capture_display_protocol,
};

static bool protopirate_tool_scene_plugin_ensure_loaded(
    ProtoPirateApp* app,
    ProtoPirateToolScenePluginKind kind) {
    furi_check(app);

    if(app->tool_scene_plugin && app->tool_scene_plugin->kind == kind) {
        return true;
    }

    // Resolve the kind before unloading: an unsupported one must not cost us the plugin
    // that is already resident.
    ProtoPirateSharedPlugin plugin_type;
    switch(kind) {
    case ProtoPirateToolScenePluginKindSubDecode:
        plugin_type = ProtoPirateSharedPluginsSubDecode;
        break;
#ifdef ENABLE_TIMING_TUNER_SCENE
    case ProtoPirateToolScenePluginKindTimingTuner:
        plugin_type = ProtoPirateSharedPluginsTimingTuner;
        break;
#endif
    default:
        FURI_LOG_E(TAG, "Unknown tool scene kind %d", kind);
        return false;
    }

    if(app->tool_scene_plugin) {
        if(app->tool_scene_plugin->release) {
            app->tool_scene_plugin->release(app);
        }

        shared_plugin_unload(
            (void**)&app->tool_scene_plugin_flipper_application,
            (const void**)&app->tool_scene_plugin);
    }

    // A missing, stale or too-large-for-the-heap .fal leaves tool_scene_plugin NULL, which
    // set_host_api below would fault on.
    if(!shared_plugin_load(
           (void**)&app->tool_scene_plugin_flipper_application,
           (const void**)&app->tool_scene_plugin,
           plugin_type,
           NULL)) {
        FURI_LOG_E(TAG, "Failed to load tool scene plugin for kind %d", kind);
        return false;
    }

    app->tool_scene_plugin->set_host_api(&protopirate_tool_scene_host_api);
    return true;
}

bool protopirate_tool_scene_on_enter(void* context, ProtoPirateToolScenePluginKind kind) {
    ProtoPirateApp* app = context;
    furi_check(app);

    if(!protopirate_tool_scene_plugin_ensure_loaded(app, kind)) {
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return false;
    }

    app->tool_scene_plugin->on_enter(app);
    return true;
}

bool protopirate_tool_scene_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = context;
    if(!app || !app->tool_scene_plugin || !app->tool_scene_plugin->on_event) {
        return false;
    }

    const bool consumed = app->tool_scene_plugin->on_event(app, event);
    return consumed;
}

void protopirate_tool_scene_on_exit(void* context) {
    ProtoPirateApp* app = context;
    if(!app) return;

    if(app->tool_scene_plugin) {
        if(app->tool_scene_plugin->on_exit) {
            app->tool_scene_plugin->on_exit(app);
        }
        if(app->tool_scene_plugin->release) {
            app->tool_scene_plugin->release(app);
        }
    }

    shared_plugin_unload(
        (void**)&app->tool_scene_plugin_flipper_application,
        (const void**)&app->tool_scene_plugin);
}

void protopirate_tool_scene_plugin_release(ProtoPirateApp* app) {
    if(!app) return;

    if(app->tool_scene_plugin && app->tool_scene_plugin->release) {
        app->tool_scene_plugin->release(app);
    }
    shared_plugin_unload(
        (void**)&app->tool_scene_plugin_flipper_application,
        (const void**)&app->tool_scene_plugin);
}
