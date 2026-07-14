#pragma once

#include "ibutton_converter.h"

#include <gui/gui.h>
#include <gui/view.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>

#include <ibutton/ibutton_protocols.h>

#include <storage/storage.h>
#include <dialogs/dialogs.h>

#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/loading.h>

#include "ibutton_converter_icons.h"

#include "ibutton_converter_custom_event.h"
#include "scenes/ibutton_converter_scene.h"

#define IBUTTON_APP_FOLDER             EXT_PATH("ibutton")
#define IBUTTON_APP_FILENAME_PREFIX    "iBtn"
#define IBUTTON_APP_FILENAME_EXTENSION ".ibtn"

#define IBUTTON_KEY_NAME_SIZE 23

struct iButtonConverter {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;

    Gui* gui;
    Storage* storage;
    DialogsApp* dialogs;

    iButtonKey* source_key;
    iButtonKey* converted_key;
    iButtonProtocols* protocols;

    FuriString* file_path;
    char key_name[IBUTTON_KEY_NAME_SIZE];

    Submenu* submenu;
    TextInput* text_input;
    Popup* popup;
    Widget* widget;
    Loading* loading;
};

typedef enum {
    iButtonConverterViewSubmenu,
    iButtonConverterViewTextInput,
    iButtonConverterViewPopup,
    iButtonConverterViewWidget,
    iButtonConverterViewLoading,
} iButtonConverterView;

bool ibutton_converter_select_and_load_key(iButtonConverter* ibutton_converter);
bool ibutton_converter_load_key(iButtonConverter* ibutton_converter, bool show_error);
bool ibutton_converter_save_key(iButtonConverter* ibutton_converter);
void ibutton_converter_reset_key(iButtonConverter* ibutton_converter);

void ibutton_converter_submenu_callback(void* context, uint32_t index);
void ibutton_converter_widget_callback(GuiButtonType result, InputType type, void* context);
