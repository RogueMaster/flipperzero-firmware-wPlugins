#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <gui/view.h>

#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>

// #include <notification/notification.h>
// #include <notification/notification_messages.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>

#include "views/EmulateToyPad_scene.h"

#define MAX_SAVED_TOKENS 100

typedef struct {
    ViewDispatcher* view_dispatcher; // Switches between our views

    Submenu* submenu; // The application menu

    VariableItemList* variable_item_list_config; // The configuration screen

    LDToyPadSceneEmulate* view_scene_emulate; // The emulator screen

    Widget* widget_about; // The about screen

    Submenu* submenu_minifigure_selection; // The minifigure selection screen
    Submenu* submenu_vehicle_selection; // The vehicle selection screen
    Submenu* submenu_favorites_selection; // The favorites selection screen
    Submenu* submenu_saved_selection; // The saved selection screen

    FuriString*
        saved_token_paths[MAX_SAVED_TOKENS]; // Array of saved token file paths to be freed later
    uint8_t saved_token_count;

} LDToyPadApp;

// Each view is a screen we show the user.
typedef enum {
    ViewSubmenu, // The menu when the app starts
    ViewConfigure, // The configuration screen
    ViewEmulate, // The main screen
    ViewAbout, // The about screen with directions, link to social channel, etc.

    ViewMinifigureSelection, // The minifigure selection screen
    ViewVehicleSelection, // The vehicle selection screen
    ViewFavoritesSelection, // The favorites selection screen
    ViewSavedSelection, // The saved selection screen
} Views;

ViewDispatcher* get_view_dispatcher();

#ifdef __cplusplus
}
#endif
