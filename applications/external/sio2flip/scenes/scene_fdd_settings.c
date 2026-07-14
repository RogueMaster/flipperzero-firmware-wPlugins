/*
 * This file is part of the 8-bit ATAR SIO Emulator for Flipper Zero
 * (https://github.com/cepetr/sio2flip).
 * Copyright (c) 2025
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "app/app.h"

#include "scenes.h"

typedef enum {
    MenuIndex_InsertDisk,
    MenuIndex_EjectDisk,
    MenuIndex_NewDisk,
    MenuIndex_SwapDisk,
    MenuIndex_WriteProtect,
} MenuIndex;

static void on_write_protect_changed(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, index ? "Yes" : "No");

    App* app = (App*)variable_item_get_context(item);
    DiskImage* image = fdd_get_disk(app->fdd[app->selected_fdd]);
    if(image != NULL) {
        disk_image_set_write_protect(image, index ? true : false);
    }
}

static void scene_settings_enter_callback(void* context, uint32_t index) {
    App* app = (App*)context;

    switch(index) {
    case MenuIndex_InsertDisk:
        scene_manager_next_scene(app->scene_manager, SceneFddSelect);
        break;

    case MenuIndex_EjectDisk:
        fdd_eject_disk(app->fdd[app->selected_fdd]);
        furi_string_reset(app->config.fdd[app->selected_fdd].image);
        scene_manager_previous_scene(app->scene_manager);
        break;

    case MenuIndex_NewDisk:
        scene_manager_next_scene(app->scene_manager, SceneFddNewDisk);
        break;

    case MenuIndex_SwapDisk:
        if(app->selected_fdd == 0) {
            // D1 <--> D2
            fdd_swap_disk(app->fdd[0], app->fdd[1]);
        } else {
            // Dx <--> D1
            fdd_swap_disk(app->fdd[app->selected_fdd], app->fdd[0]);
        }
        scene_manager_next_scene(app->scene_manager, SceneFddSwapDisk);
        break;
    }
}

void scene_fdd_settings_init(App* app) {
    VariableItemList* list = app->var_item_list;

    VariableItem* item;

    item = variable_item_list_add(list, "Insert disk...", 0, NULL, app);

    item = variable_item_list_add(list, "Eject disk", 0, NULL, app);

    item = variable_item_list_add(list, "New disk...", 0, NULL, app);

    if(app->selected_fdd == 0) {
        item = variable_item_list_add(list, "Swap with D2:", 0, NULL, app);
    } else {
        item = variable_item_list_add(list, "Swap with D1:", 0, NULL, app);
    }

    DiskImage* image = fdd_get_disk(app->fdd[app->selected_fdd]);
    if(image != NULL) {
        item = variable_item_list_add(list, "Write protect", 2, on_write_protect_changed, app);
        variable_item_set_current_value_index(item, disk_image_get_write_protect(image) ? 1 : 0);
        on_write_protect_changed(item);
    }

    variable_item_list_set_enter_callback(list, scene_settings_enter_callback, app);
}

void scene_fdd_settings_on_enter(void* context) {
    App* app = (App*)context;

    scene_fdd_settings_init(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewVariableList);
}

bool scene_fdd_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void scene_fdd_settings_on_exit(void* context) {
    App* app = (App*)context;

    variable_item_list_reset(app->var_item_list);
}
