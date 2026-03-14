/*
    Unitemp - Universal temperature reader
    Copyright (C) 2022-2026  Victor Nikitchuk (https://github.com/quen0n)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "../unitemp.h"
#include "sensors.h"
#include "./helpers/unitemp_gpio.h"
#include "scenes/unitemp_scene.h"
#include "unitemp_icons.h"

#include "./interfaces/i2c_sensor.h"
#include "./interfaces/onewire_sensor.h"
#include "./interfaces/singlewire_sensor.h"
#include "./interfaces/spi_sensor.h"

void unitemp_scene_sensors_list_on_enter(void* context) {
    UnitempApp* app = context;
    Submenu* submenu = app->submenu;

    uint8_t sensor_models_count = unitemp_sensors_models_get_count();
    const SensorModel** models = unitemp_sensors_models_get();

    for(uint8_t i = 0; i < (sensor_models_count); i++) {
        submenu_add_item(
            submenu,
            (models[i]->altname == NULL) ? models[i]->modelname : models[i]->altname,
            i,
            unitemp_submenu_callback,
            app);
    }

    submenu_add_item(
        submenu, " * Need help? * ", sensor_models_count, unitemp_submenu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, UnitempViewSubmenu);
}

bool unitemp_scene_sensors_list_on_event(void* context, SceneManagerEvent event) {
    UnitempApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == unitemp_sensors_models_get_count()) {
            scene_manager_next_scene(app->scene_manager, UnitempSceneHelp);
            return true;
        }

        const SensorModel* model = unitemp_sensors_models_get()[event.event];
        do {
            //Checking Sensor Availability
            if(unitemp_gpio_get_aviable_pin(model->interface, 0, NULL) == NULL) {
                DialogMessage* message = dialog_message_alloc();
                dialog_message_set_icon(
                    message,
                    &I_confused_dolph_43x31,
                    0,
                    64 - icon_get_height(&I_confused_dolph_43x31));
                dialog_message_set_header(
                    message, "Unable to add a sensor", 64, 6, AlignCenter, AlignCenter);
                if(model->interface == &unitemp_singlewire || model->interface == &unitemp_1w) {
                    dialog_message_set_text(
                        message,
                        "All GPIO's are busy",
                        (128 - icon_get_width(&I_confused_dolph_43x31)) / 2 +
                            icon_get_width(&I_confused_dolph_43x31),
                        36,
                        AlignCenter,
                        AlignCenter);
                    UNITEMP_DEBUG("Unable to add a sensor. All GPIOs are busy");
                } else if(model->interface == &unitemp_i2c) {
                    dialog_message_set_text(
                        message,
                        "GPIO's 15 or 16\nare busy",
                        (128 - icon_get_width(&I_confused_dolph_43x31)) / 2 +
                            icon_get_width(&I_confused_dolph_43x31),
                        36,
                        AlignCenter,
                        AlignCenter);
                    UNITEMP_DEBUG("Unable to add a sensor. GPIOs 15 or 16 are busy");
                } else if(model->interface == &unitemp_spi) {
                    dialog_message_set_text(
                        message,
                        "GPIO's 1, 2 or 4\n are busy or there \nare no available pin\nfor CS wire",
                        (128 - icon_get_width(&I_confused_dolph_43x31)) / 2 +
                            icon_get_width(&I_confused_dolph_43x31),
                        36,
                        AlignCenter,
                        AlignCenter);
                    UNITEMP_DEBUG(
                        "Unable to add a sensor. GPIOs 1, 2 or 4 are busy or there are no available pins for CS");
                }

                dialog_message_show(app->dialogs, message);
                dialog_message_free(message);

                scene_manager_previous_scene(app->scene_manager);
                break;
            }
            //Counting available sensors of this type
            uint8_t sensor_current_model_count = 0;
            for(uint8_t i = 0; i < unitemp_sensors_get_count(); i++) {
                if(unitemp_sensors_get(i)->model == model) {
                    sensor_current_model_count++;
                }
            }
            //Allocating memory for a name
            char* name = malloc(11);
            if(name == NULL) {
                FURI_LOG_E(APP_NAME, "Sensor %s name allocation error", model->modelname);
                break;
            }
            //Adding a counter to the name if such a sensor exists
            if(sensor_current_model_count == 0)
                snprintf(name, 11, "%s%c", model->modelname, 0);
            else
                snprintf(name, 11, "%s_%d%c", model->modelname, sensor_current_model_count, 0);

            char* args = malloc(21);
            //Selecting the first available port for single wire and SPI sensor
            if(model->interface == &unitemp_singlewire || model->interface == &unitemp_spi) {
                snprintf(
                    args, 4, "%d", unitemp_gpio_get_aviable_pin(model->interface, 0, NULL)->num);
            }
            //Selecting the first available port for the one wire sensor and writing a zero ID
            if(model->interface == &unitemp_1w) {
                snprintf(
                    args,
                    21,
                    "%d %02X%02X%02X%02X%02X%02X%02X%02X",
                    unitemp_gpio_get_aviable_pin(model->interface, 0, NULL)->num,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0);
            }
            //For I2C the address will be selected automatically

            app->editable_sensor = unitemp_sensor_alloc(name, model, args);
            free(name);
            free(args);

            if(app->editable_sensor != NULL) {
                scene_manager_next_scene(app->scene_manager, UnitempSceneSensorEdit);
            } else {
                FURI_LOG_E(APP_NAME, "Failed to declare sensor for editing");
            }
        } while(0);
        consumed = true;
    }

    return consumed;
}

void unitemp_scene_sensors_list_on_exit(void* context) {
    UnitempApp* app = context;
    submenu_reset(app->submenu);
    submenu_set_selected_item(app->submenu, 0);
}
