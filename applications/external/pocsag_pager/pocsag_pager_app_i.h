#pragma once

#include "helpers/pocsag_pager_types.h"
#include "helpers/pocsag_pager_storage.h"
#include "helpers/radio_device_loader.h"

#include "scenes/pocsag_pager_scene.h"
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <flipper_application/flipper_application.h>
#include "views/pocsag_pager_receiver.h"
#include "pocsag_pager_history.h"

#include <lib/subghz/subghz_setting.h>
#include <lib/subghz/subghz_worker.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/registry.h>
#include <lib/subghz/devices/devices.h>

typedef struct POCSAGPagerApp POCSAGPagerApp;

struct POCSAGPagerTxRx {
    SubGhzWorker* worker;

    SubGhzEnvironment* environment;
    SubGhzReceiver* receiver;
    SubGhzRadioPreset* preset;
    PCSGHistory* history;
    uint16_t idx_menu_chosen;
    PCSGTxRxState txrx_state;
    PCSGHopperState hopper_state;
    uint8_t hopper_timeout;
    uint8_t hopper_idx_frequency;
    PCSGRxKeyState rx_key_state;

    const SubGhzDevice* radio_device;
};

typedef struct POCSAGPagerTxRx POCSAGPagerTxRx;

struct POCSAGPagerApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    POCSAGPagerTxRx* txrx;
    SceneManager* scene_manager;
    NotificationApp* notifications;
    VariableItemList* variable_item_list;
    Submenu* submenu;
    Widget* widget;
    PCSGReceiver* pcsg_receiver;
    PCSGLock lock;
    SubGhzSetting* setting;

    PCSGSettings send_settings;

    // On-demand loaded Send/TX .fal plugin handles (NULL during RX)
    Storage* send_plugin_storage;
    FlipperApplication* send_plugin_app;
    const void* send_plugin_api;
    void* send_plugin_ctx;
};

void pcsg_preset_init(
    void* context,
    const char* preset_name,
    uint32_t frequency,
    uint8_t* preset_data,
    size_t preset_data_size);
void pcsg_get_frequency_modulation(
    POCSAGPagerApp* app,
    FuriString* frequency,
    FuriString* modulation);
void pcsg_begin(POCSAGPagerApp* app, uint8_t* preset_data);
uint32_t pcsg_rx(POCSAGPagerApp* app, uint32_t frequency);
void pcsg_idle(POCSAGPagerApp* app);
void pcsg_rx_end(POCSAGPagerApp* app);
void pcsg_sleep(POCSAGPagerApp* app);
void pcsg_hopper_update(POCSAGPagerApp* app);
