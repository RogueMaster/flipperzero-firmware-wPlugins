#include "uv_meter_data.hpp"

#include <gui/elements.h>
#include "uv_meter_as7331_icons.h"

#include <locale/locale.h>
#include <math.h>

#define UV_METER_MAX_RAW_VALUE 65535.0

struct UVMeterData {
    View* view;
    AS7331* sensor;
    FuriMutex* sensor_mutex;
    UVMeterDataEnterSettingsCallback callback;
    void* context;
};

// Configuration mode states
typedef enum {
    UVMeterConfigModeNone, // No config selected
    UVMeterConfigModeGain, // Configure gain
    UVMeterConfigModeExposureTime, // Configure exposure time
    UVMeterConfigModeEyesProtection, // Configure eyes protection mode
} UVMeterConfigMode;

typedef struct {
    FuriString* buffer;
    UVMeterConfigMode current_config_mode;
    AS7331::Results results;
    AS7331::RawResults raw_results;
    UVMeterEffectiveResults effective_results;
    int16_t gain_value;
    double conversion_time;
    bool eyes_protected;
    UVMeterUnit unit;
} UVMeterDataModel;

static void uv_meter_data_draw_raw_meter(
    Canvas* canvas,
    int x,
    int y,
    uint16_t raw_value,
    double max_meter_value) {
    int meter_value = (int)lround((double)raw_value / max_meter_value * (double)8.0);

    // Draw the meter frame
    canvas_draw_frame(canvas, x, y, 3, 10);

    // Draw the meter level
    canvas_draw_line(canvas, x + 1, y + 9, x + 1, y + 9 - meter_value);

    // Show alert if value is too low or at max
    if(raw_value < 5 || raw_value >= max_meter_value) {
        canvas_draw_icon(canvas, x + 5, y + 1, &I_Alert_9x8);
    }
}

static void uv_meter_data_draw_uv_measurements(
    Canvas* canvas,
    const AS7331::Results* results,
    const AS7331::RawResults* raw_results,
    UVMeterUnit unit,
    FuriString* buffer) {
    canvas_set_font(canvas, FontSecondary);
    int x_1_align = 0;
    int y_uva_bottom = 9;
    int y_uvb_bottom = 23;
    int y_uvc_bottom = 37;

    // Draw labels
    canvas_draw_str_aligned(canvas, x_1_align, y_uva_bottom, AlignLeft, AlignBottom, "UVA:");
    canvas_draw_str_aligned(canvas, x_1_align, y_uvb_bottom, AlignLeft, AlignBottom, "UVB:");
    canvas_draw_str_aligned(canvas, x_1_align, y_uvc_bottom, AlignLeft, AlignBottom, "UVC:");

    // Display UV measurements
    canvas_set_font(canvas, FontPrimary);
    int x_2_align = 51;

    double multiplier = 1.0;
    switch(unit) {
    case UVMeterUnituW_cm_2:
        multiplier = 1.0;
        break;
    case UVMeterUnitW_m_2:
        multiplier = 0.01;
        break;
    case UVMeterUnitmW_m_2:
        multiplier = 10;
        break;
    }

    // Display UVA value
    double uv_a = results->uv_a * multiplier;
    furi_string_printf(buffer, "%.*f", (uv_a >= 1000 ? 0 : (uv_a >= 100 ? 1 : 2)), uv_a);
    canvas_draw_str_aligned(
        canvas, x_2_align, y_uva_bottom, AlignRight, AlignBottom, furi_string_get_cstr(buffer));

    // Display UVB value
    double uv_b = results->uv_b * multiplier;
    furi_string_printf(buffer, "%.*f", (uv_b >= 1000 ? 0 : (uv_b >= 100 ? 1 : 2)), uv_b);
    canvas_draw_str_aligned(
        canvas, x_2_align, y_uvb_bottom, AlignRight, AlignBottom, furi_string_get_cstr(buffer));

    // Display UVC value
    double uv_c = results->uv_c * multiplier;
    furi_string_printf(buffer, "%.*f", (uv_c >= 1000 ? 0 : (uv_c >= 100 ? 1 : 2)), uv_c);
    canvas_draw_str_aligned(
        canvas, x_2_align, y_uvc_bottom, AlignRight, AlignBottom, furi_string_get_cstr(buffer));

    // Draw raw meters with alerts
    int raw_meter_x = x_2_align + 3;

    // UVA raw meter
    uv_meter_data_draw_raw_meter(
        canvas, raw_meter_x, y_uva_bottom - 9, raw_results->uv_a, UV_METER_MAX_RAW_VALUE);

    // UVB raw meter
    uv_meter_data_draw_raw_meter(
        canvas, raw_meter_x, y_uvb_bottom - 9, raw_results->uv_b, UV_METER_MAX_RAW_VALUE);

    // UVC raw meter
    uv_meter_data_draw_raw_meter(
        canvas, raw_meter_x, y_uvc_bottom - 9, raw_results->uv_c, UV_METER_MAX_RAW_VALUE);
}

