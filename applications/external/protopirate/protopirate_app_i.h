// protopirate_app_i.h
#pragma once

#include "helpers/protopirate_types.h"
#include "helpers/protopirate_settings.h"
#include "scenes/protopirate_scene.h"
#include "views/protopirate_receiver.h"
#include "views/protopirate_receiver_info.h"
#include "protopirate_history.h"
#include "helpers/radio_device_loader.h"

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <notification/notification_messages.h>
#include <lib/subghz/subghz_setting.h>
#include <lib/subghz/subghz_worker.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/transmitter.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/subghz_file_encoder_worker.h>
#include <dialogs/dialogs.h>
#include "defines.h"
#include "protocols/psa.h"

#define PROTOPIRATE_KEYSTORE_DIR_NAME APP_ASSETS_PATH("encrypted")

typedef struct ProtoPirateApp ProtoPirateApp;

typedef struct {
    SubGhzWorker* worker;
    SubGhzEnvironment* environment;
    SubGhzReceiver* receiver;
    SubGhzRadioPreset* preset;
    ProtoPirateHistory* history;
    const SubGhzDevice* radio_device;
    ProtoPirateTxRxState txrx_state;
    ProtoPirateHopperState hopper_state;
    ProtoPirateRxKeyState rx_key_state;
    uint8_t hopper_idx_frequency;
    uint8_t hopper_timeout;
    uint16_t idx_menu_chosen;
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
    View* view_about;
    FuriString* file_path;
    ProtoPirateReceiver* protopirate_receiver;
    ProtoPirateReceiverInfo* protopirate_receiver_info;
    ProtoPirateTxRx* txrx;
    SubGhzSetting* setting;
    ProtoPirateLock lock;
    FuriString* loaded_file_path;
    bool auto_save;
    bool radio_initialized;
    ProtoPirateSettings settings;
    uint32_t start_tx_time;
    uint8_t tx_power;
    PsaBfState* psa_bf_state;
    FuriThread* psa_bf_thread;
};

typedef enum {
    ProtoPirateSetTypeFord_v0,
    ProtoPirateSetTypeMAX,
} ProtoPirateSetType;

void protopirate_preset_init(
    void* context,
    const char* preset_name,
    uint32_t frequency,
    uint8_t* preset_data,
    size_t preset_data_size);

const char* preset_name_to_short(const char* preset_name);

void protopirate_get_frequency_modulation(
    ProtoPirateApp* app,
    FuriString* frequency,
    FuriString* modulation);

void protopirate_begin(ProtoPirateApp* app, uint8_t* preset_data);
uint32_t protopirate_rx(ProtoPirateApp* app, uint32_t frequency);
void protopirate_idle(ProtoPirateApp* app);
void protopirate_rx_end(ProtoPirateApp* app);
void protopirate_sleep(ProtoPirateApp* app);
void protopirate_hopper_update(ProtoPirateApp* app);
void protopirate_tx(ProtoPirateApp* app, uint32_t frequency);
void protopirate_tx_stop(ProtoPirateApp* app);
bool protopirate_radio_init(ProtoPirateApp* app);
void protopirate_radio_deinit(ProtoPirateApp* app);

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
