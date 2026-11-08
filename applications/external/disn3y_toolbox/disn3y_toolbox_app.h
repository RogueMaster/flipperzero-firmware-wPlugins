#pragma once

#include <furi.h>
#include <furi_hal_bt.h>
#include <gui/gui.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/popup.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/scene_manager.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <lfrfid/lfrfid_worker.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <toolbox/protocols/protocol_dict.h>

#include "data/droid_beacons.h"
#include "data/kyber_crystals.h"
#include "data/location_beacons.h"
#include "data/magicband_codes.h"
#include "extra_beacon.h"
#include "scenes/scenes.h"

#define TAG "DisneyToolbox"

enum DisneyToolboxCustomEvent {
    DisneyToolboxEventNext = 100,
    DisneyToolboxEventPopupClosed,
    DisneyToolboxEventWriteOK,
    DisneyToolboxEventWriteProtocolCannotBeWritten,
    DisneyToolboxEventWriteFobCannotBeWritten,
    DisneyToolboxEventWriteTooLongToWrite,
    DisneyToolboxEventReadDone,
};

typedef struct {
    Gui* gui;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    // UI modules
    Submenu* submenu;
    DialogEx* dialog_ex;
    Widget* widget;
    Popup* popup;
    VariableItemList* var_item_list;
    View* droid_view;

    // Kyber Crystal state
    FuriString* selection_string;
    CrystalSeries1ID selected_s1;
    CrystalSeries2ID selected_s2;
    uint8_t kyber_series;
    LFRFIDWorker* lfworker;
    ProtocolDict* protocol_dict;
    ProtocolId protocol_id;
    bool write_animation_paused;

    // MagicBand Beacon state
    FuriString* status_string;
    GapExtraBeaconConfig beacon_config;
    uint8_t beacon_data[EXTRA_BEACON_MAX_DATA_SIZE];
    uint8_t beacon_data_len;
    bool is_beacon_active;
    bool preset_mode;
    const char* preset_name;
    MagicBandCodeType selected_code_type;
    MagicBandCodeParams code_params;

    // Droid Beacon state
    DroidPersonality selected_droid_personality;
    bool droid_paired;

    // Droid Location state
    DroidLocation selected_droid_location;
    uint8_t droid_loc_interval_idx;
    uint8_t droid_loc_rssi_idx;
    uint8_t droid_loc_field;  // LocField enum from scene_droid_location

    // Animation
    uint8_t animation_counter;
    bool animation_counter_direction;
} DisneyToolboxApp;

typedef enum {
    DisneyToolboxAppViewSubmenu,
    DisneyToolboxAppViewDialog,
    DisneyToolboxAppViewWidget,
    DisneyToolboxAppViewPopup,
    DisneyToolboxAppViewConfig,
    DisneyToolboxAppViewDroid,
} DisneyToolboxAppView;

void disney_toolbox_app_popup_timeout_callback(void* context);
