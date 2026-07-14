#include "../application.h"
#include "scenes.h"
#include <stdio.h>

static const char* mode_names[] = {
    "",
    "Hoerspiel (Random)",
    "Album (Complete)",
    "Party (Shuffle)",
    "Einzel (Single)",
    "Hoerbuch (Progress)",
    "Admin",
    "Hoerspiel Von-Bis",
    "Album Von-Bis",
    "Party Von-Bis",
    "Hoerbuch Einzel",
    "Repeat Last",
};

static void tonuino_scene_set_mode_callback(void* context, uint32_t index) {
    TonuinoApp* app = context;
    // Save selected index before navigating away
    scene_manager_set_scene_state(app->scene_manager, TonuinoSceneSetMode, index - 1);
    scene_manager_handle_custom_event(app->scene_manager, index);
}

void tonuino_scene_set_mode_on_enter(void* context) {
    TonuinoApp* app = context;

    submenu_reset(app->submenu);

    // Add mode items with current mode highlighted
    char mode_label[64];
    for(int i = 1; i <= ModeRepeatLast; i++) {
        if(i == app->card_data.mode && app->card_data.mode >= 1 &&
           app->card_data.mode <= ModeRepeatLast) {
            snprintf(mode_label, sizeof(mode_label), "* %s", mode_names[i]);
        } else {
            snprintf(mode_label, sizeof(mode_label), "  %s", mode_names[i]);
        }
        submenu_add_item(app->submenu, mode_label, i, tonuino_scene_set_mode_callback, app);
    }

    // Restore cursor position if we've been here before
    uint32_t selected_item =
        scene_manager_get_scene_state(app->scene_manager, TonuinoSceneSetMode);
    if(selected_item == 0 && app->card_data.mode >= 1 && app->card_data.mode <= ModeRepeatLast) {
        // First time entering or state was 0: use current mode
        selected_item = app->card_data.mode - 1;
    }
    submenu_set_selected_item(app->submenu, selected_item);

    view_dispatcher_switch_to_view(app->view_dispatcher, TonuinoViewSubmenu);
}

bool tonuino_scene_set_mode_on_event(void* context, SceneManagerEvent event) {
    TonuinoApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        // Mode selected (1-11)
        if(event.event >= 1 && event.event <= ModeRepeatLast) {
            app->card_data.mode = event.event;
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }

    return false;
}

void tonuino_scene_set_mode_on_exit(void* context) {
    TonuinoApp* app = context;
    submenu_reset(app->submenu);
}
