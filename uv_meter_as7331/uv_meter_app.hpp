/**
 * @file uv_meter_app.hpp
 * @brief UV Meter Application for AS7331 UV Spectral Sensor
 *
 * This application interfaces with the AS7331 UV spectral sensor using I2C communication
 * to measure UV-A, UV-B, and UV-C irradiance. The measurements are displayed on the Flipper Zero's screen.
 *
 * Hardware Connections:
 * - SCL: C0 [pin 16]
 * - SDA: C1 [pin 15]
 * - 3V3: 3V3 [pin 9]
 * - GND: GND [pin 11 or 18]
 */
#pragma once

typedef struct UVMeterApp UVMeterApp;

typedef enum {
    UVMeterI2CAddressAuto,
    UVMeterI2CAddress74,
    UVMeterI2CAddress75,
    UVMeterI2CAddress76,
    UVMeterI2CAddress77,
} UVMeterI2CAddress;

typedef enum {
    UVMeterUnituW_cm_2,
    UVMeterUnitW_m_2,
    UVMeterUnitmW_m_2,
} UVMeterUnit;
