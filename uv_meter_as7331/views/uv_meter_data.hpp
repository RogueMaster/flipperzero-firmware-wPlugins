#pragma once

#include <gui/view.h>
#include "AS7331.hpp"
#include "uv_meter_app.hpp"

typedef struct UVMeterData UVMeterData;
typedef void (*UVMeterDataEnterSettingsCallback)(void* context);

/** @brief Struct to hold effective irradiance and daily exposure time */
typedef struct {
    double uv_a_eff; /**< Effective UV-A Irradiance in µW/cm² */
    double uv_b_eff; /**< Effective UV-B Irradiance in µW/cm² */
    double uv_c_eff; /**< Effective UV-C Irradiance in µW/cm² */
    double uv_total_eff; /**< Effective total UV Irradiance in µW/cm² */
    double t_max; /**< Maximum Daily Exposure Time in seconds*/
} UVMeterEffectiveResults;

UVMeterData* uv_meter_data_alloc(void);
void uv_meter_data_free(UVMeterData* instance);
void uv_meter_data_reset(UVMeterData* instance, bool update = false);

View* uv_meter_data_get_view(UVMeterData* instance);

void uv_meter_data_set_enter_settings_callback(
    UVMeterData* instance,
    UVMeterDataEnterSettingsCallback callback,
    void* context);

// AS7331 Sensor
void uv_meter_data_set_sensor(UVMeterData* instance, AS7331* sensor, FuriMutex* sensor_mutex);
void uv_meter_update_from_sensor(UVMeterData* instance);

// General getter and setter
void uv_meter_data_set_results(
    UVMeterData* instance,
    const AS7331::Results* results,
    const AS7331::RawResults* raw_results);
UVMeterEffectiveResults uv_meter_data_get_effective_results(UVMeterData* instance);
void uv_meter_data_set_eyes_protected(UVMeterData* instance, bool eyes_protected);
bool uv_meter_data_get_eyes_protected(UVMeterData* instance);
void uv_meter_data_set_unit(UVMeterData* instance, UVMeterUnit unit);

// Helper
UVMeterEffectiveResults
    uv_meter_data_calculate_effective_results(const AS7331::Results* results, bool eyes_protected);
