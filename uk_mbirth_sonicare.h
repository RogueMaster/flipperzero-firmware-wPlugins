#pragma once

#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <gui/view.h>

#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <notification/notification_messages.h>

#include <gui/modules/submenu.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/popup.h>
#include <gui/modules/loading.h>
#include <gui/modules/text_input.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/widget.h>

#include <loader/loader.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <toolbox/path.h>

#include "scenes/sonicare_scene.h"

typedef struct Sonicare Sonicare;

struct Sonicare {
    ViewDispatcher* view_dispatcher;
    Gui* gui;
    NotificationApp* notifications;
    SceneManager* scene_manager;
    Storage* storage;
    DialogsApp* dialogs;
    Widget* widget;
    
    // Common Views
    Submenu* submenu;
    DialogEx* dialog_ex;
    Popup* popup;
    TextInput* text_input;
    ByteInput* byte_input;
};

typedef enum {
    SonicareViewSubmenu,
    SonicareViewDialogEx,
    SonicareViewPopup,
    SonicareViewWidget,
    SonicareViewTextInput,
    SonicareViewByteInput,
    SonicareViewRead,
} SonicareView;

typedef enum {
    SonicareMenuIndexRead,
    SonicareMenuIndexSaved,
    SonicareMenuIndexAddManually,
    SonicareMenuIndexExtraActions,
} SonicareMenuIndex;
