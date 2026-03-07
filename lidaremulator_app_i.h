#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>

#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>

#include <notification/notification_messages.h>
#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>

#include "scenes/lidaremulator_scene.h"

#include "view_hijacker.h"

/** IR output selection: internal LED or external via extension header A7 */
typedef enum {
    LidarEmulatorIrOutputInternal,
    LidarEmulatorIrOutputExternal,
    LidarEmulatorIrOutputNum,
} LidarEmulatorIrOutput;

struct LidarEmulatorApp {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;

    Gui* gui;

    Submenu* submenu;
    VariableItemList* variable_item_list;

    ViewInputCallback tmp_input_callback;

    ViewHijacker* view_hijacker;

    /** Selected IR output pin (internal vs external A7) */
    LidarEmulatorIrOutput ir_output;

    /** Enable 5V on extension header when using external IR */
    bool ir_ext_5v_enabled;

    /** Transmit worker thread (non-null while transmitting) */
    FuriThread* transmit_thread;
};

typedef struct LidarEmulatorApp LidarEmulatorApp;

/** Load settings from storage into app state. */
void lidaremulator_load_settings(LidarEmulatorApp* app);

/** Enumeration of all used view types. */
typedef enum {
    LidarEmulatorViewSubmenu,
    LidarEmulatorViewVariableList,
} LidarEmulatorView;

