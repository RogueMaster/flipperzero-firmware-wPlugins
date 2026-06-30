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
#include "view_single_sensor.h"
#include "../unitemp.h"
#include "../helpers/unitemp_draw.h"
#include "../helpers/unitemp_utils.h"
#include "../interfaces/singlewire_sensor.h"
#include "../interfaces/i2c_sensor.h"
#include "../interfaces/spi_sensor.h"
#include "../interfaces/onewire_sensor.h"

#include <stdlib.h>
#include <gui/elements.h>
#include <locale/locale.h>

#include <assets_icons.h>
#include "unitemp_icons.h"

#define TEMP_STR_SIZE 32
static char* temp_str;

#define UT_DATA_POS_CENTER      37, 23
#define UT_DATA_POS_UP_LEFT     9, 16
#define UT_DATA_POS_UP_MIDDLE   37, 16
#define UT_DATA_POS_UP_RIGHT    65, 16
#define UT_DATA_POS_DOWN_LEFT   9, 39
#define UT_DATA_POS_DOWN_MIDDLE 37, 39
#define UT_DATA_POS_DOWN_RIGHT  65, 39
#define UT_DATA_POS_NONE        255, 255

//Массив содержит в себе сколько элементов в себе содержит то или иное отображение UT_DATA_TYPE
static const uint8_t data_types_values_count[UT_DATA_TYPE_COUNT] = {
    1, //UT_DATA_TYPE_TEMP
    2, //UT_DATA_TYPE_TEMP_HUM
    2, //UT_DATA_TYPE_TEMP_PRESS
    3, //UT_DATA_TYPE_TEMP_HUM_PRESS
    3, //UT_DATA_TYPE_TEMP_HUM_CO2
};
//Массив содержит координаты для отображения одного, двух и более элементов
static const uint8_t values_positions[4][4][2] = {
    {{UT_DATA_POS_CENTER}},
    {{UT_DATA_POS_UP_MIDDLE}, {UT_DATA_POS_DOWN_MIDDLE}},
    {{UT_DATA_POS_UP_LEFT}, {UT_DATA_POS_UP_RIGHT}, {UT_DATA_POS_DOWN_MIDDLE}},
    {{UT_DATA_POS_UP_LEFT},
     {UT_DATA_POS_UP_RIGHT},
     {UT_DATA_POS_DOWN_LEFT},
     {UT_DATA_POS_DOWN_RIGHT}},
};

static void _draw_sensor_not_responding(Canvas* canvas, Sensor* sensor) {
    const Icon* frames[] = {
        &I_flipper_happy_60x38, &I_flipper_happy_2_60x38, &I_flipper_sad_60x38};
    canvas_draw_icon(canvas, 34, 23, frames[furi_get_tick() % 2250 / 750]);

    canvas_set_font(canvas, FontSecondary);

    if(sensor->model->interface == &unitemp_singlewire) {
        snprintf(
            temp_str,
            TEMP_STR_SIZE,
            "Sensor waiting on %s",
            ((SingleWireSensor*)sensor->instance)->data_pin->name);
    } else if(sensor->model->interface == &unitemp_i2c) {
        snprintf(temp_str, TEMP_STR_SIZE, "Sensor waiting on SDA & SCL");
    } else if(sensor->model->interface == &unitemp_spi) {
        snprintf(temp_str, TEMP_STR_SIZE, "Sensor waiting on SPI pins");
    } else if(sensor->model->interface == &unitemp_1w) {
        snprintf(
            temp_str,
            TEMP_STR_SIZE,
            "Sensor waiting on %s",
            ((OneWireSensor*)sensor->instance)->bus->bus_pin->name);
    }

    canvas_draw_str_aligned(canvas, 65, 19, AlignCenter, AlignCenter, temp_str);
}

static void _draw_sensor_polling(Canvas* canvas, Sensor* sensor) {
    UNUSED(sensor);
    canvas_draw_icon(canvas, 34, 23, &I_flipper_happy_60x38);

    canvas_set_font(canvas, FontSecondary);

    canvas_draw_str_aligned(canvas, 65, 19, AlignCenter, AlignCenter, "Reading values...");
}

