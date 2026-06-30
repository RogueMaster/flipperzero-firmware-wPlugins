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
#include "unitemp_utils.h"
#include <locale/locale.h>

static EnvironmentState last_enviroment_state = EnvironmentStateUndefined;

static const NotificationMessage message_green_128 = {
    .type = NotificationMessageTypeLedGreen,
    .data.led.value = 128,
};
// static const NotificationMessage message_green_64 = {
//     .type = NotificationMessageTypeLedGreen,
//     .data.led.value = 64,
// };
// static const NotificationMessage message_red_128 = {
//     .type = NotificationMessageTypeLedRed,
//     .data.led.value = 128,
// };
static const NotificationMessage message_red_191 = {
    .type = NotificationMessageTypeLedRed,
    .data.led.value = 191,
};

const NotificationMessage message_blink_start_125 = {
    .type = NotificationMessageTypeLedBlinkStart,
    .data.led_blink.color = 0,
    .data.led_blink.on_time = 75,
    .data.led_blink.period = 150,
};

static const NotificationSequence EnvironmentStateUndefinedSequence = {
    &message_blink_stop,
    &message_red_0,
    &message_do_not_reset,
    &message_blue_0,
    &message_do_not_reset,
    &message_green_0,
    &message_do_not_reset,
    NULL,
};

static const NotificationSequence EnvironmentStateExcellentSequence = {
    &message_blink_stop,
    &message_red_0,
    &message_do_not_reset,
    &message_green_255,
    &message_do_not_reset,
    &message_blue_0,
    &message_do_not_reset,
    NULL,
};
static const NotificationSequence EnvironmentStateGoodSequence = {
    &message_blink_stop,
    &message_red_191,
    &message_do_not_reset,
    &message_green_255,
    &message_do_not_reset,
    &message_blue_0,
    &message_do_not_reset,
    NULL,
};
static const NotificationSequence EnvironmentStateModerateSequence = {
    &message_blink_stop,
    &message_red_255,
    &message_do_not_reset,
    &message_green_128,
    &message_do_not_reset,
    &message_blue_0,
    &message_do_not_reset,
    NULL,
};
static const NotificationSequence EnvironmentStatePoorSequence = {
    &message_blink_stop,
    &message_red_255,
    &message_do_not_reset,
    &message_green_0,
    &message_do_not_reset,
    &message_blue_0,
    &message_do_not_reset,
    NULL,
};

static const NotificationSequence EnvironmentStateDangerousSequence = {
    &message_blink_start_125,
    &message_do_not_reset,
    &message_blink_set_color_red,
    &message_do_not_reset,
    NULL};

static const NotificationSequence* notification_sequences[6] = {
    &EnvironmentStateUndefinedSequence,
    &EnvironmentStateExcellentSequence,
    &EnvironmentStateGoodSequence,
    &EnvironmentStateModerateSequence,
    &EnvironmentStatePoorSequence,
    &EnvironmentStateDangerousSequence};

float unitemp_calculate_dew_point(float temperature_in_celsius, float humidity_in_percent) {
    if(humidity_in_percent <= 0.0f || humidity_in_percent > 100.0f ||
       temperature_in_celsius < -40.0f) {
        return -128.0f;
    }

    float a = 17.27f;
    float b = 237.7f;
    float alpha = ((a * temperature_in_celsius) / (b + temperature_in_celsius)) +
                  logf(humidity_in_percent / 100.0f);
    float dew_point = (b * alpha) / (a - alpha);

    return dew_point;
}

float unitemp_calculate_heat_index(float temperature_in_fahrenheit, float humidity_in_percent) {
    if(humidity_in_percent <= 0.0f || humidity_in_percent > 100.0f ||
       temperature_in_fahrenheit < 70.0f || temperature_in_fahrenheit > 115.0f) {
        return temperature_in_fahrenheit;
    }

    double temp_f = (double)temperature_in_fahrenheit;
    double rh = (double)humidity_in_percent;
    const double c1 = 0.363445176;
    const double c2 = 0.988622465;
    const double c3 = 4.777114035;
    const double c4 = -0.114037667;
    const double c5 = -8.50208e-4;
    const double c6 = -2.0716198e-2;
    const double c7 = 6.87678e-4;
    const double c8 = 2.74954e-4;
    const double c9 = 0.0;

    double hi_f = c1 + c2 * temp_f + c3 * rh + c4 * temp_f * rh + c5 * temp_f * temp_f +
                  c6 * rh * rh + c7 * temp_f * temp_f * rh + c8 * temp_f * rh * rh +
                  c9 * temp_f * temp_f * rh * rh;

    return (float)hi_f;
}

