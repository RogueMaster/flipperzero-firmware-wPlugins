/*
 * BME280 glue — bridges unitemp BMx280 driver to our App*.
 *
 * Uses verbatim copies of unitemp BMx280.c / I2CSensor.c compiled as part
 * of this application.  Only ~50 lines here; all sensor logic lives in the
 * unitemp source files.
 */
#include "../co2_app_i.h"
#include "unitemp/sensors/BMx280.h"

/* Pa returned by BMx280_compensate_pressure → divide to get hPa */
#define PA_TO_HPA 100.0f

/* Static storage — allocated once for the lifetime of the app */
static Sensor    g_sensor;
static I2CSensor g_i2c;
static BMx280_instance g_bme_inst;

static bool try_init_addr(uint8_t addr) {
    /* Wire up the unitemp structures manually (skipping the full alloc path
     * that would call gpio_lock — our gpio_stub no-ops those anyway). */
    g_bme_inst.chip_id           = 0x60; /* BME280_ID — enables humidity */
    g_bme_inst.last_cal_update_time = 0;

    g_i2c.i2c            = &furi_hal_i2c_handle_external;
    g_i2c.currentI2CAdr  = addr;
    g_i2c.minI2CAdr      = 0x76 << 1;
    g_i2c.maxI2CAdr      = 0x77 << 1;
    g_i2c.sensorInstance = &g_bme_inst;

    g_sensor.instance = &g_i2c;
    g_sensor.type     = &BME280;
    g_sensor.name     = "BME280";

    return unitemp_BMx280_init(&g_sensor);
}

/* --- Public API (signatures match co2_app_i.h declarations) -------------- */

void bme280_init(App* app) {
    app->bme280_found = false;

    /* Try 0x76 first, then 0x77 */
    uint8_t addrs[2] = {0x76 << 1, 0x77 << 1};
    for(int i = 0; i < 2; i++) {
        if(try_init_addr(addrs[i])) {
            app->bme280_found = true;
            app->bme280_addr  = addrs[i];
            FURI_LOG_I(APP_TAG, "BME280 at 0x%02X", addrs[i] >> 1);
            return;
        }
    }
    FURI_LOG_W(APP_TAG, "BME280 not found");
}

void bme280_deinit(App* app) {
    if(app->bme280_found) {
        unitemp_BMx280_deinit(&g_sensor);
    }
}

bool bme280_read(App* app, float* temp, float* hum, float* press) {
    if(!app->bme280_found) return false;

    UnitempStatus st = unitemp_BMx280_update(&g_sensor);
    if(st != UT_SENSORSTATUS_OK) {
        app->bme280_found = false; /* force re-init on next poll */
        return false;
    }

    *temp  = g_sensor.temp;
    *hum   = g_sensor.hum;
    *press = g_sensor.pressure / PA_TO_HPA; /* Pa → hPa */
    return true;
}
