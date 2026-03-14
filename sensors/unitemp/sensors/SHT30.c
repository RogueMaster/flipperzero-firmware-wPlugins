/*
    Unitemp - Universal temperature reader
    Copyright (C) 2022-2026  Victor Nikitchuk (https://github.com/quen0n)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "SHT30.h"
#include "../interfaces/I2CSensor.h"

const SensorType SHT30 = {
    .typename = "SHT30",
    .altname = "SHT30/31/35",
    .interface = &I2C,
    .datatype = UT_TEMPERATURE | UT_HUMIDITY,
    .pollingInterval = 1000,
    .allocator = unitemp_SHT30_I2C_alloc,
    .mem_releaser = unitemp_SHT30_I2C_free,
    .initializer = unitemp_SHT30_init,
    .deinitializer = unitemp_SHT30_I2C_deinit,
    .updater = unitemp_SHT30_I2C_update};
const SensorType GXHT30 = {
    .typename = "GXHT30",
    .altname = "GXHT30/31/35",
    .interface = &I2C,
    .datatype = UT_TEMPERATURE | UT_HUMIDITY,
    .pollingInterval = 1000,
    .allocator = unitemp_SHT30_I2C_alloc,
    .mem_releaser = unitemp_SHT30_I2C_free,
    .initializer = unitemp_GXHT30_init,
    .deinitializer = unitemp_SHT30_I2C_deinit,
    .updater = unitemp_SHT30_I2C_update};

bool unitemp_SHT30_I2C_alloc(Sensor* sensor, char* args) {
    UNUSED(args);
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;

    //Addresses on the I2C bus (7 bits)
    i2c_sensor->minI2CAdr = 0x44 << 1;
    i2c_sensor->maxI2CAdr = 0x45 << 1;
    return true;
}

bool unitemp_SHT30_I2C_free(Sensor* sensor) {
    //Nothing to release since nothing was allocated
    UNUSED(sensor);
    return true;
}

static uint8_t unitemp_SHT30_crc8(const uint8_t* data, const size_t len) {
    // From Sensirion application note: polynomial 0x31, init 0xFF
    uint8_t crc = 0xFF;

    for(size_t byte = 0; byte < len; ++byte) {
        crc ^= data[byte];
        for(uint8_t bit = 0; bit < 8; ++bit) {
            if(crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc = (crc << 1);
            }
        }
    }

    return crc;
}

static bool unitemp_SHT30_readSerial(I2CSensor* i2c_sensor, uint32_t* serial) {
    // Read 32-bit unique serial number; does not encode the exact SHT3x variant
    uint8_t command[2] = {0x37, 0x80};
    uint8_t response[6] = {0};

    if(!unitemp_i2c_writeArray(i2c_sensor, 2, command)) return false;
    furi_delay_ms(1);
    if(!unitemp_i2c_readArray(i2c_sensor, 6, response)) return false;

    if(unitemp_SHT30_crc8(response, 2) != response[2]) return false;
    if(unitemp_SHT30_crc8(&response[3], 2) != response[5]) return false;

    if(serial) {
        *serial = ((uint32_t)response[0] << 24) | ((uint32_t)response[1] << 16) |
                  ((uint32_t)response[3] << 8) | response[4];
    }

    return true;
}

static bool unitemp_SHT30_startPeriodicMeasurements(I2CSensor* i2c_sensor) {
    /*
     * SHT3x devices power on in an idle state. The "fetch data" command (0xE000)
     * returns a NACK until the sensor has been switched into periodic
     * measurement mode at least once. Without priming the device, the first
     * poll after connecting the probe can look identical to a missing sensor.
     * Enabling periodic mode here ensures subsequent reads return real data
     * instead of a NACK so we do not incorrectly treat an attached probe as
     * absent.
     */
    //Enable automatic conversion mode 2 times per second
    uint8_t data[2] = {0x22, 0x36};
    return unitemp_i2c_writeArray(i2c_sensor, 2, data);
}

bool unitemp_SHT30_init(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;
    if(!unitemp_SHT30_startPeriodicMeasurements(i2c_sensor)) return false;

    uint32_t serial = 0;
    if(unitemp_SHT30_readSerial(i2c_sensor, &serial)) {
        /*
         * Sensirion exposes a unique serial number but not a product identifier.
         * We can therefore confirm an SHT3x-family device is alive on the bus and
         * surface the serial for troubleshooting, but we cannot algorithmically
         * distinguish SHT30 vs SHT31 vs SHT35 beyond user-provided context.
         */
        FURI_LOG_I(
            APP_NAME,
            "SHT3x detected at 0x%02X (serial 0x%08lX)",
            i2c_sensor->currentI2CAdr,
            (unsigned long)serial);
    } else {
        FURI_LOG_W(APP_NAME, "SHT3x serial read failed at 0x%02X", i2c_sensor->currentI2CAdr);
    }

    return true;
}

bool unitemp_GXHT30_init(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;
    return unitemp_SHT30_startPeriodicMeasurements(i2c_sensor);
}

bool unitemp_SHT30_I2C_deinit(Sensor* sensor) {
    //Nothing to deinitialize
    UNUSED(sensor);
    return true;
}

UnitempStatus unitemp_SHT30_I2C_update(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;
    //Receiving data
    uint8_t data[6] = {0xE0, 0x00};
    if(!unitemp_i2c_writeArray(i2c_sensor, 2, data) ||
       !unitemp_i2c_readArray(i2c_sensor, 6, data)) {
        /*
         * Reading the "fetch data" command (0xE000) returns a NACK if the sensor was
         * never switched into periodic measurement mode. Trigger periodic
         * measurements here and retry once so we don't misclassify a present SHT3x as
         * disconnected after the first poll.
         */
        if(!unitemp_SHT30_startPeriodicMeasurements(i2c_sensor)) return UT_SENSORSTATUS_TIMEOUT;
        furi_delay_ms(10);
        data[0] = 0xE0;
        data[1] = 0x00;
        if(!unitemp_i2c_writeArray(i2c_sensor, 2, data)) return UT_SENSORSTATUS_TIMEOUT;
        if(!unitemp_i2c_readArray(i2c_sensor, 6, data)) return UT_SENSORSTATUS_TIMEOUT;
    }

    sensor->temp = -45 + 175 * (((uint16_t)(data[0] << 8) | data[1]) / 65535.0f);
    sensor->hum = 100 * (((uint16_t)(data[3] << 8) | data[4]) / 65535.0f);

    return UT_SENSORSTATUS_OK;
}
