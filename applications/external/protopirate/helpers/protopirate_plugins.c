#include "protopirate_plugins.h"
#include <loader/firmware_api/firmware_api.h>

#define TAG "PPPlugins"

// -----------------------------------------------------------------------------
// Plugin load / unload
// -----------------------------------------------------------------------------
const FlipperAppPluginDescriptor* load_plugin_fal(
    FlipperApplication** fal_app,
    const char* plugin_path,
    const char* application_id,
    uint32_t api_version) {
    const FlipperAppPluginDescriptor* app_descriptor;

    bool return_value = false;
    do {
        FlipperApplicationPreloadStatus preload_res =
            flipper_application_preload(*fal_app, plugin_path);
        if(preload_res != FlipperApplicationPreloadStatusSuccess) {
            FURI_LOG_E(
                TAG,
                "Failed to preload plugin: %s (%s)",
                plugin_path,
                preload_res == FlipperApplicationPreloadStatusNotEnoughMemory ?
                    "out of memory" :
                    "invalid or stale");
            break;
        }

        if(!flipper_application_is_plugin(*fal_app)) {
            FURI_LOG_E(TAG, "Plugin file is not a library");
            break;
        }

        FlipperApplicationLoadStatus load_status = flipper_application_map_to_memory(*fal_app);
        if(load_status != FlipperApplicationLoadStatusSuccess) {
            FURI_LOG_E(TAG, "Failed to load plugin file");
            break;
        }

        FURI_LOG_D(TAG, "is mapped to memory");
        app_descriptor = flipper_application_plugin_get_descriptor(*fal_app);
        if(strcmp(app_descriptor->appid, application_id) != 0) {
            FURI_LOG_E(TAG, "Application id mismatch %s", application_id);
            break;
        }

        if(app_descriptor->ep_api_version != api_version) {
            FURI_LOG_E(TAG, "API version mismatch %lu", api_version);
            break;
        }

        FURI_LOG_I(
            TAG,
            "Loaded plugin for appid '%s', API %lu",
            app_descriptor->appid,
            app_descriptor->ep_api_version);
        return_value = true;
    } while(false);

    //Free the applciation if there was an error, otherwise return the app descriptor.
    if(return_value) {
        return app_descriptor;
    } else {
        if(*fal_app) {
            flipper_application_free(*fal_app);
            *fal_app = NULL;
        }
        return NULL;
    }
}

void shared_plugin_unload(void** flipper_application_pointer, const void** plugin_pointer) {
    //Free the flipper application.
    if(*flipper_application_pointer) {
        flipper_application_free(*flipper_application_pointer);
        *flipper_application_pointer = NULL;
    }

    //Free the pointer.
    if(*plugin_pointer) {
        *plugin_pointer = NULL;
    }
}

