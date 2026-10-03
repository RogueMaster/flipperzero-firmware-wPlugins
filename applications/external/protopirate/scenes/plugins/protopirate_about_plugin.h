#pragma once
#include <gui/scene_manager.h>
#include "../../helpers/protopirate_settings.h"

#define PROTOPIRATE_ABOUT_PLUGIN_APP_ID      "pp_about"
#define PROTOPIRATE_ABOUT_PLUGIN_API_VERSION 1U

typedef struct ProtoPirateApp ProtoPirateApp;

typedef struct ProtoPirateAboutSceneHostApi {
    bool (*ensure_view_about)(ProtoPirateApp* app);
    void (*settings_load)(ProtoPirateSettings* settings);
    void (*settings_save)(ProtoPirateSettings* settings);
    const char* fap_version;
} ProtoPirateAboutSceneHostApi;

typedef struct ProtoPirateAboutPlugin {
    const char* plugin_name;
    void (*on_enter)(void* app);
    void (*on_exit)(void* app);
    bool (*on_event)(void* app, SceneManagerEvent event);
    void (*set_host_api)(const ProtoPirateAboutSceneHostApi* host_api);
} ProtoPirateAboutPlugin;
