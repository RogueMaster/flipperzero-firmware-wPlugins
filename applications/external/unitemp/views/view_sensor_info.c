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
#include "view_sensor_info.h"
#include "../unitemp.h"

#include <gui/elements.h>
#include "view_single_sensor.h"

#include "../interfaces/singlewire_sensor.h"
#include "../interfaces/i2c_sensor.h"
#include "../interfaces/spi_sensor.h"
#include "../interfaces/onewire_sensor.h"

extern const Icon I_ButtonRight_4x7;
extern const Icon I_ButtonLeft_4x7;

struct SensorInfo {
    View* view;
    void* context;
};

typedef struct {
    void* context;
} SensorInfoViewModel;

static void sensor_info_draw_callback(Canvas* canvas, void* model) {
    furi_assert(model);

    SensorInfoViewModel* view_model = model;
    UnitempApp* app = view_model->context;

    uint8_t sensor_index;
    Sensor* sensor;

    with_view_model(
        single_sensor_get_view(app->single_sensor),
        SingleSensorViewModel * m,
        {
            if(m->sensor_index > unitemp_sensors_get_count() - 1) {
                m->sensor_index = unitemp_sensors_get_count() - 1;
            }
            sensor_index = m->sensor_index;
        },
        false);
    sensor = unitemp_sensors_get(sensor_index);

    //Drawing a frame
    canvas_draw_rframe(canvas, 0, 0, 128, 63, 7);
    canvas_draw_rframe(canvas, 0, 0, 128, 64, 7);

    //Right arrow
    if(unitemp_sensors_get_count() > 0 && sensor_index < unitemp_sensors_get_count() - 1) {
        canvas_draw_icon(canvas, 122, 29, &I_ButtonRight_4x7);
    }
    //Left arrow
    if(sensor_index > 0) {
        canvas_draw_icon(canvas, 2, 29, &I_ButtonLeft_4x7);
    }
    //Name stamp
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 7, AlignCenter, AlignCenter, sensor->name);
    //Underscore
    uint8_t line_len = canvas_string_width(canvas, sensor->name) + 2;
    canvas_draw_line(canvas, 64 - line_len / 2, 12, 64 + line_len / 2, 12);

    FuriString* temp_str = furi_string_alloc();

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 10, 23, "Model:");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 48, 23, sensor->model->modelname);
    canvas_set_font(canvas, FontPrimary);
    if(sensor->model->interface == &unitemp_singlewire) {
        SingleWireSensor* s = sensor->instance;
        canvas_draw_str(canvas, 10, 34, "Data pin: ");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 57, 34, s->data_pin->name);
    } else if(sensor->model->interface == &unitemp_i2c) {
        I2CSensor* s = sensor->instance;
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 10, 34, "I2C address:");
        canvas_draw_str(canvas, 10, 45, "SDA pin:");
        canvas_draw_str(canvas, 10, 56, "SCL pin:");
        canvas_set_font(canvas, FontSecondary);
        furi_string_printf(temp_str, "0x%02X", s->current_i2c_adress >> 1);
        canvas_draw_str(canvas, 76, 34, furi_string_get_cstr(temp_str));
        canvas_draw_str(canvas, 56, 45, unitemp_gpio_get_from_int(15)->name);
        canvas_draw_str(canvas, 55, 56, unitemp_gpio_get_from_int(16)->name);
    } else if(sensor->model->interface == &unitemp_spi) {
        SPISensor* s = sensor->instance;
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 10, 34, "MISO pin:");
        canvas_draw_str(canvas, 10, 45, "SCK pin:");
        canvas_draw_str(canvas, 10, 56, "CS pin:");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 62, 34, unitemp_gpio_get_from_int(3)->name);
        canvas_draw_str(canvas, 56, 45, unitemp_gpio_get_from_int(5)->name);
        canvas_draw_str(canvas, 49, 56, s->cs_pin->name);
    } else if(sensor->model->interface == &unitemp_1w) {
        OneWireSensor* s = sensor->instance;
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 10, 34, "Bus pin:");
        canvas_draw_str(canvas, 10, 45, "ID:");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 53, 34, s->bus->bus_pin->name);
        furi_string_printf(
            temp_str,
            "%02X%02X%02X%02X%02X%02X%02X%02X",
            s->deviceID[0],
            s->deviceID[1],
            s->deviceID[2],
            s->deviceID[3],
            s->deviceID[4],
            s->deviceID[5],
            s->deviceID[6],
            s->deviceID[7]);
        canvas_draw_str(canvas, 26, 45, furi_string_get_cstr(temp_str));
        canvas_draw_str(canvas, 74, 23, unitemp_onewire_sensor_get_fc_name(sensor));
    }
    furi_string_free(temp_str);
}

