// protopirate_app_i.h
#pragma once

#include <stddef.h>
#include "helpers/protopirate_types.h"
#include "scenes/protopirate_scene.h"
#include "views/protopirate_receiver.h"
#include "views/protopirate_remote_analyzer.h"
#include "protopirate_history.h"
#include "helpers/radio_device_loader.h"

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>

#include <gui/modules/widget.h>
#include <gui/modules/text_input.h>
#include <notification/notification_messages.h>
#include <lib/subghz/subghz_setting.h>
#include <lib/subghz/subghz_worker.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/transmitter.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/subghz_file_encoder_worker.h>
#include <dialogs/dialogs.h>
#include "defines.h"
#include "protocols/protocols_common.h"
#include "protocols/protocol_items.h"
#include "helpers/protopirate_plugins.h"
#include "helpers/protopirate_views.h"
#include "helpers/protopirate_radio.h"
#include "helpers/protopirate_protocol_plugin_host.h"
#include "protocols/protopirate_protocol_plugins.h"
#include "helpers/protopirate_txrx.h"
#include "helpers/protopirate_models.h"
#include "helpers/protopirate_settings.h"

#ifdef ENABLE_MODELS_DATABASE
#define PROTOPIRATE_KEYSTORE_DIR_NAME APP_ASSETS_PATH("keystore/encrypted")
#else
#define PROTOPIRATE_KEYSTORE_DIR_NAME APP_ASSETS_PATH("encrypted")
#endif

typedef struct VariableItemList VariableItemList;

typedef struct ProtoPirateTxRx {
    SubGhzWorker* worker;
    SubGhzEnvironment* environment;
    SubGhzReceiver* receiver;
    SubGhzRadioPreset* preset;
    const SubGhzProtocolRegistry* protocol_registry;
    FlipperApplication* protocol_plugin_flipper_application;
    ProtoPiratePlugin running_plugin;
    ProtoPirateProtocolRegistryRoute protocol_registry_route;
    ProtoPirateHistory* history;
    const SubGhzDevice* radio_device;
    ProtoPirateTxRxState txrx_state;
    ProtoPirateHopperState hopper_state;
    ProtoPirateRxKeyState rx_key_state;
    uint16_t idx_menu_chosen;
    uint8_t hopper_rssi;
    uint8_t hopper_idx_frequency;
    uint8_t hopper_timeout;
} ProtoPirateTxRx;

struct ProtoPirateApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;
    DialogsApp* dialogs;
    VariableItemList* variable_item_list;
    Submenu* submenu;
    Widget* widget;
    TextInput* text_input;
    View* view_about;
    char* file_path;
    ProtoPirateReceiver* protopirate_receiver;
    FuriTimer* deferred_storage_timer;
    ProtoPirateTxRx* txrx;
    SubGhzSetting* setting;
    ProtoPirateLock lock;
    char* loaded_file_path;
    char* save_filename;
    ProtoPiratePlugin running_plugin;
    FlipperApplication* running_plugin_flipper_application;
    FlipperApplication* bruteforce_plugin_flipper_application;
    ProtoPiratePlugin running_bruteforce_plugin;
    uint32_t start_tx_time;
#ifdef ENABLE_MODELS_DATABASE
    ProtoPirateCarModel* selected_model;
    uint16_t car_models_count;
#endif
    uint16_t save_history_idx;
    uint8_t tx_power;
    /*****************/
    // Byte 1
    uint8_t deferred_storage_in_progress : 1;
    uint8_t auto_save                    : 1;
    uint8_t check_saved                  : 1;
    uint8_t sound                        : 1;
    uint8_t datetime_filenames           : 1;
    uint8_t radio_initialized            : 1;
    uint8_t emulate_disabled_for_loaded  : 1;
    uint8_t emulate_feature_enabled      : 1;
    // Byte 2
    uint8_t key_found                    : 1;
    uint8_t reserved                     : 7;
    /*****************/
};

typedef enum {
    ProtoPirateSetTypeFord_v0,
    ProtoPirateSetTypeMAX,
} ProtoPirateSetType;

void protopirate_app_free(ProtoPirateApp* app);

static const NotificationSequence sequence_tx = {
    &message_note_c5,
    &message_vibro_on,
    &message_red_255,
    &message_blue_255,
    &message_blink_start_10,
    &message_delay_25,
    &message_vibro_off,
    &message_delay_25,
    &message_sound_off,
    NULL,
};