static void uv_meter_data_draw_uv_effectiveness(
    Canvas* canvas,
    const UVMeterEffectiveResults* effective_results,
    FuriString* buffer) {
    canvas_set_font(canvas, FontSecondary);
    int x_3_align = 94;
    int y_uva_bottom = 9;
    int y_uvb_bottom = 23;
    int y_uvc_bottom = 37;

    // Skip if total is zero to avoid division by zero
    if(effective_results->uv_total_eff <= 0) {
        return;
    }

    // UVA percentage of Maximum Daily Exposure Time
    furi_string_printf(
        buffer,
        "%d%%",
        (int)lround(effective_results->uv_a_eff / effective_results->uv_total_eff * 100));
    canvas_draw_str_aligned(
        canvas, x_3_align, y_uva_bottom, AlignRight, AlignBottom, furi_string_get_cstr(buffer));

    // UVB percentage of Maximum Daily Exposure Time
    furi_string_printf(
        buffer,
        "%d%%",
        (int)lround(effective_results->uv_b_eff / effective_results->uv_total_eff * 100));
    canvas_draw_str_aligned(
        canvas, x_3_align, y_uvb_bottom, AlignRight, AlignBottom, furi_string_get_cstr(buffer));

    // UVC percentage of Maximum Daily Exposure Time
    furi_string_printf(
        buffer,
        "%d%%",
        (int)lround(effective_results->uv_c_eff / effective_results->uv_total_eff * 100));
    canvas_draw_str_aligned(
        canvas, x_3_align, y_uvc_bottom, AlignRight, AlignBottom, furi_string_get_cstr(buffer));

    // Draw separator line and arrow
    int x_3_4 = 96;
    canvas_draw_line(canvas, x_3_4, y_uva_bottom - 9, x_3_4, y_uvc_bottom);
    canvas_draw_icon(canvas, x_3_4 + 1, y_uvb_bottom - 6, &I_ButtonRightSmall_3x5);
}

static void uv_meter_data_draw_maximum_daily_exposure_time(
    Canvas* canvas,
    double t_max,
    FuriString* buffer) {
    int x_center_4_4 = 112;
    int y_uvb_bottom = 23;

    // Draw sun icon
    canvas_draw_icon(canvas, x_center_4_4 - 7, -7, &I_Sun_15x16);

    // Draw "min" label
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, x_center_4_4, y_uvb_bottom + 2, AlignCenter, AlignTop, "min");

    // Draw max exposure time in minutes
    canvas_set_font(canvas, FontPrimary);
    double t_max_minutes = t_max / 60;
    furi_string_printf(
        buffer, "%.*f", (t_max_minutes >= 100 ? 0 : (t_max_minutes >= 10 ? 1 : 2)), t_max_minutes);
    canvas_draw_str_aligned(
        canvas, x_center_4_4, y_uvb_bottom, AlignCenter, AlignBottom, furi_string_get_cstr(buffer));
}

