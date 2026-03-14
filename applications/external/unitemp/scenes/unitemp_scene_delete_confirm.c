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

#include "unitemp.h"
#include "interfaces/i2c_sensor.h"
#include "interfaces/onewire_sensor.h"
#include "interfaces/singlewire_sensor.h"
#include "interfaces/spi_sensor.h"
#include "unitemp_icons.h"

void unitemp_scene_delete_confirm_on_enter(void* context) {
    UnitempApp* app = context;
    Sensor* sensor = app->editable_sensor;
    Widget* widget = app->widget;

    FuriString* tmp = furi_string_alloc();

    widget_reset(app->widget);

    //Adding buttons
    widget_add_button_element(widget, GuiButtonTypeLeft, "Back", unitemp_widget_callback, context);
    widget_add_button_element(
        widget, GuiButtonTypeRight, "Delete", unitemp_widget_callback, context);

    furi_string_printf(tmp, "\e#Delete %s?\e#\n", sensor->name);
    widget_add_text_box_element(
        app->widget, 0, 0, 128, 23, AlignCenter, AlignCenter, furi_string_get_cstr(tmp), false);

    if(sensor->model->interface == &unitemp_1w) {
        OneWireSensor* s = sensor->instance;

        furi_string_printf(tmp, "\e#Model:\e# %s", unitemp_onewire_sensor_get_fc_name(sensor));
        widget_add_text_box_element(
            app->widget, 0, 16, 128, 23, AlignLeft, AlignTop, furi_string_get_cstr(tmp), false);
        furi_string_printf(tmp, "\e#Data pin:\e# %s", s->bus->bus_pin->name);
        widget_add_text_box_element(
            app->widget, 0, 28, 128, 23, AlignLeft, AlignTop, furi_string_get_cstr(tmp), false);

        furi_string_printf(
            tmp,
            "\e#ID:\e# %02X%02X%02X%02X%02X%02X%02X%02X",
            s->deviceID[0],
            s->deviceID[1],
            s->deviceID[2],
            s->deviceID[3],
            s->deviceID[4],
            s->deviceID[5],
            s->deviceID[6],
            s->deviceID[7]);
        widget_add_text_box_element(
            app->widget, 0, 40, 128, 23, AlignLeft, AlignTop, furi_string_get_cstr(tmp), false);
    }

    if(sensor->model->interface == &unitemp_singlewire) {
        furi_string_printf(tmp, "\e#Model:\e# %s", sensor->model->modelname);
        widget_add_text_box_element(
            app->widget, 0, 16, 128, 23, AlignLeft, AlignTop, furi_string_get_cstr(tmp), false);
        furi_string_printf(
            tmp, "\e#Bus pin:\e# %s", ((SingleWireSensor*)sensor->instance)->data_pin->name);
        widget_add_text_box_element(
            app->widget, 0, 28, 128, 23, AlignLeft, AlignTop, furi_string_get_cstr(tmp), false);
    }
    if(sensor->model->interface == &unitemp_spi) {
        furi_string_printf(tmp, "\e#Model:\e# %s", sensor->model->modelname);
        widget_add_text_box_element(
            app->widget, 0, 16, 128, 23, AlignLeft, AlignTop, furi_string_get_cstr(tmp), false);
        furi_string_printf(tmp, "\e#CS pin:\e# %s", ((SPISensor*)sensor->instance)->cs_pin->name);
        widget_add_text_box_element(
            app->widget, 0, 28, 128, 23, AlignLeft, AlignTop, furi_string_get_cstr(tmp), false);
    }

    if(sensor->model->interface == &unitemp_i2c) {
        furi_string_printf(tmp, "\e#Model:\e# %s", sensor->model->modelname);
        widget_add_text_box_element(
            app->widget, 0, 16, 128, 23, AlignLeft, AlignTop, furi_string_get_cstr(tmp), false);
        furi_string_printf(
            tmp,
            "\e#I2C addr:\e# 0x%02X",
            ((I2CSensor*)sensor->instance)->current_i2c_adress >> 1);
        widget_add_text_box_element(
            app->widget, 0, 28, 128, 23, AlignLeft, AlignTop, furi_string_get_cstr(tmp), false);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, UnitempViewWidget);
    furi_string_free(tmp);
}

bool unitemp_scene_delete_confirm_on_event(void* context, SceneManagerEvent event) {
    UnitempApp* app = context;
    SceneManager* scene_manager = app->scene_manager;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == GuiButtonTypeRight) {
            if(unitemp_sensor_delete(app->editable_sensor)) {
                unitemp_sensors_save(app);
                scene_manager_next_scene(scene_manager, UnitempSceneDeleteSuccess);
            } else {
                DialogMessage* message = dialog_message_alloc();
                dialog_message_set_header(
                    message, "An error occurred", 64, 6, AlignCenter, AlignCenter);
                dialog_message_set_text(
                    message,
                    "Please report\nthis to the\napp developers\ntiny.one/unitemp",
                    (128 - icon_get_width(&I_confused_dolph_43x31)) / 2 +
                        icon_get_width(&I_confused_dolph_43x31),
                    36,
                    AlignCenter,
                    AlignCenter);
                dialog_message_set_icon(
                    message,
                    &I_confused_dolph_43x31,
                    0,
                    64 - icon_get_height(&I_confused_dolph_43x31));
                dialog_message_show(app->dialogs, message);
                dialog_message_free(message);

                scene_manager_previous_scene(scene_manager);
            }
        } else if(event.event == GuiButtonTypeLeft) {
            scene_manager_previous_scene(scene_manager);
        }
    }

    return consumed;
}

void unitemp_scene_delete_confirm_on_exit(void* context) {
    UnitempApp* app = context;
    widget_reset(app->widget);
}
