#include "veml7700.hpp"
#include "furi_hal_i2c.h"
#include <furi.h>

// Timeout dla operacji I2C (w milisekundach)
#define VEML7700_I2C_TIMEOUT 100

// Rejestry VEML7700
#define ALS_CONF_REG        0x00
#define ALS_MEAS_RESULT_REG 0x04

VEML7700::VEML7700(uint8_t i2c_address_7bit)
    : _i2c_addr_8bit(i2c_address_7bit << 1) {
}

bool VEML7700::init() {
    uint16_t initial_config_value = 0x0000;
    uint8_t config_buffer[3];
    config_buffer[0] = ALS_CONF_REG;
    config_buffer[1] = static_cast<uint8_t>(initial_config_value & 0xFF); // LSB
    config_buffer[2] = static_cast<uint8_t>((initial_config_value >> 8) & 0xFF); // MSB

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool success = furi_hal_i2c_tx(
        &furi_hal_i2c_handle_external, _i2c_addr_8bit, config_buffer, 3, VEML7700_I2C_TIMEOUT);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    if(!success) {
        FURI_LOG_E("VEML7700", "Blad zapisu konfiguracji I2C.");
        return false;
    }

    // Krótka pauza po konfiguracji
    furi_delay_ms(500);

    return true;
}

bool VEML7700::readLux(float& lux_value) {
    uint8_t raw_data[2];
    uint8_t reg_addr = ALS_MEAS_RESULT_REG;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    // Wysłanie adresu rejestru
    bool success = furi_hal_i2c_tx_ext(
        &furi_hal_i2c_handle_external,
        _i2c_addr_8bit,
        false,
        &reg_addr,
        1,
        FuriHalI2cBeginStart,
        FuriHalI2cEndAwaitRestart,
        VEML7700_I2C_TIMEOUT);

    if(!success) {
        FURI_LOG_E("VEML7700", "Blad wysylania adresu rejestru.");
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        return false;
    }

    // Odczyt dwóch bajtów danych
    success = furi_hal_i2c_rx_ext(
        &furi_hal_i2c_handle_external,
        _i2c_addr_8bit,
        false,
        raw_data,
        2,
        FuriHalI2cBeginRestart,
        FuriHalI2cEndStop,
        VEML7700_I2C_TIMEOUT);

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    if(!success) {
        FURI_LOG_E("VEML7700", "Blad odczytu danych.");
        return false;
    }

    // Konwersja na 16-bitową wartość
    uint16_t als_value = (static_cast<uint16_t>(raw_data[1]) << 8) | raw_data[0];

    // Obliczenie wartości lux
    lux_value = als_value * 0.0336f;

    return true;
}