static bool sensor_info_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    SensorInfo* sensor_info = context;
    UnitempApp* app = sensor_info->context;
    bool consumed = false;

    if(event->key == InputKeyOk && event->type == InputTypeShort) {
        with_view_model(
            single_sensor_get_view(app->single_sensor),
            SingleSensorViewModel * model,
            {
                Sensor* sensor = unitemp_sensors_get(model->sensor_index);
                app->editable_sensor = sensor;
            },
            false);

        scene_manager_next_scene(app->scene_manager, UnitempSceneSensorMenu);
        consumed = true;
    } else if(event->key == InputKeyUp && event->type == InputTypeShort) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, CustomEventSwitchToSingleSensorView);
        consumed = true;
    } else if(event->key == InputKeyLeft && event->type == InputTypeShort) {
        with_view_model(
            single_sensor_get_view(app->single_sensor),
            SingleSensorViewModel * model,
            {
                if(--model->sensor_index >= unitemp_sensors_get_count()) {
                    model->sensor_index = unitemp_sensors_get_count() - 1;
                }
            },
            true);
        consumed = true;
    } else if(event->key == InputKeyRight && event->type == InputTypeShort) {
        with_view_model(
            single_sensor_get_view(app->single_sensor),
            SingleSensorViewModel * model,
            {
                if(++model->sensor_index >= unitemp_sensors_get_count()) {
                    model->sensor_index = 0;
                }
            },
            true);
        consumed = true;
    }
    with_view_model(app->sensor_info->view, SensorInfoViewModel * model, { UNUSED(model); }, true);
    return consumed;
}

static uint32_t sensor_info_previous_callback(void* context) {
    furi_assert(context);
    SensorInfo* sensor_info = context;
    UnitempApp* app = sensor_info->context;
    view_dispatcher_send_custom_event(app->view_dispatcher, CustomEventSwitchToSingleSensorView);
    return UnitempViewSensorInfo;
}

SensorInfo* sensor_info_alloc(void* context) {
    UnitempApp* app = context;
    SensorInfo* sensor_info = malloc(sizeof(SensorInfo));

    sensor_info->view = view_alloc();
    sensor_info->context = app;

    view_allocate_model(sensor_info->view, ViewModelTypeLockFree, sizeof(SensorInfoViewModel));

    with_view_model(
        sensor_info->view, SensorInfoViewModel * model, { model->context = app; }, false);

    view_set_context(sensor_info->view, sensor_info);
    view_set_draw_callback(sensor_info->view, sensor_info_draw_callback);
    view_set_input_callback(sensor_info->view, sensor_info_input_callback);
    view_set_previous_callback(sensor_info->view, sensor_info_previous_callback);

    return sensor_info;
}

void sensor_info_free(SensorInfo* sensor_info) {
    furi_assert(sensor_info);
    view_free_model(sensor_info->view);
    view_free(sensor_info->view);
    free(sensor_info);
}

View* sensor_info_get_view(SensorInfo* sensor_info) {
    furi_assert(sensor_info);
    return sensor_info->view;
}
