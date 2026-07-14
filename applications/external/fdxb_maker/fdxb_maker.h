#pragma once

#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>

#include <flipper_format/flipper_format.h>
#include <lfrfid/lfrfid_dict_file.h>
#include <bit_lib/bit_lib.h>
#include <toolbox/path.h>

#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_input.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/number_input.h>
#include "modules/big_number_input.h"
#include "modules/fdxb_temperature_input.h"
#include <gui/modules/widget.h>

#include <fdxb_maker_icons.h>
#include "scenes/fdxb_maker_scene.h"

#define TAG "FdxbMaker"

#define FDXB_MAKER_KEY_NAME_SIZE   22
#define FDXB_MAKER_TEXT_STORE_SIZE 40
#define FDXB_MAKER_BYTE_STORE_SIZE 4

#define FDXB_DECODED_DATA_SIZE (11)

#define LFRFID_APP_FOLDER             EXT_PATH("lfrfid")
#define LFRFID_APP_FILENAME_PREFIX    "RFID"
#define LFRFID_APP_FILENAME_EXTENSION ".rfid"

enum FdxbMakerCustomEvent {
    FdxbMakerEventNext = 100,
    FdxbMakerEventPopupClosed,
    FdxbMakerEventNonstandardValue
};

typedef enum {
    FdxbTrailerModeOff,
    FdxbTrailerModeAppData,
    FdxbTrailerModeTemperature
} FdxbTrailerMode;

typedef struct FdxbExpanded FdxbExpanded;

//TODO: Pack?
struct FdxbExpanded {
    uint64_t national_code;
    uint16_t country_code;
    bool RUDI_bit;
    uint8_t RFU;
    uint8_t visual_start_digit;
    uint8_t user_info;
    uint8_t retagging_count;
    bool animal;
    FdxbTrailerMode trailer_mode;
    uint32_t trailer;
    float temperature;
};

typedef struct FdxbMaker FdxbMaker;

struct FdxbMaker {
    Gui* gui;
    Storage* storage;
    DialogsApp* dialogs;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;

    Submenu* submenu;
    Widget* widget;
    VariableItemList* variable_item_list;
    Popup* popup;
    TextInput* text_input;
    ByteInput* byte_input;
    NumberInput* number_input;
    BigNumberInput* big_number_input;
    FdxbTemperatureInput* fdxb_temperature_input;

    char text_store[FDXB_MAKER_TEXT_STORE_SIZE + 1];
    uint8_t byte_store[FDXB_MAKER_BYTE_STORE_SIZE];
    FuriString* file_name;
    FuriString* file_path;
    FdxbExpanded* data;
    uint8_t* bytes;
    ProtocolDict* dict;
    ProtocolDict* tmp_dict;

    bool needs_restore;
};

typedef enum {
    FdxbMakerViewSubmenu,
    FdxbMakerViewVarItemList,
    FdxbMakerViewPopup,
    FdxbMakerViewTextInput,
    FdxbMakerViewByteInput,
    FdxbMakerViewNumberInput,
    FdxbMakerViewBigNumberInput,
    FdxbMakerViewFdxbTemperatureInput,
    FdxbMakerViewWidget
} FdxbMakerView;

typedef enum {
    FdxbMakerMenuIndexNew,
    FdxbMakerMenuIndexEdit
} FdxbMakerMenuIndex;

void fdxb_maker_construct_data(FdxbMaker* app);

void fdxb_maker_deconstruct_data(FdxbMaker* app);

void fdxb_maker_reset_data(FdxbMaker* app);

bool fdxb_maker_save_key(FdxbMaker* app);

bool fdxb_maker_load_key_from_file_select(FdxbMaker* app);

bool fdxb_maker_delete_key(FdxbMaker* app);

bool fdxb_maker_load_key_data(FdxbMaker* app, FuriString* path, bool show_dialog);

bool fdxb_maker_save_key_data(FdxbMaker* app, FuriString* path);

bool fdxb_maker_save_key_and_switch_scenes(FdxbMaker* app);

void fdxb_maker_make_app_folder(FdxbMaker* app);

void fdxb_maker_text_store_set(FdxbMaker* app, const char* text, ...);

void fdxb_maker_text_store_clear(FdxbMaker* app);

void fdxb_maker_widget_callback(GuiButtonType result, InputType type, void* context);

void fdxb_maker_popup_timeout_callback(void* context);

void fdxb_maker_text_input_callback(void* context);

void fdxb_maker_byte_input_callback(void* context);
