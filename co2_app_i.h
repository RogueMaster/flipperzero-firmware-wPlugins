#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>

#include "sensors/mhz19_pwm.h"

#define APP_TAG "CO2App"

// Shared sensor data (protected by mutex)
typedef struct {
    int32_t  co2_ppm;
    bool     co2_connected;
    float    temperature;
    float    humidity;
    float    pressure;
    bool     bme280_valid;
} AppData;

// Main application struct
typedef struct {
    Gui*              gui;
    ViewPort*         view_port;
    FuriMessageQueue* event_queue;
    FuriMutex*        mutex;
    AppData           data;

    // MH-Z19 PWM (GPIO PA6)
    const GpioPin* co2_pin;
    SensorRange    co2_range;
    int32_t        co2_prevVal;
    int32_t        co2_th;
    int32_t        co2_tl;
    int32_t        co2_h;
    int32_t        co2_l;

    // BME280 I2C
    bool    bme280_found;
    uint8_t bme280_addr; // 8-bit shifted: 0xEC (0x76<<1) or 0xEE (0x77<<1)
    struct {
        uint16_t dig_T1; int16_t dig_T2; int16_t dig_T3;
        uint16_t dig_P1; int16_t dig_P2; int16_t dig_P3;
        int16_t  dig_P4; int16_t dig_P5; int16_t dig_P6;
        int16_t  dig_P7; int16_t dig_P8; int16_t dig_P9;
        uint8_t  dig_H1; int16_t dig_H2; uint8_t  dig_H3;
        int16_t  dig_H4; int16_t dig_H5; int8_t   dig_H6;
        int32_t  t_fine;
    } bme280_cal;
} App;

// BME280 API
void bme280_init(App* app);
void bme280_deinit(App* app);
bool bme280_read(App* app, float* temp, float* hum, float* press);

// Entry point
int32_t co2_app_main(void* p);
