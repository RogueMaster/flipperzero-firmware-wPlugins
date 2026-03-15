#include "mhz19c_sensor.h"
#include "mhz19_pwm.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <furi_hal_power.h>
#include <string.h>

#define MHZ19C_CO2_BUF_SIZE 5

/* ---- Instance struct ---- */
/* No edge for this many ms → sensor considered disconnected */
#define MHZ19C_DISCONNECT_TIMEOUT_MS 3000

typedef struct {
    int32_t prevVal;
    int32_t th;
    int32_t tl;
    int32_t h;
    int32_t l;
    int32_t co2_buf[MHZ19C_CO2_BUF_SIZE];
    uint8_t buf_idx;
    uint8_t buf_count;
    uint32_t last_edge_tick;
    bool disconnected;
} MHZ19CInstance;

/* ---- Custom "direct GPIO" interface ---- */
/*
 * MHZ19C doesn't use a standard bus (I2C/SPI/1-Wire/SingleWire).
 * We wrap the SensorType functions in a thin Interface so that
 * Sensors.c generic code (unitemp_sensor_alloc etc.) works without modification.
 */

static bool mhz19c_if_alloc(Sensor* sensor, char* args) {
    UNUSED(args);
    return sensor->type->allocator(sensor, NULL);
}

static bool mhz19c_if_free(Sensor* sensor) {
    return sensor->type->mem_releaser(sensor);
}

static UnitempStatus mhz19c_if_update(Sensor* sensor) {
    return sensor->type->updater(sensor);
}

static const Interface DIRECT_GPIO = {
    .name         = "DirectGPIO",
    .allocator    = mhz19c_if_alloc,
    .mem_releaser = mhz19c_if_free,
    .updater      = mhz19c_if_update,
};

/* ---- SensorType callbacks ---- */

static bool mhz19c_alloc(Sensor* sensor, char* args) {
    UNUSED(args);
    MHZ19CInstance* inst = malloc(sizeof(MHZ19CInstance));
    if(!inst) return false;
    memset(inst, 0, sizeof(MHZ19CInstance));
    sensor->instance = inst;
    return true;
}

static bool mhz19c_free(Sensor* sensor) {
    free(sensor->instance);
    sensor->instance = NULL;
    return true;
}

static bool mhz19c_init(Sensor* sensor) {
    MHZ19CInstance* inst = sensor->instance;
    inst->last_edge_tick = furi_get_tick();
    inst->disconnected = false;
    if(!furi_hal_power_is_otg_enabled()) {
        furi_hal_power_enable_otg();
    }
    furi_hal_gpio_init(&gpio_ext_pa6, GpioModeInput, GpioPullUp, GpioSpeedVeryHigh);
    return true;
}

static bool mhz19c_deinit(Sensor* sensor) {
    UNUSED(sensor);
    furi_hal_gpio_init(&gpio_ext_pa6, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    if(!app->settings.lastOTGState) {
        furi_hal_power_disable_otg();
    }
    return true;
}

static UnitempStatus mhz19c_update(Sensor* sensor) {
    MHZ19CInstance* inst = sensor->instance;

    int32_t gpio_val = furi_hal_gpio_read(&gpio_ext_pa6) ? 1 : 0;

    int32_t old_prev = inst->prevVal;
    int32_t ppm = calculate_ppm(
        &inst->prevVal,
        gpio_val,
        &inst->th,
        &inst->tl,
        &inst->h,
        &inst->l,
        RANGE_2000);

    bool edge_detected = (inst->prevVal != old_prev);

    if(edge_detected) {
        inst->last_edge_tick = furi_get_tick();
        if(inst->disconnected) {
            /* First edge after disconnect — reset state machine, skip stale ppm */
            inst->disconnected = false;
            inst->th = 0;
            inst->tl = 0;
            inst->h = 0;
            inst->l = 0;
            memset(inst->co2_buf, 0, sizeof(inst->co2_buf));
            inst->buf_idx = 0;
            inst->buf_count = 0;
            return UT_SENSORSTATUS_POLLING;
        }
    } else {
        /* No edge — check for disconnect timeout */
        if(!inst->disconnected && inst->last_edge_tick > 0 &&
           (furi_get_tick() - inst->last_edge_tick) > MHZ19C_DISCONNECT_TIMEOUT_MS) {
            inst->disconnected = true;
            sensor->co2 = -1.0f;
        }
    }

    if(ppm > 0) {
        inst->co2_buf[inst->buf_idx] = ppm;
        inst->buf_idx = (inst->buf_idx + 1) % MHZ19C_CO2_BUF_SIZE;
        if(inst->buf_count < MHZ19C_CO2_BUF_SIZE) inst->buf_count++;

        /* Trimmed mean: sort, drop min+max, average the rest */
        int32_t sorted[MHZ19C_CO2_BUF_SIZE];
        memcpy(sorted, inst->co2_buf, inst->buf_count * sizeof(int32_t));
        for(uint8_t i = 1; i < inst->buf_count; i++) {
            int32_t key = sorted[i];
            int8_t j = (int8_t)i - 1;
            while(j >= 0 && sorted[j] > key) {
                sorted[j + 1] = sorted[j];
                j--;
            }
            sorted[j + 1] = key;
        }

        int32_t filtered;
        if(inst->buf_count < 3) {
            filtered = sorted[inst->buf_count / 2];
        } else {
            int32_t sum = 0;
            for(uint8_t i = 1; i < inst->buf_count - 1; i++) sum += sorted[i];
            filtered = sum / (int32_t)(inst->buf_count - 2);
        }

        sensor->co2 = (float)filtered;
        return UT_SENSORSTATUS_OK;
    }

    /* Still measuring (waiting for edge) */
    return UT_SENSORSTATUS_POLLING;
}

/* ---- Public SensorType ---- */
const SensorType MHZ19C = {
    .typename       = "MHZ19C",
    .altname        = "MH-Z19C (PWM)",
    .datatype       = UT_DATA_TYPE_CO2,
    .interface      = &DIRECT_GPIO,
    .pollingInterval = 100,
    .allocator      = mhz19c_alloc,
    .mem_releaser   = mhz19c_free,
    .initializer    = mhz19c_init,
    .deinitializer  = mhz19c_deinit,
    .updater        = mhz19c_update,
};
