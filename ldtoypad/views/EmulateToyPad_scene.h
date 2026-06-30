#pragma once

#include <furi.h>

#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>

#include "../usb/usb_toypad.h"
#include "sub_screens.h"

typedef struct LDToyPadSceneEmulate LDToyPadSceneEmulate;

LDToyPadSceneEmulate* ldtoypad_scene_emulate_alloc();

void ldtoypad_scene_emulate_free(LDToyPadSceneEmulate* ldtoypad_emulate);

View* ldtoypad_scene_emulate_get_view(LDToyPadSceneEmulate* ldtoypad_scene_emulate);

typedef enum {
    MiniSelectionFavorite,
    MiniSelectionSave,
    MiniSelectionCount // Total number of selections, must always be last
} MiniSelectionType;

typedef struct {
    bool left_pressed;
    bool right_pressed;
    bool up_pressed;
    bool down_pressed;
    bool ok_pressed;
    // bool back_pressed;
    bool back_long_pressed;
    bool connected;
    char* connection_status;

    uint32_t selected_minifigure_index;
    uint32_t selected_vehicle_index;

    usbd_device* usbDevice;

    // setting indexes below
    bool show_debug_text_index;
    bool show_icons_index;
    bool minifig_only_mode;

    bool show_mini_menu_selected;
    MiniSelectionType mini_option_selected;

    // Inner-screens / pseudo-screens
    bool show_placement_selection_screen;
    SelectionType sub_screen_box_selected;
} LDToyPadSceneEmulateModel;

void minifigures_submenu_callback(void* context, uint32_t index);
void vehicles_submenu_callback(void* context, uint32_t index);
void saved_token_submenu_callback(void* context, uint32_t index);

extern struct BoxInfo boxInfo[NUM_BOXES]; // Array of box information