static void uv_meter_data_draw_config_section(
    Canvas* canvas,
    int16_t gain_value,
    double conversion_time,
    bool eyes_protected,
    UVMeterUnit unit,
    UVMeterConfigMode current_config_mode,
    FuriString* buffer) {
    // Draw unit
    const Icon* icon = NULL;
    switch(unit) {
    case UVMeterUnituW_cm_2:
        icon = &I_Unit_uW_cm2_34x11;
        break;
    case UVMeterUnitW_m_2:
        icon = &I_Unit_W_m2_22x11;
        break;
    case UVMeterUnitmW_m_2:
        icon = &I_Unit_mW_m2_28x11;
        break;
    }
    int x_1_align = 0;
    int y_conf = 52;
    canvas_draw_icon(canvas, x_1_align, y_conf - 11, icon);

    // Draw text explaining selected setting
    canvas_set_font(canvas, FontSecondary);
    int x_setting_right = 123;

    const char* setting_string;
    switch(current_config_mode) {
    case UVMeterConfigModeGain:
        setting_string = "Gain";
        break;
    case UVMeterConfigModeExposureTime:
        setting_string = "Exposure Time (s)";
        break;
    case UVMeterConfigModeEyesProtection:
        setting_string = "Eyes Protected";
        break;
    default:
        setting_string = "";
        break;
    }

    uint16_t text_width = canvas_string_width(canvas, setting_string);
    canvas_draw_str_aligned(
        canvas, x_setting_right, y_conf - 2, AlignRight, AlignBottom, setting_string);

    // Navigation arrows
    canvas_draw_icon(canvas, x_setting_right + 2, y_conf - 8, &I_ButtonRightSmall_3x5);
    canvas_draw_icon(canvas, x_setting_right - text_width - 5, y_conf - 8, &I_ButtonLeftSmall_3x5);

    // Frames/Boxes for settings
    int setting_x_size = 33;
    int setting_y_size = 14;

    // Config button box
    canvas_draw_rframe(canvas, 0, y_conf, setting_x_size, setting_y_size, 3);

    // Gain box
    if(current_config_mode == UVMeterConfigModeGain) {
        canvas_draw_rbox(canvas, setting_x_size - 1, y_conf, setting_x_size, setting_y_size, 3);
    } else {
        canvas_draw_rframe(canvas, setting_x_size - 1, y_conf, setting_x_size, setting_y_size, 3);
    }

    // Exposure Time box
    if(current_config_mode == UVMeterConfigModeExposureTime) {
        canvas_draw_rbox(
            canvas, setting_x_size * 2 - 2, y_conf, setting_x_size, setting_y_size, 3);
    } else {
        canvas_draw_rframe(
            canvas, setting_x_size * 2 - 2, y_conf, setting_x_size, setting_y_size, 3);
    }

    // Eyes Protection box
    if(current_config_mode == UVMeterConfigModeEyesProtection) {
        canvas_draw_rbox(
            canvas, setting_x_size * 3 - 3, y_conf, setting_x_size - 1, setting_y_size, 3);
    } else {
        canvas_draw_rframe(
            canvas, setting_x_size * 3 - 3, y_conf, setting_x_size - 1, setting_y_size, 3);
    }

    // Config button
    canvas_draw_icon(canvas, 2, y_conf + 3, &I_ButtonCenter_7x7);
    canvas_draw_str_aligned(canvas, 11, y_conf + 3, AlignLeft, AlignTop, "Conf");

    // Gain value
    if(current_config_mode == UVMeterConfigModeGain) {
        canvas_set_color(canvas, ColorWhite);
    }
    furi_string_printf(buffer, "%d", gain_value);
    canvas_draw_str_aligned(
        canvas,
        setting_x_size - 1 + (setting_x_size / 2),
        y_conf + 3,
        AlignCenter,
        AlignTop,
        furi_string_get_cstr(buffer));
    canvas_set_color(canvas, ColorBlack);

    // Exposure Time value
    if(current_config_mode == UVMeterConfigModeExposureTime) {
        canvas_set_color(canvas, ColorWhite);
    }
    furi_string_printf(
        buffer,
        "%.*f",
        (conversion_time >= 100 ? 1 : (conversion_time >= 10 ? 2 : 3)),
        conversion_time);
    canvas_draw_str_aligned(
        canvas,
        setting_x_size * 2 - 2 + (setting_x_size / 2),
        y_conf + 3,
        AlignCenter,
        AlignTop,
        furi_string_get_cstr(buffer));
    canvas_set_color(canvas, ColorBlack);

    // Eyes Protection
    if(current_config_mode == UVMeterConfigModeEyesProtection) {
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_icon(
        canvas,
        setting_x_size * 3 - 3 + 4,
        y_conf + 2,
        (eyes_protected) ? &I_Sunglasses_24x8 : &I_Glasses_24x8);
    canvas_set_color(canvas, ColorBlack);
}

static void uv_meter_data_draw_callback(Canvas* canvas, void* model) {
    auto* m = static_cast<UVMeterDataModel*>(model);
    FURI_LOG_D("UV_Meter Data", "Redrawing");

    // Draw UV measurements section
    uv_meter_data_draw_uv_measurements(canvas, &m->results, &m->raw_results, m->unit, m->buffer);

    // Draw UV effectiveness percentages
    uv_meter_data_draw_uv_effectiveness(canvas, &m->effective_results, m->buffer);

    // Draw maximum daily exposure time
    uv_meter_data_draw_maximum_daily_exposure_time(canvas, m->effective_results.t_max, m->buffer);

    // Draw configuration section
    uv_meter_data_draw_config_section(
        canvas,
        m->gain_value,
        m->conversion_time,
        m->eyes_protected,
        m->unit,
        m->current_config_mode,
        m->buffer);
}

static bool uv_meter_data_input_callback(InputEvent* event, void* context) {
    auto* instance = static_cast<UVMeterData*>(context);

    bool consumed = false;
    bool setting_changed = false;

    if(event->type != InputTypeShort) {
        return false;
    }

    if(event->key == InputKeyOk) {
        if(instance->callback) {
            instance->callback(instance->context);
            return true;
        }
        return false;
    }
    FURI_LOG_D("UV_Meter Data", "Update Input");
    with_view_model_cpp(
        instance->view,
        UVMeterDataModel*,
        model,
        {
            // Handle left/right to select config
            if(event->key == InputKeyLeft || event->key == InputKeyRight) {
                if(event->key == InputKeyLeft) {
                    // Move to previous config mode
                    switch(model->current_config_mode) {
                    case UVMeterConfigModeNone:
                    case UVMeterConfigModeGain:
                        model->current_config_mode = UVMeterConfigModeEyesProtection;
                        break;
                    case UVMeterConfigModeExposureTime:
                        model->current_config_mode = UVMeterConfigModeGain;
                        break;
                    case UVMeterConfigModeEyesProtection:
                        model->current_config_mode = UVMeterConfigModeExposureTime;
                        break;
                    }
                } else { // InputKeyRight
                    // Move to next config mode
                    switch(model->current_config_mode) {
                    case UVMeterConfigModeNone:
                    case UVMeterConfigModeGain:
                        model->current_config_mode = UVMeterConfigModeExposureTime;
                        break;
                    case UVMeterConfigModeExposureTime:
                        model->current_config_mode = UVMeterConfigModeEyesProtection;
                        break;
                    case UVMeterConfigModeEyesProtection:
                        model->current_config_mode = UVMeterConfigModeGain;
                        break;
                    }
                }
                consumed = true;
                setting_changed = true;
            }

            // Handle up/down to change selected config mode
            else if(
                instance->sensor && model->current_config_mode != UVMeterConfigModeNone &&
                (event->key == InputKeyUp || event->key == InputKeyDown)) {
                furi_mutex_acquire(instance->sensor_mutex, FuriWaitForever);

                switch(model->current_config_mode) {
                case UVMeterConfigModeGain: {
                    // Adjust gain
                    int current_gain = static_cast<int>(instance->sensor->getGain());
                    int new_gain = (event->key == InputKeyUp) ? current_gain - 1 :
                                                                current_gain + 1;

                    if(new_gain >= static_cast<int>(GAIN_2048) &&
                       new_gain <= static_cast<int>(GAIN_1)) {
                        instance->sensor->setGain(static_cast<as7331_gain_t>(new_gain));
                        model->gain_value = instance->sensor->getGainValue();
                        setting_changed = true;
                    }
                    break;
                }

                case UVMeterConfigModeExposureTime: {
                    // Adjust integration time
                    int current_time = static_cast<int>(instance->sensor->getIntegrationTime());
                    int new_time = (event->key == InputKeyUp) ? current_time + 1 :
                                                                current_time - 1;

                    if(new_time >= static_cast<int>(TIME_1MS) &&
                       new_time <= static_cast<int>(TIME_16384MS)) {
                        instance->sensor->setIntegrationTime(
                            static_cast<as7331_integration_time_t>(new_time));
                        model->conversion_time = instance->sensor->getConversionTime();
                        setting_changed = true;
                    }
                    break;
                }

                case UVMeterConfigModeEyesProtection:
                    // Toggle eyes protected
                    model->eyes_protected = !model->eyes_protected;
                    model->effective_results = uv_meter_data_calculate_effective_results(
                        &model->results, model->eyes_protected);
                    setting_changed = true;
                    break;

                default:
                    break;
                }
                furi_mutex_release(instance->sensor_mutex);
                consumed = true;
            }
        },
        setting_changed);

    return consumed;
}

UVMeterData* uv_meter_data_alloc(void) {
    UVMeterData* instance = new UVMeterData();
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(UVMeterDataModel));
    view_set_draw_callback(instance->view, uv_meter_data_draw_callback);
    view_set_input_callback(instance->view, uv_meter_data_input_callback);
    view_set_context(instance->view, instance);

    FURI_LOG_D("UV_Meter Data", "Update Alloc");
    with_view_model_cpp(
        instance->view,
        UVMeterDataModel*,
        model,
        {
            model->buffer = furi_string_alloc();
            model->current_config_mode = UVMeterConfigModeGain;
            model->results.uv_a = 0;
            model->results.uv_b = 0;
            model->results.uv_c = 0;
            model->raw_results.uv_a = 0;
            model->raw_results.uv_b = 0;
            model->raw_results.uv_c = 0;
            model->effective_results.uv_a_eff = 0;
            model->effective_results.uv_b_eff = 0;
            model->effective_results.uv_c_eff = 0;
            model->effective_results.uv_total_eff = 0;
            model->effective_results.t_max = 0;
            model->gain_value = 1;
            model->conversion_time = 1.0;
            model->eyes_protected = true;
            model->unit = UVMeterUnituW_cm_2;
        },
        true);

    return instance;
}

