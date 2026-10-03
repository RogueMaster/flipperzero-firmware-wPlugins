#pragma once
#include <gui/scene_manager.h>

#define PROTOPIRATE_SAVED_INFO_PLUGIN_APP_ID      "pp_saved_info"
#define PROTOPIRATE_SAVED_INFO_PLUGIN_API_VERSION 1U

typedef struct ProtoPirateApp ProtoPirateApp;
typedef struct ProtoPirateSavedInfoSceneHostApi {
    bool (*ensure_widget)(ProtoPirateApp* app);
    bool (*psa_bf_plugin_ensure_loaded)(ProtoPirateApp* app);
    void (*psa_bf_plugin_unload_if_idle)(ProtoPirateApp* app);
    bool (*protocol_catalog_can_tx)(const char* protocol_name);
    bool (*storage_delete_file)(const char* file_path);
    bool (*protocol_catalog_offers_bruteforce)(const char* protocol_name);
} ProtoPirateSavedInfoSceneHostApi;

typedef struct ProtoPirateSavedInfoPlugin {
    const char* plugin_name;
    void (*on_enter)(void* app);
    bool (*on_event)(void* app, SceneManagerEvent event);
    void (*set_host_api)(const ProtoPirateSavedInfoSceneHostApi* host_api);
} ProtoPirateSavedInfoPlugin;
