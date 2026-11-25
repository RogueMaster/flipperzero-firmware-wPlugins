#pragma once

#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <notification/notification_messages.h>
#include <lib/subghz/subghz_setting.h>
#include <lib/subghz/devices/devices.h>
#include "../helpers/pocsag_pager_storage.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PCSG_SEND_PLUGIN_APP_ID      "pocsag_pager_send"
#define PCSG_SEND_PLUGIN_API_VERSION 1U

typedef struct {
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;
    SubGhzSetting* setting;
    const SubGhzDevice* radio_device;
    bool is_external_radio;
    PCSGSettings* settings;
    uint32_t view_id_base;
    void (*save_settings)(const PCSGSettings* settings);
    void (*on_frequency_changed)(void* host_context, uint32_t frequency);
    void* host_context;
} PCSGSendHostApi;

typedef struct {
    void* (*create)(const PCSGSendHostApi* host);
    bool (*on_event)(void* plugin_ctx, SceneManagerEvent event);
    void (*destroy)(void* plugin_ctx);
} PCSGSendPluginInterface;

#ifdef __cplusplus
}
#endif