void uv_meter_data_free(UVMeterData* instance) {
    furi_assert(instance);

    with_view_model_cpp(
        instance->view, UVMeterDataModel*, model, { furi_string_free(model->buffer); }, false);

    view_free(instance->view);
    delete instance;
    // Data View is not responsible for `sensor` and `sensor_mutex`
}

void uv_meter_data_reset(UVMeterData* instance, bool update) {
    furi_assert(instance);
    FURI_LOG_D("UV_Meter Data", "Update Reset");
    with_view_model_cpp(
        instance->view,
        UVMeterDataModel*,
        model,
        {
            model->current_config_mode = UVMeterConfigModeGain;
            model->results.uv_a = 0;
            model->results.uv_b = 0;
            model->results.uv_c = 0;
            model->raw_results.uv_a = 0;
            model->raw_results.uv_b = 0;
            model->raw_results.uv_c = 0;
            model->effective_results.uv_a_eff = 0;
            model->effective_results.uv_b_eff = 0;
            model->effective_results.uv_c_eff = 0;
            model->effective_results.uv_total_eff = 0;
            model->effective_results.t_max = 0;
            model->gain_value = 1;
            model->conversion_time = 1.0;
            model->eyes_protected = true;
            model->unit = UVMeterUnituW_cm_2;
        },
        update);
}

