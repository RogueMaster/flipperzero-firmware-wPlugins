#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    const GpioPin* output_pin;
    bool active;              // Current relay state
    bool enabled;             // Feature enabled/disabled
    uint16_t on_threshold;    // CO2 ppm to turn ON (default 1000)
    uint16_t off_threshold;   // CO2 ppm to turn OFF (default 800)
    uint32_t debounce_ms;     // Min time between switches (default 30000)
    uint32_t last_change_tick;
} FlapControl;

typedef struct Co2App Co2App;

void flap_control_init(FlapControl* flap);
void flap_control_deinit(FlapControl* flap);
void flap_control_update(FlapControl* flap, int16_t co2_ppm);
