#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <lib/flipper_application/flipper_application.h>
#include <gui/scene_manager.h>
#include <flipper_format/flipper_format.h>

#include "../../protocols/psa_bf_types.h"

#define PROTOPIRATE_PSA_BF_PLUGIN_APP_ID      "protopirate_psa_bf_plugin"
#define PROTOPIRATE_PSA_BF_PLUGIN_API_VERSION 1U

typedef struct ProtoPirateApp ProtoPirateApp;
typedef struct ProtoPirateHistory ProtoPirateHistory;
typedef struct Widget Widget;

typedef enum {
    ProtoPiratePsaBfContextReceiverInfo,
    ProtoPiratePsaBfContextSubDecode,
} ProtoPiratePsaBfContext;

typedef struct {
    bool (*ensure_widget)(void* app);
    Widget* (*get_widget)(void* app);
    FlipperFormat* (*get_history_flipper_format)(void* app);
    uint16_t (*get_history_index)(void* app);
    void (*set_history_index)(void* app, uint16_t idx);
    ProtoPirateHistory* (*get_history)(void* app);
    void (*history_set_item_str)(void* app, uint16_t idx, const char* str);
    void (*patch_flipper_format_on_success)(FlipperFormat* ff, const PsaBfState* state);
    void (*send_custom_event)(void* app, uint32_t event);
    void (*notification_error)(void* app);
    void (*notification_success)(void* app);
    void (*receiver_info_rebuild_widget)(void* app);
    void (*subdecode_signal_info_refresh)(void* app);
    void (*scene_previous)(void* app);
} ProtoPiratePsaBfHostApi;

typedef struct {
    const char* plugin_name;
    void (*set_host_api)(const ProtoPiratePsaBfHostApi* api);
    bool (*needs_bruteforce)(void* app, ProtoPiratePsaBfContext ctx);
    bool (*is_running)(void* app);
    void (*on_scene_enter)(void* app, ProtoPiratePsaBfContext ctx);
    bool (*on_scene_event)(void* app, ProtoPiratePsaBfContext ctx, SceneManagerEvent event);
    void (*on_scene_exit)(void* app, ProtoPiratePsaBfContext ctx);
    bool (*widget_left_should_bruteforce)(void* app, ProtoPiratePsaBfContext ctx);
    void (*context_release)(void* app);
} ProtoPiratePsaBfPlugin;