bool shared_plugin_load(
    void** flipper_application_pointer,
    const void** plugin_pointer,
    ProtoPirateSharedPlugin plugin_type,
    const char* txrx_path) {
    const char* application_id = NULL;
    const char* plugin_path = NULL;
    uint32_t api_version = 0;

    if(*plugin_pointer) return true;

    switch(plugin_type) {
    case ProtoPirateSharedPluginsConfig: {
        application_id = PROTOPIRATE_CONFIG_PLUGIN_APP_ID;
        api_version = PROTOPIRATE_CONFIG_PLUGIN_API_VERSION;
        plugin_path = CONFIG_PLUGIN_PATH;
        break;
    }
    case ProtoPirateSharedPluginsSavedInfo: {
        application_id = PROTOPIRATE_SAVED_INFO_PLUGIN_APP_ID;
        api_version = PROTOPIRATE_SAVED_INFO_PLUGIN_API_VERSION;
        plugin_path = SAVED_INFO_PLUGIN_PATH;
        break;
    }
    case ProtoPirateSharedPluginsAbout: {
        application_id = PROTOPIRATE_ABOUT_PLUGIN_APP_ID;
        api_version = PROTOPIRATE_ABOUT_PLUGIN_API_VERSION;
        plugin_path = ABOUT_PLUGIN_PATH;
        break;
    }
#ifdef ENABLE_EMULATE_FEATURE
    case ProtoPirateSharedPluginsEmulate: {
        application_id = PROTOPIRATE_EMULATE_PLUGIN_APP_ID;
        api_version = PROTOPIRATE_EMULATE_PLUGIN_API_VERSION;
        plugin_path = EMULATE_PLUGIN_PATH;
        break;
    }
#endif
    case ProtoPirateSharedPluginsSubDecode: {
        application_id = PROTOPIRATE_TOOL_SCENE_PLUGIN_APP_ID;
        api_version = PROTOPIRATE_TOOL_SCENE_PLUGIN_API_VERSION;
        plugin_path = SUB_DECODE_PLUGIN_PATH;
        break;
    }
#ifdef ENABLE_TIMING_TUNER_SCENE
    case ProtoPirateSharedPluginsTimingTuner: {
        application_id = PROTOPIRATE_TOOL_SCENE_PLUGIN_APP_ID;
        api_version = PROTOPIRATE_TOOL_SCENE_PLUGIN_API_VERSION;
        plugin_path = TIMING_TUNER_PLUGIN_PATH;
        break;
    }
#endif
    case ProtoPirateSharedPluginsPSABruteforce: {
        application_id = PROTOPIRATE_PSA_BF_PLUGIN_APP_ID;
        api_version = PROTOPIRATE_PSA_BF_PLUGIN_API_VERSION;
        plugin_path = PSA_BF_PLUGIN_PATH;
        break;
    }
    case ProtoPirateSharedPluginsTXRX: {
        application_id = PROTOPIRATE_PROTOCOL_PLUGIN_APP_ID;
        api_version = PROTOPIRATE_PROTOCOL_PLUGIN_API_VERSION;
        plugin_path = txrx_path;
        break;
    }
    default:
        return false;
    }

    FURI_LOG_D(TAG, "Loading plugin: %s", plugin_path);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperApplication* fal_app = flipper_application_alloc(storage, firmware_api_interface);
    bool return_value = false;
    do {
        //Load the FAP
        const FlipperAppPluginDescriptor* app_descriptor =
            load_plugin_fal(&fal_app, plugin_path, application_id, api_version);
        if(app_descriptor) {
            //Get the Plugin and assign it.
            if(plugin_type == ProtoPirateSharedPluginsConfig) {
                const ProtoPirateConfigPlugin* plugin_config = app_descriptor->entry_point;
                if(!plugin_config || !plugin_config->on_enter) {
                    FURI_LOG_E(TAG, "Config plugin entry point is invalid");
                } else {
                    *plugin_pointer = plugin_config;
                    return_value = true;
                }
            } else if(plugin_type == ProtoPirateSharedPluginsSavedInfo) {
                const ProtoPirateSavedInfoPlugin* plugin_saved_info = app_descriptor->entry_point;
                if(!plugin_saved_info || !plugin_saved_info->on_enter) {
                    FURI_LOG_E(TAG, "Saved Info plugin entry point is invalid");
                } else {
                    *plugin_pointer = plugin_saved_info;
                    return_value = true;
                }
            } else if(plugin_type == ProtoPirateSharedPluginsAbout) {
                const ProtoPirateAboutPlugin* plugin_about = app_descriptor->entry_point;
                if(!plugin_about || !plugin_about->on_enter) {
                    FURI_LOG_E(TAG, "About plugin entry point is invalid");
                } else {
                    *plugin_pointer = plugin_about;
                    return_value = true;
                }
            }
#ifdef ENABLE_EMULATE_FEATURE
            else if(plugin_type == ProtoPirateSharedPluginsEmulate) {
                const ProtoPirateEmulatePlugin* plugin_emulate = app_descriptor->entry_point;
                if(!plugin_emulate || !plugin_emulate->on_enter) {
                    FURI_LOG_E(TAG, "Emulate plugin entry point is invalid");
                } else {
                    *plugin_pointer = plugin_emulate;
                    return_value = true;
                }
            }
#endif
            else if(
                plugin_type == ProtoPirateSharedPluginsSubDecode
#ifdef ENABLE_TIMING_TUNER_SCENE
                || plugin_type == ProtoPirateSharedPluginsTimingTuner
#endif
            ) {
                const ProtoPirateToolScenePlugin* plugin_tool_scene = app_descriptor->entry_point;
                if(!plugin_tool_scene || !plugin_tool_scene->on_enter ||
                   !plugin_tool_scene->set_host_api) {
                    FURI_LOG_E(TAG, "Tool Scene plugin entry point is invalid");
                } else {
                    *plugin_pointer = plugin_tool_scene;
                    return_value = true;
                }
            } else if(plugin_type == ProtoPirateSharedPluginsPSABruteforce) {
                const ProtoPiratePsaBfPlugin* plugin_psa_bf = app_descriptor->entry_point;
                if(!plugin_psa_bf || !plugin_psa_bf->set_host_api || !plugin_psa_bf->is_running ||
                   !plugin_psa_bf->on_scene_event) {
                    FURI_LOG_E(TAG, "PSA plugin entry point is invalid");
                } else {
                    *plugin_pointer = plugin_psa_bf;
                    return_value = true;
                }
            } else if(plugin_type == ProtoPirateSharedPluginsTXRX) {
                const ProtoPirateProtocolPlugin* plugin_txrx = app_descriptor->entry_point;
                if(!plugin_txrx || !plugin_txrx->registry || plugin_txrx->registry->size == 0U) {
                    FURI_LOG_E(TAG, "Protocol plugin registry entry is invalid");
                } else {
                    *plugin_pointer = plugin_txrx;
                    return_value = true;
                }
            }
        }
    } while(false);

    // A descriptor that loads but fails validation leaves fal_app mapped, so drop it here:
    // "returns false" must mean "nothing to clean up", or every call site has to remember.
    if(!return_value && fal_app) {
        flipper_application_free(fal_app);
        fal_app = NULL;
    }
    *flipper_application_pointer = fal_app;
    furi_record_close(RECORD_STORAGE);
    return return_value;
}
bool shared_plugin_handle_navigation_events(
    SceneManager* scene_manager,
    ViewDispatcher* view_dispatcher,
    SceneManagerEvent event) {
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == ProtoPirateCustomEventPluginNavigateEmulate) {
#ifdef ENABLE_EMULATE_FEATURE
            scene_manager_next_scene(scene_manager, ProtoPirateSceneEmulate);
#endif
            return true;
        }
        if(event.event == ProtoPirateCustomEventPluginNavigateConfig) {
            scene_manager_next_scene(scene_manager, ProtoPirateSceneReceiverConfig);
            return true;
        } else if(event.event == ProtoPirateCustomEventPluginNavigateBack) {
            scene_manager_previous_scene(scene_manager);
            return true;
        } else if(event.event == ProtoPirateCustomEventPluginNavigateStopApp) {
            scene_manager_stop(scene_manager);
            view_dispatcher_stop(view_dispatcher);
            return true;
        }
    }
    return false;
}
