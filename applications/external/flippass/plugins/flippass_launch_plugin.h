#pragma once
#include <flipper_application/flipper_application.h>
#include <stdbool.h>
#include <stdint.h>
struct App;
#define FLIPPASS_LAUNCH_PLUGIN_APP_ID      "flippass_launch"
#define FLIPPASS_LAUNCH_PLUGIN_API_VERSION 3U
typedef enum {
    FlipPassLaunchBrowse,
    FlipPassLaunchExit,
    FlipPassLaunchTypeMenu,
    FlipPassLaunchLock,
} FlipPassLaunchOutcome;
typedef struct {
    uint32_t api_version;
    bool (*start)(struct App* app, const char* arguments);
    FlipPassLaunchOutcome (*after_open)(struct App* app);
} FlipPassLaunchPluginV2;
