#pragma once
#include <furi.h>
#include <flipper_application/flipper_application.h>
#include <gui/scene_manager.h>

#define FLIPPASS_FILE_BROWSER_PLUGIN_APP_ID      "flippass_file_browser"
#define FLIPPASS_FILE_BROWSER_PLUGIN_API_VERSION 1u
struct App;
typedef struct {
    uint32_t api_version;
    void (*on_enter)(void* context);
    bool (*on_event)(void* context, SceneManagerEvent event);
    void (*on_exit)(void* context);
    bool (*check_database)(struct App* app, const char* path);
} FlipPassFileBrowserPluginV1;
