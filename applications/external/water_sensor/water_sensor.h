#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_adc.h>
#include <furi_hal_resources.h>

#ifndef ADC_GPIO
#define ADC_GPIO (&gpio_ext_pc0)
#endif

#ifndef ADC_CHANNEL
#define ADC_CHANNEL FuriHalAdcChannel1
#endif

#define UPDATE_MS 400

int32_t water_sensor_main(void* p);
