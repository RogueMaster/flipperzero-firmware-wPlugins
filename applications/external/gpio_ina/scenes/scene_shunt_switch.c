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

#include "utils.h"

static void popup_callback(void* context) {
    App* app = (App*)context;

    // Swap shunt resistors
    double r = app->config.shunt_resistor;
    app->config.shunt_resistor = app->config.shunt_resistor_alt;
    app->config.shunt_resistor_alt = r;

    app_restart_sensor_driver(app);

    scene_manager_previous_scene(app->scene_manager);
}

void scene_shunt_switch_on_enter(void* context) {
    App* app = (App*)context;

    popup_set_header(app->popup, "Shunt resistor changed", 0, 0, AlignLeft, AlignTop);
    popup_set_text(
        app->popup,
        "Please verify that the\nconnected shunt resistor\nmatches the selected value.",
        64,
        24,
        AlignCenter,
        AlignTop);

    popup_set_timeout(app->popup, 1000);
    popup_set_context(app->popup, app);
    popup_set_callback(app->popup, popup_callback);

    popup_enable_timeout(app->popup);

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewWiring);
}

bool scene_shunt_switch_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;

    UNUSED(context);
    UNUSED(event);

    return consumed;
}

void scene_shunt_switch_on_exit(void* context) {
    App* app = (App*)context;

    popup_reset(app->popup);
}
