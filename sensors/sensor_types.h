#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int16_t co2_ppm;       // MH-Z19: 0-5000, -1 = invalid
    int8_t co2_temp;       // MH-Z19 internal temperature (from UART response)
    float temperature;     // BME280: degrees C
    float humidity;        // BME280: %RH
    float pressure;        // BME280: hPa
    bool co2_valid;
    bool bme280_valid;
    bool co2_warming_up;   // First 3 minutes after power-on
    uint32_t co2_update_tick; // furi_get_tick() when last valid CO2 received
} SensorData;
