#include "protopirate_plugins.h"
#include <loader/firmware_api/firmware_api.h>
#include "scenes/protopirate_scene.h"

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

inline void shared_plugin_unload(
    FlipperApplication** flipper_application_pointer,
    ProtoPiratePlugin* plugin_pointer) {
    //Free the flipper application.
    if(*flipper_application_pointer) {
        flipper_application_free(*flipper_application_pointer);
        *flipper_application_pointer = NULL;
    }

    //Free the pointer.
    if(plugin_pointer->plugin_pointer) {
        //memset(plugin_pointer, 0, sizeof(*plugin_pointer));
        plugin_pointer->plugin_pointer = NULL;
    }
}

bool shared_plugin_load(
    FlipperApplication** flipper_application_pointer,
    ProtoPiratePlugin* plugin_pointer,
    ProtoPirateSharedPluginIDs plugin_type,
    const char* txrx_path) {
    const char* application_id = NULL;
    const char* plugin_path = NULL;
    uint32_t api_version = 0;

    if((*plugin_pointer).plugin_pointer) return true;

    switch(plugin_type) {
    case ProtoPirateSharedPluginsConfig: {
        application_id = PROTOPIRATE_CONFIG_PLUGIN_APP_ID;
        api_version = PROTOPIRATE_CONFIG_PLUGIN_API_VERSION;
        plugin_path = PROTOPIRATE_CONFIG_PLUGIN_PATH;
        break;
    }
    case ProtoPirateSharedPluginsSavedInfo: {
        application_id = PROTOPIRATE_SAVED_INFO_PLUGIN_APP_ID;
        api_version = PROTOPIRATE_SAVED_INFO_PLUGIN_API_VERSION;
        plugin_path = PROTOPIRATE_SAVED_INFO_PLUGIN_PATH;
        break;
    }
    case ProtoPirateSharedPluginsAbout: {
        application_id = PROTOPIRATE_ABOUT_PLUGIN_APP_ID;
        api_version = PROTOPIRATE_ABOUT_PLUGIN_API_VERSION;
        plugin_path = PROTOPIRATE_ABOUT_PLUGIN_PATH;
        break;
    }
#ifdef ENABLE_EMULATE_FEATURE
    case ProtoPirateSharedPluginsEmulate: {
        application_id = PROTOPIRATE_EMULATE_PLUGIN_APP_ID;
        api_version = PROTOPIRATE_EMULATE_PLUGIN_API_VERSION;
        plugin_path = PROTOPIRATE_EMULATE_PLUGIN_PATH;
        break;
    }
#endif
    case ProtoPirateSharedPluginsSubDecode: {
        application_id = PROTOPIRATE_SUB_DECODE_PLUGIN_APP_ID;
        api_version = PROTOPIRATE_SUB_DECODE_PLUGIN_API_VERSION;
        plugin_path = PROTOPIRATE_SUB_DECODE_PLUGIN_PATH;
        break;
    }
#ifdef ENABLE_TIMING_TUNER_SCENE
    case ProtoPirateSharedPluginsTimingTuner: {
        application_id = PROTOPIRATE_TIMING_TUNER_PLUGIN_APP_ID;
        api_version = PROTOPIRATE_TIMING_TUNER_PLUGIN_API_VERSION;
        plugin_path = PROTOPIRATE_TIMING_TUNER_PLUGIN_PATH;
        break;
    }
#endif
    case ProtoPirateSharedPluginsPSABruteforce: {
        application_id = PROTOPIRATE_BRUTEFORCE_PLUGIN_APP_ID;
        api_version = PROTOPIRATE_BRUTEFORCE_PLUGIN_API_VERSION;
        plugin_path = PROTOPIRATE_BRUTEFORCE_PLUGIN_PATH;
        break;
    }
    case ProtoPirateSharedPluginsTXRX: {
        application_id = PROTOPIRATE_PROTOCOL_PLUGIN_APP_ID;
        api_version = PROTOPIRATE_PROTOCOL_PLUGIN_API_VERSION;
        plugin_path = txrx_path;
        break;
    }
#ifdef ENABLE_WELCOME_SCREEN
    case ProtoPirateSharedPluginsWelcome: {
        application_id = PROTOPIRATE_WELCOME_PLUGIN_APP_ID;
        api_version = PROTOPIRATE_WELCOME_PLUGIN_API_VERSION;
        plugin_path = PROTOPIRATE_WELCOME_PLUGIN_PATH;
        break;
    }
#endif
#ifdef ENABLE_REMOTE_ANALYZER
    case ProtoPirateSharedPluginsRemoteAnalyzer: {
        application_id = PROTOPIRATE_REMOTE_ANALYZER_PLUGIN_APP_ID;
        api_version = PROTOPIRATE_REMOTE_ANALYZER_PLUGIN_API_VERSION;
        plugin_path = PROTOPIRATE_REMOTE_ANALYZER_PLUGIN_PATH;
        break;
    }
#endif
    default:
        return false;
    }

    FURI_LOG_D(TAG, "Loading plugin: %s", plugin_path);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperApplication* fal_app = flipper_application_alloc(storage, firmware_api_interface);
    bool return_value;

    //Load the FAP
#define FILE_NAME_BUFFER_SIZE 128
    char buffer[FILE_NAME_BUFFER_SIZE] = {0};
    snprintf(buffer, FILE_NAME_BUFFER_SIZE, "%s%s", APP_ASSETS_PATH("plugins/"), plugin_path);
    const FlipperAppPluginDescriptor* app_descriptor =
        load_plugin_fal(&fal_app, buffer, application_id, api_version);
    if(app_descriptor) {
        (*plugin_pointer).plugin_pointer = app_descriptor->entry_point;
        return_value = true;
    } else {
        return_value = false;
        if(fal_app) {
            flipper_application_free(fal_app);
            fal_app = NULL;
        }
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
        uint32_t scene_id = 0;
        if(event.event == ProtoPirateCustomEventPluginNavigateReceiver) {
            scene_id = ProtoPirateSceneReceiver;
        }
        if(event.event == ProtoPirateCustomEventPluginNavigateSwitchToReceiver) {
            scene_manager_previous_scene(scene_manager);
            scene_id = ProtoPirateSceneReceiver;
        }
        if(event.event == ProtoPirateCustomEventPluginNavigateEmulate) {
#ifdef ENABLE_EMULATE_FEATURE
            scene_id = ProtoPirateSceneEmulate;
#endif
        }
        if(event.event == ProtoPirateCustomEventPluginNavigateConfig) {
            scene_id = ProtoPirateSceneReceiverConfig;
        } else if(event.event == ProtoPirateCustomEventPluginNavigateBack) {
            scene_manager_previous_scene(scene_manager);
            return true;
        } else if(event.event == ProtoPirateCustomEventPluginNavigateStopApp) {
            scene_manager_stop(scene_manager);
            view_dispatcher_stop(view_dispatcher);
            return true;
        }

        //If we have a scene switch to it.
        if(scene_id) {
            scene_manager_next_scene(scene_manager, scene_id);
            return true;
        }
    }
    return false;
}
