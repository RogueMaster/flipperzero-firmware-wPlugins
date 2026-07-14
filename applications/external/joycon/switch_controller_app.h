#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_input.h>
#include <gui/modules/popup.h>
#include <dialogs/dialogs.h>
#include <notification/notification_messages.h>

#include "usb_hid_switch.h"
#include "macro.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SWITCH_CONTROLLER_APP_PATH_FOLDER "/ext/apps_data/switch_controller"
#define SWITCH_CONTROLLER_MACRO_EXTENSION ".scm"

// Forward declarations
typedef struct SwitchControllerApp SwitchControllerApp;

// View types
typedef enum {
    SwitchControllerViewSubmenu,
    SwitchControllerViewController,
    SwitchControllerViewRecording,
    SwitchControllerViewPlaying,
    SwitchControllerViewTextInput,
    SwitchControllerViewMacroList,
    SwitchControllerViewPopup,
} SwitchControllerView;

// Scene types
typedef enum {
    SwitchControllerSceneMenu,
    SwitchControllerSceneController,
    SwitchControllerSceneRecording,
    SwitchControllerScenePlaying,
    SwitchControllerSceneMacroName,
    SwitchControllerSceneMacroList,
    SwitchControllerSceneNum,
} SwitchControllerScene;

// App modes
typedef enum {
    AppModeController,
    AppModeRecording,
    AppModePlaying,
} AppMode;

// Main app structure
struct SwitchControllerApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;
    DialogsApp* dialogs;

    // Views
    Submenu* submenu;
    VariableItemList* macro_list;
    TextInput* text_input;
    Popup* popup;
    View* controller_view;
    View* recording_view;
    View* playing_view;

    // USB and controller state
    SwitchControllerState controller_state;
    bool usb_connected;
    FuriHalUsbInterface* usb_mode_prev;

    // Macro system
    Macro* current_macro;
    MacroRecorder* recorder;
    MacroPlayer* player;
    char macro_name_buffer[MACRO_NAME_MAX_LEN];

    // App state
    AppMode mode;
    FuriString* temp_path;
    FuriTimer* timer;
};

// Scene handlers
void switch_controller_scene_on_enter_menu(void* context);
bool switch_controller_scene_on_event_menu(void* context, SceneManagerEvent event);
void switch_controller_scene_on_exit_menu(void* context);

void switch_controller_scene_on_enter_controller(void* context);
bool switch_controller_scene_on_event_controller(void* context, SceneManagerEvent event);
void switch_controller_scene_on_exit_controller(void* context);

void switch_controller_scene_on_enter_recording(void* context);
bool switch_controller_scene_on_event_recording(void* context, SceneManagerEvent event);
void switch_controller_scene_on_exit_recording(void* context);

void switch_controller_scene_on_enter_playing(void* context);
bool switch_controller_scene_on_event_playing(void* context, SceneManagerEvent event);
void switch_controller_scene_on_exit_playing(void* context);

void switch_controller_scene_on_enter_macro_name(void* context);
bool switch_controller_scene_on_event_macro_name(void* context, SceneManagerEvent event);
void switch_controller_scene_on_exit_macro_name(void* context);

void switch_controller_scene_on_enter_macro_list(void* context);
bool switch_controller_scene_on_event_macro_list(void* context, SceneManagerEvent event);
void switch_controller_scene_on_exit_macro_list(void* context);

#ifdef __cplusplus
}
#endif