void single_sensor_draw_sensor(Canvas* canvas, Sensor* sensor, SingleSensorViewModel* view_model) {
    UnitempSettings* settings = ((UnitempApp*)(view_model->context))->settings;

    if(sensor == NULL) return;

    //Drawing a frame
    canvas_draw_rframe(canvas, 0, 0, 128, 63, 7);
    canvas_draw_rframe(canvas, 0, 0, 128, 64, 7);

    //Right arrow
    if(unitemp_sensors_get_count() > 0 &&
       view_model->sensor_index < unitemp_sensors_get_count() - 1) {
        canvas_draw_icon(canvas, 122, 29, &I_ButtonRight_4x7);
    }
    //Left arrow
    if(view_model->sensor_index > 0) {
        canvas_draw_icon(canvas, 2, 29, &I_ButtonLeft_4x7);
    }

    //Name stamp
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 7, AlignCenter, AlignCenter, sensor->name);
    //Underscore
    uint8_t line_len = canvas_string_width(canvas, sensor->name) + 2;
    canvas_draw_line(canvas, 64 - line_len / 2, 12, 64 + line_len / 2, 12);

    SensorDataType data_type = sensor->model->data_type;

    if(sensor->status == UT_SENSORSTATUS_OK ||
       (sensor->status == UT_SENSORSTATUS_POLLING && sensor->temperature != -128.0f)) {
        uint8_t values_count_index = data_types_values_count[data_type] - 1;
        switch(data_type) {
        case UT_DATA_TYPE_TEMP:
            unitemp_draw_temperature(
                canvas,
                sensor,
                settings->temperature_unit,
                values_positions[values_count_index][0][0],
                values_positions[values_count_index][0][1]);
            break;
        case UT_DATA_TYPE_TEMP_HUM:
            values_count_index += (settings->heat_index ? 1 : 0);
            unitemp_draw_temperature(
                canvas,
                sensor,
                settings->temperature_unit,
                values_positions[values_count_index][0][0],
                values_positions[values_count_index][0][1]);
            unitemp_draw_humidity(
                canvas,
                sensor,
                settings->humidity_unit,
                settings->temperature_unit,
                values_positions[values_count_index][settings->heat_index ? 2 : 1][0],
                values_positions[values_count_index][settings->heat_index ? 2 : 1][1]);
            if(settings->heat_index) {
                unitemp_draw_heat_index(
                    canvas,
                    sensor,
                    settings->temperature_unit,
                    values_positions[values_count_index][1][0],
                    values_positions[values_count_index][1][1]);
            }
            break;
        case UT_DATA_TYPE_TEMP_PRESS:
            unitemp_draw_temperature(
                canvas,
                sensor,
                settings->temperature_unit,
                values_positions[values_count_index][0][0],
                values_positions[values_count_index][0][1]);
            unitemp_draw_pressure(
                canvas,
                sensor,
                settings->pressure_unit,
                values_positions[values_count_index][1][0] - 11,
                values_positions[values_count_index][1][1],
                false);
            break;
        case UT_DATA_TYPE_TEMP_HUM_PRESS:
            values_count_index += (settings->heat_index ? 1 : 0);
            unitemp_draw_temperature(
                canvas,
                sensor,
                settings->temperature_unit,
                values_positions[values_count_index][0][0],
                values_positions[values_count_index][0][1]);
            unitemp_draw_humidity(
                canvas,
                sensor,
                settings->humidity_unit,
                settings->temperature_unit,
                values_positions[values_count_index][settings->heat_index ? 3 : 1][0],
                values_positions[values_count_index][settings->heat_index ? 3 : 1][1]);
            unitemp_draw_pressure(
                canvas,
                sensor,
                settings->pressure_unit,
                values_positions[values_count_index][2][0] - (settings->heat_index ? 0 : 11),
                values_positions[values_count_index][2][1],
                settings->heat_index);
            if(settings->heat_index) {
                unitemp_draw_heat_index(
                    canvas,
                    sensor,
                    settings->temperature_unit,
                    values_positions[values_count_index][1][0],
                    values_positions[values_count_index][1][1]);
            }
            break;
        case UT_DATA_TYPE_TEMP_HUM_CO2:
            values_count_index += (settings->heat_index ? 1 : 0);
            unitemp_draw_temperature(
                canvas,
                sensor,
                settings->temperature_unit,
                values_positions[values_count_index][0][0],
                values_positions[values_count_index][0][1]);
            unitemp_draw_humidity(
                canvas,
                sensor,
                settings->humidity_unit,
                settings->temperature_unit,
                values_positions[values_count_index][settings->heat_index ? 3 : 1][0],
                values_positions[values_count_index][settings->heat_index ? 3 : 1][1]);
            unitemp_draw_co2(
                canvas,
                sensor,
                (settings->heat_index ? values_positions[values_count_index][2][0] : 22),
                values_positions[values_count_index][2][1],
                ColorWhite,
                settings->heat_index);
            if(settings->heat_index) {
                unitemp_draw_heat_index(
                    canvas,
                    sensor,
                    settings->temperature_unit,
                    values_positions[values_count_index][1][0],
                    values_positions[values_count_index][1][1]);
            }
            break;
        default:
            FURI_LOG_E(APP_NAME, "Unknown data type %d", sensor->model->data_type);
        }
    } else {
        if((sensor->status == UT_SENSORSTATUS_POLLING && sensor->temperature == -128.0f) ||
           (sensor->status == UT_SENSORSTATUS_INITIALIZED)) {
            _draw_sensor_polling(canvas, sensor);
        } else {
            _draw_sensor_not_responding(canvas, sensor);
        }
    }
}

