#include "../protopirate_app_i.h"
#include "protopirate_psa_bf_host.h"
#include "radio_device_loader.h"

#include <loader/firmware_api/firmware_api.h>
#include <notification/notification_messages.h>

#define TAG "ProtoPirateToolScene"

#define SUB_DECODE_PLUGIN_PATH APP_ASSETS_PATH("plugins/protopirate_sub_decode_plugin.fal")
#ifdef ENABLE_TIMING_TUNER_SCENE
#define TIMING_TUNER_PLUGIN_PATH APP_ASSETS_PATH("plugins/protopirate_timing_tuner_plugin.fal")
#endif

static const char* protopirate_tool_scene_plugin_path(ProtoPirateToolScenePluginKind kind) {
    switch(kind) {
    case ProtoPirateToolScenePluginKindSubDecode:
        return SUB_DECODE_PLUGIN_PATH;
#ifdef ENABLE_TIMING_TUNER_SCENE
    case ProtoPirateToolScenePluginKindTimingTuner:
        return TIMING_TUNER_PLUGIN_PATH;
#endif
    default:
        return NULL;
    }
}

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
};

static void protopirate_tool_scene_plugin_unload(ProtoPirateApp* app) {
    furi_check(app);

    app->tool_scene_plugin = NULL;

    if(app->tool_scene_plugin_manager) {
        plugin_manager_free(app->tool_scene_plugin_manager);
        app->tool_scene_plugin_manager = NULL;
    }

    if(app->tool_scene_plugin_resolver) {
        composite_api_resolver_free(app->tool_scene_plugin_resolver);
        app->tool_scene_plugin_resolver = NULL;
    }
}

static bool protopirate_tool_scene_plugin_ensure_loaded(
    ProtoPirateApp* app,
    ProtoPirateToolScenePluginKind kind) {
    furi_check(app);

    if(app->tool_scene_plugin && app->tool_scene_plugin->kind == kind) {
        return true;
    }

    if(app->tool_scene_plugin) {
        if(app->tool_scene_plugin->release) {
            app->tool_scene_plugin->release(app);
        }
        protopirate_tool_scene_plugin_unload(app);
    }

    const char* plugin_path = protopirate_tool_scene_plugin_path(kind);
    if(!plugin_path) {
        FURI_LOG_E(TAG, "No tool scene plugin path for kind %d", (int)kind);
        return false;
    }

    CompositeApiResolver* resolver = composite_api_resolver_alloc();
    if(!resolver) {
        FURI_LOG_E(TAG, "Failed to allocate tool scene resolver");
        return false;
    }
    composite_api_resolver_add(resolver, firmware_api_interface);

    PluginManager* manager = plugin_manager_alloc(
        PROTOPIRATE_TOOL_SCENE_PLUGIN_APP_ID,
        PROTOPIRATE_TOOL_SCENE_PLUGIN_API_VERSION,
        composite_api_resolver_get(resolver));
    if(!manager) {
        FURI_LOG_E(TAG, "Failed to allocate tool scene plugin manager");
        composite_api_resolver_free(resolver);
        return false;
    }

    PluginManagerError error = plugin_manager_load_single(manager, plugin_path);
    if(error != PluginManagerErrorNone) {
        FURI_LOG_E(TAG, "Failed to load tool scene plugin %s: %d", plugin_path, (int)error);
        plugin_manager_free(manager);
        composite_api_resolver_free(resolver);
        return false;
    }

    const ProtoPirateToolScenePlugin* plugin = plugin_manager_get_ep(manager, 0U);
    if(!plugin || plugin->kind != kind || !plugin->set_host_api || !plugin->on_enter ||
       !plugin->on_event || !plugin->on_exit) {
        FURI_LOG_E(TAG, "Tool scene plugin entry point is invalid for kind %d", (int)kind);
        plugin_manager_free(manager);
        composite_api_resolver_free(resolver);
        return false;
    }

    app->tool_scene_plugin_resolver = resolver;
    app->tool_scene_plugin_manager = manager;
    app->tool_scene_plugin = plugin;
    app->tool_scene_plugin_kind = kind;
    plugin->set_host_api(&protopirate_tool_scene_host_api);
    return true;
}

static void protopirate_tool_scene_apply_pending_nav(ProtoPirateApp* app) {
    furi_check(app);

    const uint8_t nav = app->tool_scene_nav_pending;
    if(nav == TOOL_SCENE_NAV_NONE) {
        return;
    }

    const uint32_t target = app->tool_scene_nav_target;
    app->tool_scene_nav_pending = TOOL_SCENE_NAV_NONE;
    app->tool_scene_nav_target = 0;

    switch(nav) {
    case TOOL_SCENE_NAV_POP:
        scene_manager_previous_scene(app->scene_manager);
        break;
    case TOOL_SCENE_NAV_NEXT:
        scene_manager_next_scene(app->scene_manager, target);
        break;
    case TOOL_SCENE_NAV_SEARCH_PREVIOUS:
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, target);
        break;
    default:
        break;
    }
}

bool protopirate_tool_scene_on_enter(void* context, ProtoPirateToolScenePluginKind kind) {
    ProtoPirateApp* app = context;
    furi_check(app);

    app->tool_scene_nav_pending = TOOL_SCENE_NAV_NONE;
    app->tool_scene_nav_target = 0;

    if(!protopirate_tool_scene_plugin_ensure_loaded(app, kind) || !app->tool_scene_plugin) {
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return false;
    }

    app->tool_scene_plugin->on_enter(app);
    protopirate_tool_scene_apply_pending_nav(app);
    return true;
}

bool protopirate_tool_scene_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = context;
    if(!app || !app->tool_scene_plugin || !app->tool_scene_plugin->on_event) {
        return false;
    }

    const bool consumed = app->tool_scene_plugin->on_event(app, event);
    protopirate_tool_scene_apply_pending_nav(app);
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

    protopirate_tool_scene_plugin_unload(app);
}

void protopirate_tool_scene_plugin_release(ProtoPirateApp* app) {
    if(!app) return;

    if(app->tool_scene_plugin && app->tool_scene_plugin->release) {
        app->tool_scene_plugin->release(app);
    }
    protopirate_tool_scene_plugin_unload(app);
}
