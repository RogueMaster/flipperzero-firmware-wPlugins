#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/menu.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_box.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_input.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <gui/modules/loading.h>
#include <infrared.h>
#include <infrared_worker.h>
#include <furi_hal_infrared.h>
// #include <bad_usb/helpers/bad_usb_hid.h>
#include <ble_profile/extra_profiles/hid_profile.h>
#include <ble_profile/extra_services/hid_service.h>
#include <bt/bt_service/bt.h>
#include <notification/notification_messages.h>
#include <fire_string_icons.h>

#include "helpers/bad_usb_hid/bad_usb_hid.h"

#define TAG                      "firestring-app"
#define DEFAULT_PATH             APP_DATA_PATH()
#define DICT_MAX_SIZE            512
#define STR_RESERVE_LEN          4096
#define FILE_EXT                 ".rnd"
#define TEXT_INPUT_BUF_SIZE      24
#define DICT_FILE                "eff_short_wordlist_1_alpha.txt"
#define HID_BT_KEYS_STORAGE_NAME ".bt_hid.keys"
#define HID_DEFAULT_LAYOUT       "en-US.kl"
#define ASCII_TO_KEY(script, x) \
    (((uint8_t)x < 128) ? (script->hid->layout[(uint8_t)x]) : HID_KEYBOARD_NONE)

// scene index
typedef enum {
    FireStringScene_MainMenu,
    FireStringScene_Settings,
    FireStringScene_Generate,
    FireStringScene_GenerateStepTwo,
    FireStringScene_LoadingUSB,
    FireStringScene_USB,
    FireStringScene_Bluetooth,
    FireStringScene_LoadString,
    FireStringScene_SaveString,
    FireStringScene_About,
    FireStringScene_Loading_Word_List,
    FireStringScene_count
} FireStringScene;

// view index
typedef enum {
    FireStringView_Menu,
    FireStringView_SubMenu,
    FireStringView_VariableItemList,
    FireStringView_Loading,
    FireStringView_Widget,
    FireStringView_TextInput,
} FireStringView;

// custom event index
typedef enum {
    FireStringEvent_ShowVariableItemList,
    FireStringEvent_ShowStringGeneratorView,
    FireStringEvent_ShowBadUSB,
    FireStringEvent_ShowFileBrowser,
    FireStringEvent_ShowSaved,
    FireStringCustomEvent_SaveDone,
    FireStringEvent_ShowAbout,
    FireStringEvent_Exit
} FireStringEvent;

typedef enum {
    StrType_AlphaNumSymb,
    StrType_Words,
    StrType_AlphaNum,
    StrType_Alpha,
    StrType_Symb,
    StrType_Num,
    StrType_Hex,
    StrType_Bin,
} StrType;

typedef enum {
    DelimType_Dash,
    DelimType_Plus,
    DelimType_Comma,
    DelimType_SemiColon,
    DelimType_ForwardSlash,
    DelimType_VerticalBar,
    DelimType_Space,
} DelimType;

// app settings
typedef struct {
    uint32_t str_len;
    uint8_t str_type;
    uint8_t delimiter;
    bool use_ir;
    bool file_loaded;
} FireStringSettings;

typedef struct {
    FuriHalBleProfileBase* ble_hid_profile;
    Bt* bt;
    bool bt_connected;
    const BadUsbHidApi* api;
    void* hid_inst;
    BadUsbHidInterface interface;
    FuriHalUsbInterface* usb_if_prev;
    uint16_t layout[128];
} FireStringHID;

typedef struct {
    FuriString* char_list;
    FuriString** word_list;
    size_t len;
} FireStringDictionary;

// app context
typedef struct {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Gui* gui;
    Menu* menu;
    Submenu* submenu;
    Widget* widget;
    VariableItemList* variable_item_list;
    TextInput* text_input;
    char text_buffer[TEXT_INPUT_BUF_SIZE];
    Loading* loading;
    InfraredWorker* ir_worker;
    FuriString* fire_string;
    FireStringDictionary* dict;
    FireStringHID* hid;
    FireStringSettings* settings;
    FuriThread* thread;
    NotificationApp* notifications;
} FireString;