float unitemp_convert_pa_to_mm_hg(float pressure_in_pa) {
    return pressure_in_pa * 0.007500638f;
}
float unitemp_convert_pa_to_in_hg(float pressure_in_pa) {
    return pressure_in_pa * 0.0002953007f;
}
float unitemp_convert_pa_to_kpa(float pressure_in_pa) {
    return pressure_in_pa / 1000.0f;
}
float unitemp_convert_pa_to_hpa(float pressure_in_pa) {
    return pressure_in_pa / 100.0f;
}

//https://en.wikipedia.org/wiki/Heat_index#Effects_of_the_heat_index_(shade_values)
EnvironmentState unitemp_determine_environment_state_from_hi(float heat_index_in_fahrenheit) {
    if(heat_index_in_fahrenheit < 80.0f) {
        return EnvironmentStateExcellent;
    } else if(heat_index_in_fahrenheit > 80.0f && heat_index_in_fahrenheit <= 90.0f) {
        return EnvironmentStateGood;
    } else if(heat_index_in_fahrenheit > 90.0f && heat_index_in_fahrenheit <= 106.0f) {
        return EnvironmentStateModerate;
    } else if(heat_index_in_fahrenheit > 106.0f && heat_index_in_fahrenheit <= 129.0f) {
        return EnvironmentStatePoor;
    } else if(heat_index_in_fahrenheit > 129.0f) {
        return EnvironmentStateDangerous;
    } else
        return EnvironmentStateUndefined;
}

EnvironmentState unitemp_determine_environment_state_from_co2(uint16_t ppm) {
    if(ppm <= 400.0f) {
        return EnvironmentStateExcellent;
    } else if(ppm > 400.0f && ppm <= 1000.0f) {
        return EnvironmentStateGood;
    } else if(ppm > 1000.0f && ppm <= 5000.0f) {
        return EnvironmentStateModerate;
    } else if(ppm > 5000.0f && ppm <= 15000.0f) {
        return EnvironmentStatePoor;
    } else if(ppm > 15000.0f) {
        return EnvironmentStateDangerous;
    } else
        return EnvironmentStateUndefined;
}

EnvironmentState unitemp_determine_environment_state(Sensor* sensor) {
    if(sensor->status != UT_SENSORSTATUS_OK) return EnvironmentStateUndefined;
    EnvironmentState gas_state = EnvironmentStateUndefined;
    EnvironmentState hi_state = EnvironmentStateUndefined;
    EnvironmentState result_state = EnvironmentStateUndefined;
    if(sensor->model->data_type == UT_DATA_TYPE_TEMP_HUM ||
       sensor->model->data_type == UT_DATA_TYPE_TEMP_HUM_PRESS ||
       sensor->model->data_type == UT_DATA_TYPE_TEMP_HUM_CO2) {
        hi_state = unitemp_determine_environment_state_from_hi(unitemp_calculate_heat_index(
            locale_celsius_to_fahrenheit(sensor->temperature), sensor->humidity));
    }
    if(sensor->model->data_type == UT_DATA_TYPE_TEMP_HUM_CO2) {
        gas_state = unitemp_determine_environment_state_from_co2(sensor->co2);
    }
    UNITEMP_DEBUG("gas state: %d, hi_state: %d", gas_state, hi_state);
    //Choosing the worst option
    if(gas_state > result_state) {
        result_state = gas_state;
    }
    if(hi_state > result_state) {
        result_state = hi_state;
    }
    return result_state;
}

void unitemp_display_environment_state(
    NotificationApp* app,
    EnvironmentState state,
    bool light,
    bool vibro_and_sound) {
    if(state == last_enviroment_state) {
        // if(state == EnvironmentStateDangerous && vibro_and_sound) {
        //     notification_message(app, &sequence_audiovisual_alert);
        // }
        return;
    }
    last_enviroment_state = state;

    UNITEMP_DEBUG("Environment state %d", state);

    if(light) {
        notification_message(app, notification_sequences[state]);
    }
    if(state == EnvironmentStateDangerous && vibro_and_sound) {
        notification_message(app, &sequence_audiovisual_alert);
    }
}
void unitemp_reset_environment_state(NotificationApp* app) {
    last_enviroment_state = EnvironmentStateUndefined;

    notification_message(app, &sequence_blink_stop);
    notification_message(app, &sequence_reset_rgb);
}
