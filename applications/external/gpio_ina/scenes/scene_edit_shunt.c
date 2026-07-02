/* 
 * This file is part of the INA Meter application for Flipper Zero (https://github.com/cepetr/flipper-tina).
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

#include "app.h"

static void scene_edit_shunt_input_callback(void* context, int32_t number) {
    App* app = (App*)context;
    app->config.shunt_resistor = number / 1.0E3;

    scene_manager_previous_scene(app->scene_manager);
}

static void scene_edit_shunt_init(App* app) {
    number_input_set_header_text(app->number_input, "Resistance [milliohm]");

    number_input_set_result_callback(
        app->number_input,
        scene_edit_shunt_input_callback,
        app,
        round(app->config.shunt_resistor * (double)1E3),
        0,
        1000000);
}

void scene_edit_shunt_on_enter(void* context) {
    App* app = (App*)context;

    scene_edit_shunt_init(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewNumberInput);
}

bool scene_edit_shunt_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;

    UNUSED(context);
    UNUSED(event);

    return consumed;
}

void scene_edit_shunt_on_exit(void* context) {
    App* app = (App*)context;

    UNUSED(app);
}
