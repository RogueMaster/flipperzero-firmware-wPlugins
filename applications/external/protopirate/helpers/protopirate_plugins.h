#pragma once
#include "../defines.h"
#include "protopirate_types.h"
#include "helpers/protopirate_plugins_host_api.h"
#include "../scenes/plugins/protopirate_config_plugin.h"
#include "../scenes/plugins/protopirate_bruteforce_plugin.h"
#include "protopirate_bruteforce_host.h"
#include "protocols/protopirate_protocol_plugins.h"

#include <lib/flipper_application/flipper_application.h>
#include <gui/scene_manager.h>

#include <gui/view_dispatcher.h>
typedef enum ProtoPirateSharedPluginIDs {
    ProtoPirateSharedPluginsConfig,
    ProtoPirateSharedPluginsSavedInfo,
    ProtoPirateSharedPluginsAbout,
    ProtoPirateSharedPluginsWelcome,
#ifdef ENABLE_EMULATE_FEATURE
    ProtoPirateSharedPluginsEmulate,
#endif
    ProtoPirateSharedPluginsSubDecode,
#ifdef ENABLE_TIMING_TUNER_SCENE
    ProtoPirateSharedPluginsTimingTuner,
#endif
    ProtoPirateSharedPluginsPSABruteforce,
    ProtoPirateSharedPluginsTXRX,
    ProtoPirateSharedPluginsRemoteAnalyzer,
} ProtoPirateSharedPluginIDs;

//Config Plugin Uses its own plugin type, has a header file.
#define PROTOPIRATE_CONFIG_PLUGIN_PATH        "pp_config.fal"
#define PROTOPIRATE_CONFIG_PLUGIN_APP_ID      "pp_config"
#define PROTOPIRATE_CONFIG_PLUGIN_API_VERSION ((uint32_t)sizeof(ProtoPirateSharedPluginHostApi))

//Saved Info Plugin
#define PROTOPIRATE_SAVED_INFO_PLUGIN_PATH   "pp_saved.fal"
#define PROTOPIRATE_SAVED_INFO_PLUGIN_APP_ID "pp_saved"
#define PROTOPIRATE_SAVED_INFO_PLUGIN_API_VERSION \
    ((uint32_t)sizeof(ProtoPirateSharedPluginHostApi))

//About Plugin
#define PROTOPIRATE_ABOUT_PLUGIN_PATH        "pp_about.fal"
#define PROTOPIRATE_ABOUT_PLUGIN_APP_ID      "pp_about"
#define PROTOPIRATE_ABOUT_PLUGIN_API_VERSION ((uint32_t)sizeof(ProtoPirateSharedPluginHostApi))

//Emulate Plugin
#ifdef ENABLE_EMULATE_FEATURE
#define PROTOPIRATE_EMULATE_PLUGIN_PATH        "pp_emulate.fal"
#define PROTOPIRATE_EMULATE_PLUGIN_APP_ID      "pp_emulate"
#define PROTOPIRATE_EMULATE_PLUGIN_API_VERSION ((uint32_t)sizeof(ProtoPirateSharedPluginHostApi))
#endif

//Sub Decoder Plugin.
#define PROTOPIRATE_SUB_DECODE_PLUGIN_PATH   "pp_sd.fal"
#define PROTOPIRATE_SUB_DECODE_PLUGIN_APP_ID "pp_sd"
#define PROTOPIRATE_SUB_DECODE_PLUGIN_API_VERSION \
    ((uint32_t)sizeof(ProtoPirateSharedPluginHostApi))

//Timing Tuner Plugin
#ifdef ENABLE_TIMING_TUNER_SCENE
#define PROTOPIRATE_TIMING_TUNER_PLUGIN_PATH   "pp_tt.fal"
#define PROTOPIRATE_TIMING_TUNER_PLUGIN_APP_ID "pp_tt"
#define PROTOPIRATE_TIMING_TUNER_PLUGIN_API_VERSION \
    ((uint32_t)sizeof(ProtoPirateSharedPluginHostApi))
#endif

//Brute Force Plugin Uses its own plugin type, has a header file.
#define PROTOPIRATE_BRUTEFORCE_PLUGIN_PATH   "pp_bf.fal"
#define PROTOPIRATE_BRUTEFORCE_PLUGIN_APP_ID "pp_bf"
#define PROTOPIRATE_BRUTEFORCE_PLUGIN_API_VERSION \
    ((uint32_t)sizeof(ProtoPirateSharedPluginHostApi))

//Welcome Plugin
#define PROTOPIRATE_WELCOME_PLUGIN_PATH        "pp_welcome.fal"
#define PROTOPIRATE_WELCOME_PLUGIN_APP_ID      "pp_welcome"
#define PROTOPIRATE_WELCOME_PLUGIN_API_VERSION ((uint32_t)sizeof(ProtoPirateSharedPluginHostApi))

//Remote Analyzer Plugin
#define PROTOPIRATE_REMOTE_ANALYZER_PLUGIN_PATH   "pp_ra.fal"
#define PROTOPIRATE_REMOTE_ANALYZER_PLUGIN_APP_ID "pp_ra"
#define PROTOPIRATE_REMOTE_ANALYZER_PLUGIN_API_VERSION \
    ((uint32_t)sizeof(ProtoPirateSharedPluginHostApi))

typedef struct {
    const char* plugin_name;
    void (*set_host_api)(const ProtoPirateSharedPluginHostApi* host_api);
    void (*on_enter)(ProtoPirateApp* app);
    bool (*on_event)(ProtoPirateApp* app, SceneManagerEvent event);
    void (*on_exit)(ProtoPirateApp* app);
    void (*release)(ProtoPirateApp* app);
} ProtoPirateSharedPlugin;

bool shared_plugin_load(
    FlipperApplication** flipper_application_pointer,
    ProtoPiratePlugin* plugin_pointer,
    ProtoPirateSharedPluginIDs plugin_type,
    const char* txrx_path);
void shared_plugin_unload(
    FlipperApplication** flipper_application_pointer,
    ProtoPiratePlugin* plugin_pointer);
bool shared_plugin_handle_navigation_events(
    SceneManager* scene_manager,
    ViewDispatcher* view_dispatcher,
    SceneManagerEvent event);

typedef union ProtoPiratePlugin {
    const ProtoPirateSharedPlugin* shared_plugin;
    const ProtoPirateConfigPlugin* config_plugin;
    const ProtoPirateBruteForcePlugin* bruteforce_plugin;
    const ProtoPirateProtocolPlugin* protocol_plugin;
    const void* plugin_pointer; //DONT USE. ONLY FOR LOADER AND UNLOADER
} ProtoPiratePlugin;
