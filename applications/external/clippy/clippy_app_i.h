#pragma once

#include "clippy_app.h"

#include "gui/modules/loading.h"
#include "scenes/clippy_scene.h"
#include "storage/filesystem_api_internal.h"
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <dialogs/dialogs.h>
#include <notification/notification_messages.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <furi_hal_usb.h>
#include "storage/storage.h"
#include "views/clippy_mass_storage_view.h"
#include "m-string.h"
#include "m-array.h"
#include "helpers/mass_storage_usb.h"
#include "helpers/bad_usb_hid.h"
#include "views/clippy_bad_usb_view.h"
#include "views/clippy_main_view.h"
#include "clippy_icons.h"

#define CLIPPY_APP_BASE_FOLDER        EXT_PATH("clippy")
#define CLIPPY_APP_PATH_LAYOUT_FOLDER APP_ASSETS_PATH("/layouts")
#define CLIPPY_APP_LAYOUT_EXTENSION   ".kl"

typedef enum {
    ClippyAppErrorCloseRpc,
} ClippyAppError;

ARRAY_DEF(items_array, string_t);

struct ClippyApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    DialogsApp* dialogs;
    Storage* fs_api;
    Widget* widget;

    VariableItemList* variable_item_list;
    items_array_t items_array;
    Loading* loading;

    ClippyAppError error;

    // Mass Storage View
    ClippyMassStorage* mass_storage_view;
    FuriString* fat_image_file_path;
    File* file;
    FuriMutex* usb_mutex;
    MassStorageUsb* usb;

    // BadUsb view
    ClippyBadUsb* bad_usb_view;
    BadUsbHidInterface interface;
    FuriHalUsbInterface* usb_if_prev;
    FuriString* file_path;
    FuriString* keyboard_layout;
    BadUsbPayload* bad_usb_payload;
    FuriString* string_to_print;
    uint32_t delay;

    // Main view
    ClippyMain* main_view;

    uint32_t bytes_read, bytes_written;
};

typedef enum {
    ClippyAppViewCopyPasteSelection,
    ClippyAppViewPasteItemSelection,
    ClippyAppViewUsbLocked,
    ClippyAppViewMassStorageWork,
    ClippyAppViewLoading,
    ClippyAppViewBadUsbWork,
    ClippyAppViewBadUsbConfig,
    ClippyAppViewBadUsbConfigLayout,
} ClippyAppView;

enum ClippyCustomEvent {
    // Reserve first 100 events for button types and indexes, starting from 0
    ClippyMassStorageCustomEventReserved = 100,

    ClippyMassStorageCustomEventEject,
};

void clippy_app_show_loading_popup(ClippyApp* app, bool show);
