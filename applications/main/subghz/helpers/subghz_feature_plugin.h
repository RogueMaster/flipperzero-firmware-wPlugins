#pragma once

#include <flipper_application/plugins/plugin_manager.h>

#include "../subghz.h"

// Bump when the private app structs, scene events or exported signatures used by these plugins change.
#define SUBGHZ_FEATURE_PLUGIN_API_VERSION 421

// Feature scenes are mutually exclusive and share one resolver.
PluginManager* subghz_feature_plugin_load(SubGhz* subghz, const char* app_id, const char* path);
void subghz_feature_plugin_unload(SubGhz* subghz, PluginManager* manager);