static void single_sensor_draw_callback(Canvas* canvas, void* model) {
    SingleSensorViewModel* view_model = model;

    if(view_model->sensor_index > unitemp_sensors_get_count() - 1) {
        view_model->sensor_index = unitemp_sensors_get_count() - 1;
    }

    Sensor* sensor = unitemp_sensors_get(view_model->sensor_index);
    single_sensor_draw_sensor(canvas, sensor, view_model);
}

static bool single_sensor_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    SingleSensor* single_sensor = context;
    UnitempApp* app = single_sensor->context;
    bool consumed = false;

    if(event->key == InputKeyOk && event->type == InputTypeShort) {
        with_view_model(
            single_sensor->view,
            SingleSensorViewModel * model,
            {
                Sensor* sensor = unitemp_sensors_get(model->sensor_index);
                app->editable_sensor = sensor;
            },
            false);

        scene_manager_next_scene(app->scene_manager, UnitempSceneSensorMenu);
        consumed = true;
    } else if(event->key == InputKeyOk && event->type == InputTypeLong) {
        if(++app->settings->temperature_unit >= UT_TEMP_COUNT) app->settings->temperature_unit = 0;
        consumed = true;
    } else if(event->key == InputKeyLeft && event->type == InputTypeShort) {
        with_view_model(
            single_sensor->view,
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
            single_sensor->view,
            SingleSensorViewModel * model,
            {
                if(++model->sensor_index >= unitemp_sensors_get_count()) {
                    model->sensor_index = 0;
                }
            },
            true);
        consumed = true;
    } else if(event->key == InputKeyUp && event->type == InputTypeShort) {
        if(unitemp_sensors_get_count() > 1) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, CustomEventSwitchToTempOverviewView);
        }

        consumed = true;
    } else if(event->key == InputKeyDown && event->type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, CustomEventSwitchToSensorInfoView);
        consumed = true;
    }

    return consumed;
}

SingleSensor* single_sensor_alloc(void* context) {
    UnitempApp* app = context;
    SingleSensor* single_sensor = malloc(sizeof(SingleSensor));
    temp_str = malloc(TEMP_STR_SIZE);

    single_sensor->view = view_alloc();
    single_sensor->context = app;
    view_allocate_model(single_sensor->view, ViewModelTypeLockFree, sizeof(SingleSensorViewModel));

    with_view_model(
        single_sensor->view,
        SingleSensorViewModel * model,
        {
            model->sensor_index = 0;
            model->context = app;
        },
        false);

    view_set_context(single_sensor->view, single_sensor);
    view_set_draw_callback(single_sensor->view, single_sensor_draw_callback);
    view_set_input_callback(single_sensor->view, single_sensor_input_callback);

    return single_sensor;
}

void single_sensor_free(SingleSensor* single_sensor) {
    furi_assert(single_sensor);
    view_free_model(single_sensor->view);
    view_free(single_sensor->view);
    free(temp_str);
    free(single_sensor);
}

View* single_sensor_get_view(SingleSensor* single_sensor) {
    furi_assert(single_sensor);
    return single_sensor->view;
}

void single_sensor_refresh_data(SingleSensor* instance) {
    furi_assert(instance);

    //Проверяем корректность индекса и перерисовываем экран обновлением модели
    with_view_model(
        instance->view,
        SingleSensorViewModel * model,
        {
            if(model->sensor_index > unitemp_sensors_get_count() - 1) {
                model->sensor_index = unitemp_sensors_get_count() - 1;
            }

            EnvironmentState environment_state =
                unitemp_determine_environment_state(unitemp_sensors_get(model->sensor_index));

            UnitempApp* app = model->context;
            NotificationApp* notification_app = app->notifications;

            if(environment_state == EnvironmentStateDangerous) {
                if(app->settings->infinity_backlight) {
                    notification_message(
                        app->notifications, &sequence_display_backlight_enforce_auto);
                }
            }
            unitemp_display_environment_state(
                notification_app,
                environment_state,
                app->settings->environment_state_led_indication,
                true);

            if(environment_state == EnvironmentStateDangerous) {
                if(app->settings->infinity_backlight) {
                    notification_message(
                        app->notifications, &sequence_display_backlight_enforce_on);
                }
            }
        },
        true);
}
