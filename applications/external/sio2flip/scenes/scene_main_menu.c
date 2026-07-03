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
    MenuIndex_EmulateFdd,
    MenuIndex_RunXex,
    MenuIndex_LedBlinking,
    MenuIndex_SioSpeedMode,
    MenuIndex_SioBaudrate,
    MenuIndex_Atari850,
    MenuIndex_WiringInfo,
} MenuIndex;

static void on_led_blinking_changed(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, index ? "Yes" : "No");

    App* app = (App*)variable_item_get_context(item);
    app->config.led_blinking = index ? true : false;
}

static void on_speed_mode_changed(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, speed_mode_options[index].text);

    App* app = (App*)variable_item_get_context(item);
    app->config.speed_mode = speed_mode_options[index].value;
}

static void on_baudrate_changed(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, speed_index_options[index].text);

    App* app = (App*)variable_item_get_context(item);
    app->config.speed_index = speed_index_options[index].value;
}

static void on_atari850_changed(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, index ? "Yes" : "No");

    App* app = (App*)variable_item_get_context(item);
    app->config.atari850 = index ? true : false;
}

static void scene_main_menu_enter_callback(void* context, uint32_t index) {
    App* app = (App*)context;

    switch(index) {
    case MenuIndex_EmulateFdd:
        app_start_fdd_emulation(app);
        scene_manager_next_scene(app->scene_manager, SceneFddInfo);
        break;
    case MenuIndex_RunXex:
        scene_manager_next_scene(app->scene_manager, SceneXexSelect);
        break;
    case MenuIndex_WiringInfo:
        scene_manager_next_scene(app->scene_manager, SceneWiring);
        break;
    }
}

void scene_main_menu_init(App* app) {
    VariableItemList* list = app->var_item_list;

    VariableItem* item;

    item = variable_item_list_add(list, "Start emulation...", 0, NULL, app);

    item = variable_item_list_add(list, "Run XEX file...", 0, NULL, app);

    item = variable_item_list_add(list, "LED blinking", 2, on_led_blinking_changed, app);
    variable_item_set_current_value_index(item, app->config.led_blinking ? 1 : 0);
    on_led_blinking_changed(item);

    item = variable_item_list_add(
        list, "Speed mode", ARRAY_SIZE(speed_mode_options), on_speed_mode_changed, app);
    variable_item_set_current_value_index(
        item, speed_mode_by_value(app->config.speed_mode) - speed_mode_options);
    on_speed_mode_changed(item);

    item = variable_item_list_add(
        list, "High speed", ARRAY_SIZE(speed_index_options), on_baudrate_changed, app);
    variable_item_set_current_value_index(
        item, speed_index_by_value(app->config.speed_index) - speed_index_options);
    on_baudrate_changed(item);

    item = variable_item_list_add(list, "Atari 850", 2, on_atari850_changed, app);
    variable_item_set_current_value_index(item, app->config.atari850 ? 1 : 0);
    on_atari850_changed(item);

    item = variable_item_list_add(list, "Wiring info", 0, NULL, app);

    variable_item_list_set_enter_callback(list, scene_main_menu_enter_callback, app);
}

void scene_main_menu_on_enter(void* context) {
    App* app = (App*)context;

    app_stop_emulation(app);

    scene_main_menu_init(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewVariableList);
}

bool scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void scene_main_menu_on_exit(void* context) {
    App* app = (App*)context;

    variable_item_list_reset(app->var_item_list);
}