View* uv_meter_data_get_view(UVMeterData* instance) {
    furi_assert(instance);
    return instance->view;
}

void uv_meter_data_set_enter_settings_callback(
    UVMeterData* instance,
    UVMeterDataEnterSettingsCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(callback);
    with_view_model_cpp(
        instance->view,
        UVMeterDataModel*,
        model,
        {
            UNUSED(model);
            instance->callback = callback;
            instance->context = context;
        },
        false);
}

void uv_meter_data_set_sensor(UVMeterData* instance, AS7331* sensor, FuriMutex* sensor_mutex) {
    furi_assert(instance);
    furi_assert(sensor);
    furi_assert(sensor_mutex);
    with_view_model_cpp(
        instance->view,
        UVMeterDataModel*,
        model,
        {
            UNUSED(model);
            instance->sensor = sensor;
            instance->sensor_mutex = sensor_mutex;
        },
        false);
}

void uv_meter_update_from_sensor(UVMeterData* instance) {
    furi_assert(instance);
    if(instance->sensor) {
        furi_mutex_acquire(instance->sensor_mutex, FuriWaitForever);
        FURI_LOG_D("UV_Meter Data", "Update From Sensor");
        with_view_model_cpp(
            instance->view,
            UVMeterDataModel*,
            model,
            {
                model->gain_value = instance->sensor->getGainValue();
                model->conversion_time = instance->sensor->getConversionTime();
            },
            true);
        furi_mutex_release(instance->sensor_mutex);
    }
}

