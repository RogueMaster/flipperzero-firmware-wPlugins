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

#include "DS18x2x.h"
#include "interfaces/onewire_sensor.h"

#include <one_wire/maxim_crc.h>
#include <one_wire/one_wire_host.h>

const SensorModel Dallas = {
    .modelname = "Dallas",
    .altname = "DS18B20(22)/DS1820",
    .interface = &unitemp_1w,
    .data_type = UT_DATA_TYPE_TEMP,
    .polling_interval = 750,
    .allocator = unitemp_ds18x2x_sensor_alloc,
    .mem_releaser = unitemp_ds18x2x_sensor_free,
    .initializer = unitemp_ds18x2x_sensor_init,
    .deinitializer = unitemp_ds18x2x_sensor_deinit,
    .updater = unitemp_ds18x2x_sensor_update};

#define DS18B20_CMD_SKIP_ROM        0xCCU
#define DS18B20_CMD_CONVERT         0x44U
#define DS18B20_CMD_READ_SCRATCHPAD 0xBEU

bool unitemp_ds18x2x_sensor_alloc(Sensor* sensor, char* args) {
    OneWireSensor* instance = malloc(sizeof(OneWireSensor));
    if(instance == NULL) {
        FURI_LOG_E(APP_NAME, "Sensor %s instance allocation error", sensor->name);
        return false;
    }
    sensor->instance = instance;
    //Address clearing
    memset(instance->deviceID, 0, 8);

    int gpio_pin, addr_0, addr_1, addr_2, addr_3, addr_4, addr_5, addr_6, addr_7;
    sscanf(
        args,
        "%d %2X%2X%2X%2X%2X%2X%2X%2X",
        &gpio_pin,
        &addr_0,
        &addr_1,
        &addr_2,
        &addr_3,
        &addr_4,
        &addr_5,
        &addr_6,
        &addr_7);
    instance->deviceID[0] = addr_0;
    instance->deviceID[1] = addr_1;
    instance->deviceID[2] = addr_2;
    instance->deviceID[3] = addr_3;
    instance->deviceID[4] = addr_4;
    instance->deviceID[5] = addr_5;
    instance->deviceID[6] = addr_6;
    instance->deviceID[7] = addr_7;

    instance->family_code = instance->deviceID[0];

    instance->bus = unitemp_onewire_bus_alloc(unitemp_gpio_get_from_int(gpio_pin));

    if(instance != NULL) {
        return true;
    }
    FURI_LOG_E(APP_NAME, "Sensor %s bus allocation error", sensor->name);
    free(instance);
    return false;
}

bool unitemp_ds18x2x_sensor_free(Sensor* sensor) {
    unitemp_onewire_bus_free(((OneWireSensor*)sensor->instance)->bus);
    free(sensor->instance);

    return true;
}

bool unitemp_ds18x2x_sensor_init(Sensor* sensor) {
    OneWireSensor* instance = sensor->instance;
    if(instance == NULL || instance->bus == NULL) {
        FURI_LOG_E(APP_NAME, "Sensor pointer is null!");
        return false;
    }

    unitemp_onewire_bus_init(instance->bus);
    furi_delay_ms(1);

    bool result = false;
    if(instance->family_code == FC_DS18B20 || instance->family_code == FC_DS1822) {
        FURI_CRITICAL_ENTER();
        do {
            //Setting the bit depth to 10 bits
            if(!unitemp_onewire_bus_start(instance->bus)) {
                UNITEMP_DEBUG(
                    "Failed to start bus %s for sensor %s",
                    instance->bus->bus_pin->name,
                    sensor->name);
                break;
            }
            unitemp_onewire_bus_select_device(instance->bus, instance->deviceID);

            unitemp_onewire_bus_write(instance->bus, 0x4E); //Memory recording
            uint8_t buff[3];
            //Alarm values
            buff[0] = 0x4B; //Temperature lower limit value
            buff[1] = 0x46; //Upper temperature limit value
            //Configuration
            buff[2] = 0b01111111; //12 bit bit conversion
            unitemp_onewire_bus_write_bytes(instance->bus, buff, 3);

            //Stores values ​​in EEPROM for automatic recovery after power failures
            if(!unitemp_onewire_bus_start(instance->bus)) {
                UNITEMP_DEBUG(
                    "Failed to start bus %s for sensor %s after data writing",
                    instance->bus->bus_pin->name,
                    sensor->name);
                break;
            }
            unitemp_onewire_bus_select_device(instance->bus, instance->deviceID);
            unitemp_onewire_bus_write(instance->bus, 0x48); //Write to EEPROM
            result = true;
        } while(0);
        FURI_CRITICAL_EXIT();
    } else {
        FURI_CRITICAL_ENTER();
        do {
            if(!unitemp_onewire_bus_start(instance->bus)) {
                UNITEMP_DEBUG(
                    "Failed to start bus %s for sensor %s",
                    instance->bus->bus_pin->name,
                    sensor->name);
                break;
            }
            uint8_t buff[9];
            unitemp_onewire_bus_select_device(instance->bus, instance->deviceID);
            unitemp_onewire_bus_write(
                instance->bus, DS18B20_CMD_READ_SCRATCHPAD); // Read Scratch-pad
            unitemp_onewire_bus_read_bytes(instance->bus, buff, 9);
            if(!unitemp_onewire_CRC_check(buff, 9)) {
                UNITEMP_DEBUG("Failed CRC check: %s", sensor->name);
                break;
            }
            result = true;
        } while(0);
        FURI_CRITICAL_EXIT();
    }

    if(!result) unitemp_onewire_bus_deinit(instance->bus);

    return result;
}

