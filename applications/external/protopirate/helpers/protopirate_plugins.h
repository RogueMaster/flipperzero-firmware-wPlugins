#pragma once
#include "../defines.h"

#include <lib/flipper_application/flipper_application.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>

#ifdef ENABLE_EMULATE_FEATURE
#include "scenes/plugins/protopirate_emulate_plugin.h"
#endif
#include "scenes/plugins/protopirate_config_plugin.h"
#include "scenes/plugins/protopirate_saved_info_plugin.h"
#include "scenes/plugins/protopirate_about_plugin.h"
#include "scenes/plugins/protopirate_psa_bf_plugin.h"
#include "scenes/plugins/protopirate_tool_scene_plugin.h"

#define CONFIG_PLUGIN_PATH     APP_ASSETS_PATH("plugins/pp_config.fal")
#define SAVED_INFO_PLUGIN_PATH APP_ASSETS_PATH("plugins/pp_saved_info.fal")
#define ABOUT_PLUGIN_PATH      APP_ASSETS_PATH("plugins/pp_about.fal")
#ifdef ENABLE_EMULATE_FEATURE
#define EMULATE_PLUGIN_PATH APP_ASSETS_PATH("plugins/pp_emulate.fal")
#endif
#define SUB_DECODE_PLUGIN_PATH APP_ASSETS_PATH("plugins/pp_sub_decode.fal")
#ifdef ENABLE_TIMING_TUNER_SCENE
#define TIMING_TUNER_PLUGIN_PATH APP_ASSETS_PATH("plugins/pp_timing_tuner.fal")
#endif
#define PSA_BF_PLUGIN_PATH APP_ASSETS_PATH("plugins/pp_bf.fal")

typedef enum ProtoPirateSharedPlugin {
    ProtoPirateSharedPluginsConfig,
    ProtoPirateSharedPluginsSavedInfo,
    ProtoPirateSharedPluginsAbout,
#ifdef ENABLE_EMULATE_FEATURE
    ProtoPirateSharedPluginsEmulate,
#endif
    ProtoPirateSharedPluginsToolScene,
    ProtoPirateSharedPluginsSubDecode,
#ifdef ENABLE_TIMING_TUNER_SCENE
    ProtoPirateSharedPluginsTimingTuner,
#endif
    ProtoPirateSharedPluginsPSABruteforce,
    ProtoPirateSharedPluginsTXRX,
} ProtoPirateSharedPlugin;

bool shared_plugin_load(
    void** flipper_application_pointer,
    const void** plugin_pointer,
    ProtoPirateSharedPlugin plugin_type,
    const char* txrx_path);
void shared_plugin_unload(void** flipper_application_pointer, const void** plugin_pointer);
bool shared_plugin_handle_navigation_events(
    SceneManager* scene_manager,
    ViewDispatcher* view_dispatcher,
    SceneManagerEvent event);