void uv_meter_data_set_results(
    UVMeterData* instance,
    const AS7331::Results* results,
    const AS7331::RawResults* raw_results) {
    furi_assert(instance);
    furi_assert(results);
    furi_assert(raw_results);

    FURI_LOG_D("UV_Meter Data", "Update Set Results");
    with_view_model_cpp(
        instance->view,
        UVMeterDataModel*,
        model,
        {
            model->results = *results;
            model->raw_results = *raw_results;
            model->effective_results =
                uv_meter_data_calculate_effective_results(&model->results, model->eyes_protected);
        },
        true);
}

UVMeterEffectiveResults uv_meter_data_get_effective_results(UVMeterData* instance) {
    furi_assert(instance);
    UVMeterEffectiveResults effective_results;

    with_view_model_cpp(
        instance->view,
        UVMeterDataModel*,
        model,
        { effective_results = model->effective_results; },
        false);

    return effective_results;
}

void uv_meter_data_set_eyes_protected(UVMeterData* instance, bool eyes_protected) {
    furi_assert(instance);
    FURI_LOG_D("UV_Meter Data", "Update Set Eyes Protected");
    with_view_model_cpp(
        instance->view,
        UVMeterDataModel*,
        model,
        { model->eyes_protected = eyes_protected; },
        true);
}

bool uv_meter_data_get_eyes_protected(UVMeterData* instance) {
    furi_assert(instance);
    bool eyes_protected = false;

    with_view_model_cpp(
        instance->view,
        UVMeterDataModel*,
        model,
        { eyes_protected = model->eyes_protected; },
        false);

    return eyes_protected;
}

void uv_meter_data_set_unit(UVMeterData* instance, UVMeterUnit unit) {
    furi_assert(instance);
    FURI_LOG_D("UV_Meter Data", "Update Set Unit");
    with_view_model_cpp(instance->view, UVMeterDataModel*, model, { model->unit = unit; }, true);
}

UVMeterEffectiveResults
    uv_meter_data_calculate_effective_results(const AS7331::Results* results, bool eyes_protected) {
    // Weighted Spectral Effectiveness
    double w_spectral_eff_uv_a = 0.0002824;
    double w_spectral_eff_uv_b = 0.3814;
    double w_spectral_eff_uv_c = 0.6047;

    if(eyes_protected) { // 😎
        // w_spectral_eff_uv_a is the same
        w_spectral_eff_uv_b = 0.2009;
        w_spectral_eff_uv_c = 0.2547;
    }
    UVMeterEffectiveResults effective_results;
    // Effective Irradiance
    effective_results.uv_a_eff = results->uv_a * w_spectral_eff_uv_a;
    effective_results.uv_b_eff = results->uv_b * w_spectral_eff_uv_b;
    effective_results.uv_c_eff = results->uv_c * w_spectral_eff_uv_c;
    effective_results.uv_total_eff =
        effective_results.uv_a_eff + effective_results.uv_b_eff + effective_results.uv_c_eff;

    // Daily dose (seconds) based on the total effective irradiance
    double daily_dose = 0.003; // J/cm^2
    double uW_to_W = 1e-6;
    effective_results.t_max = daily_dose / (effective_results.uv_total_eff * uW_to_W);
    return effective_results;
}