bool unitemp_ds18x2x_sensor_deinit(Sensor* sensor) {
    OneWireSensor* instance = sensor->instance;
    if(instance == NULL || instance->bus == NULL) return false;
    unitemp_onewire_bus_deinit(instance->bus);

    return true;
}

SensorStatus unitemp_ds18x2x_sensor_update(Sensor* sensor) {
    //Removing the special status from the sensor in passive power mode
    if(sensor->status == UT_SENSORSTATUS_EARLYPOOL) {
        return UT_SENSORSTATUS_POLLING;
    }

    OneWireSensor* instance = sensor->instance;
    uint8_t buff[9] = {0};

    if(sensor->status != UT_SENSORSTATUS_POLLING) {
        if(!unitemp_onewire_bus_start(instance->bus)) return UT_SENSORSTATUS_TIMEOUT;
        //Starting conversion on all sensors in passive power mode
        unitemp_onewire_bus_write(instance->bus, DS18B20_CMD_SKIP_ROM); // skip addr
        //Setting a special status on all sensors of this bus so as not to start the conversion again
        for(uint8_t i = 0; i < unitemp_sensors_get_count(); i++) {
            if(unitemp_sensors_get(i)->model->interface == &unitemp_1w &&
               ((OneWireSensor*)unitemp_sensors_get(i)->instance)->bus == instance->bus) {
                unitemp_sensors_get(i)->status = UT_SENSORSTATUS_EARLYPOOL;
            }
        }

        unitemp_onewire_bus_write(instance->bus, DS18B20_CMD_CONVERT); // convert t

        unitemp_onewire_bus_strong_mode(instance->bus, true);
        return UT_SENSORSTATUS_POLLING;
    } else {
        unitemp_onewire_bus_strong_mode(instance->bus, false);

        if(!unitemp_onewire_bus_start(instance->bus)) return UT_SENSORSTATUS_TIMEOUT;
        unitemp_onewire_bus_select_device(instance->bus, instance->deviceID);
        unitemp_onewire_bus_write(instance->bus, DS18B20_CMD_READ_SCRATCHPAD); // Read Scratch-pad
        unitemp_onewire_bus_read_bytes(instance->bus, buff, 9);
        if(!unitemp_onewire_CRC_check(buff, 9)) {
            UNITEMP_DEBUG("Failed CRC check: %s", sensor->name);
            return UT_SENSORSTATUS_BADCRC;
        }
        int16_t raw = buff[0] | ((int16_t)buff[1] << 8);
        if(instance->family_code == FC_DS18S20) {
            //Pseudo-12-bit.
            //sensor->temperature = ((float)raw / 2.0f) - 0.25f + (16.0f - buff[6]) / 16.0f;
            //Honest 9 bits
            sensor->temperature = ((float)raw / 2.0f);
        } else {
            sensor->temperature = (float)raw / 16.0f;
        }
    }

    return UT_SENSORSTATUS_OK;
}
