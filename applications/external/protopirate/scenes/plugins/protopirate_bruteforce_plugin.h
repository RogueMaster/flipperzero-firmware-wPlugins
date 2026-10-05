#pragma once

#include "../../helpers/protopirate_plugins_host_api.h"

typedef struct ProtoPirateApp ProtoPirateApp;
typedef struct ProtoPirateHistory ProtoPirateHistory;
typedef struct Widget Widget;

typedef enum {
    ProtoPirateBruteForceContextReceiverInfo,
    ProtoPirateBruteForceContextSubDecode,
    ProtoPirateBruteForceContextSavedInfo,
} ProtoPirateBruteForceContext;

typedef struct {
    const char* plugin_name;
    void (*set_host_api)(const ProtoPirateSharedPluginHostApi* api);
    bool (*needs_bruteforce)(FlipperFormat* ff);
    bool (*is_running)(ProtoPirateApp* app);
    void (*on_scene_enter)(ProtoPirateApp* app, ProtoPirateBruteForceContext ctx);
    bool (*on_scene_event)(
        ProtoPirateApp* app,
        ProtoPirateBruteForceContext ctx,
        SceneManagerEvent event);
    void (*on_scene_exit)(ProtoPirateApp* app, ProtoPirateBruteForceContext ctx);
    bool (*widget_left_should_bruteforce)(ProtoPirateApp* app, FlipperFormat* ff);
    void (*release)(ProtoPirateApp* app);
} ProtoPirateBruteForcePlugin;
