#pragma once

#include "updi/updi_session.h"

#include <dialogs/dialogs.h>
#include <expansion/expansion.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_box.h>
#include <gui/modules/text_input.h>

#include <furi.h>

typedef enum {
    UpdiViewSubmenu,
    UpdiViewVarList,
    UpdiViewWidget,
    UpdiViewProgress,
    UpdiViewText,
    UpdiViewTextInput,
} UpdiView;

typedef enum {
    UpdiSceneMenu,
    UpdiScenePins,
    UpdiSceneWiring,
    UpdiSceneAbout,
    UpdiSceneDetect,
    UpdiSceneDumpName,
    UpdiSceneFuseWrite,
    UpdiSceneProgrammer,
    UpdiSceneWork,
    UpdiSceneCount,
} UpdiScene;

typedef enum {
    UpdiMenuDetect,
    UpdiMenuDump,
    UpdiMenuFlash,
    UpdiMenuReadEeprom,
    UpdiMenuWriteEeprom,
    UpdiMenuFuses,
    UpdiMenuWriteFuse,
    UpdiMenuProgrammer,
    UpdiMenuPins,
    UpdiMenuWiring,
    UpdiMenuAbout,
} UpdiMenuIndex;

typedef enum {
    UpdiOpDetect,
    UpdiOpFlash,
    UpdiOpDump,
    UpdiOpFuses,
    UpdiOpReadEeprom,
    UpdiOpWriteEeprom,
    UpdiOpWriteFuse,
} UpdiOp;

typedef enum {
    UpdiEventProgress = 1,
    UpdiEventFinished = 2,
} UpdiEvent;

#define UPDI_APPS_DATA_DIR "/ext/apps_data/avr_updi"
#define UPDI_PINS_PATH     UPDI_APPS_DATA_DIR "/pins.txt"
#define UPDI_DUMP_NAME_LEN 32

typedef struct {
    char status[40];
    uint8_t percent;
} UpdiProgressModel;

typedef struct UpdiApp {
    Gui* gui;
    DialogsApp* dialogs;
    Expansion* expansion;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Submenu* submenu;
    VariableItemList* var_list;
    Widget* widget;
    View* progress;
    TextBox* text_box;
    TextInput* text_input;
    FuriThread* thread;
    FuriMutex* mutex;
    FuriString* hex_path;
    FuriString* eeprom_path;

    UpdiOp op;
    UpdiPins pins;
    char pins_label[24];
    volatile bool cancel;
    bool showing_result;
    char status[40];
    uint8_t percent;
    char result[1024];

    char dump_name[UPDI_DUMP_NAME_LEN];
    uint8_t fuse_index;
    uint8_t fuse_value;
    char fuse_input[16];
    bool usb_connected;
    void* usb_bridge;
} UpdiApp;

int32_t updi_worker(void* context);
void updi_pins_load(UpdiApp* app);
void updi_pins_save(UpdiApp* app);
const char* updi_pins_label(UpdiPins pins);

void updi_usb_bridge_scene_on_enter(void* context);
bool updi_usb_bridge_scene_on_event(void* context, SceneManagerEvent event);
void updi_usb_bridge_scene_on_exit(void* context);
