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
//Code used by Dmitry Pogrebnyak: https://aterlux.ru/article/1wire

#include "OneWireSensor.h"
#include <furi.h>
#include <furi_hal.h>
#include <one_wire/one_wire_host.h>

const SensorType Dallas = {
    .typename = "Dallas",
    .altname = "Dallas (DS18x2x)",
    .interface = &ONE_WIRE,
    .datatype = UT_DATA_TYPE_TEMP,
    .pollingInterval = 1000,
    .allocator = unitemp_onewire_sensor_alloc,
    .mem_releaser = unitemp_onewire_sensor_free,
    .initializer = unitemp_onewire_sensor_init,
    .deinitializer = unitemp_onewire_sensor_deinit,
    .updater = unitemp_onewire_sensor_update};

//Variables for storing the intermediate result of a bus scan
//found eight-byte address
static uint8_t onewire_enum[8] = {0};
//the last zero bit where there was ambiguity (numbering from one)
static uint8_t onewire_enum_fork_bit = 65;

OneWireBus* unitemp_onewire_bus_alloc(const GPIO* gpio) {
    if(gpio == NULL) {
        return NULL;
    }

    //Checking for bus presence on this port
    for(uint8_t i = 0; i < unitemp_sensors_getActiveCount(); i++) {
        if(unitemp_sensor_getActive(i)->type->interface == &ONE_WIRE &&
           ((OneWireSensor*)unitemp_sensor_getActive(i)->instance)->bus->gpio->num == gpio->num) {
            //If there is already a bus on this port, then return a pointer to the bus
            return ((OneWireSensor*)unitemp_sensor_getActive(i)->instance)->bus;
        }
    }

    OneWireBus* bus = malloc(sizeof(OneWireBus));
    bus->device_count = 0;
    bus->gpio = gpio;
    bus->powerMode = PWR_PASSIVE;
    UNITEMP_DEBUG("one wire bus (port %d) allocated", gpio->num);

    return bus;
}

bool unitemp_onewire_bus_init(OneWireBus* bus) {
    if(bus == NULL) return false;
    bus->device_count++;
    //Output if the bus has already been initialized
    if(bus->device_count > 1) return true;

    unitemp_gpio_lock(bus->gpio, &ONE_WIRE);
    //High default level
    furi_hal_gpio_write(bus->gpio->pin, true);
    //Operating mode - OpenDrain, the lift is turned on just in case
    furi_hal_gpio_init(
        bus->gpio->pin, //Port FZ
        GpioModeOutputOpenDrain, //Operating mode - open drain
        GpioPullUp, //Forced pull-up of the data line to power
        GpioSpeedVeryHigh); //Operating speed - maximum

    return true;
}
bool unitemp_onewire_bus_deinit(OneWireBus* bus) {
    UNITEMP_DEBUG("devices on wire %d: %d", bus->gpio->num, bus->device_count);
    bus->device_count--;
    if(bus->device_count <= 0) {
        bus->device_count = 0;
        unitemp_gpio_unlock(bus->gpio);
        //Operating mode: analogue, lifting off
        furi_hal_gpio_init(
            bus->gpio->pin, //Port FZ
            GpioModeAnalog, //Operating mode - analog
            GpioPullNo, //Pull-up is turned off
            GpioSpeedLow); //Operating speed - minimum
        //Low level by default
        furi_hal_gpio_write(bus->gpio->pin, false);
        return true;
    } else {
        return false;
    }
}
bool unitemp_onewire_bus_start(OneWireBus* bus) {
    furi_hal_gpio_write(bus->gpio->pin, false);
    furi_delay_us(500);

    furi_hal_gpio_write(bus->gpio->pin, true);

    //Waiting for the tire to rise
    uint32_t t = furi_get_tick();
    while(!furi_hal_gpio_read(bus->gpio->pin)) {
        //Exit if the tire does not rise
        if(furi_get_tick() - t > 10) return false;
    }

    furi_delay_us(100);
    bool status = !furi_hal_gpio_read(bus->gpio->pin);
    furi_delay_us(400);
    return status;
}

void unitemp_onewire_bus_send_bit(OneWireBus* bus, bool state) {
    //Necessary for stable operation with passive power supply
    if(bus->powerMode == PWR_PASSIVE) furi_delay_us(100);

    if(state) {
        // write 1
        furi_hal_gpio_write(bus->gpio->pin, false);
        furi_delay_us(1);
        furi_hal_gpio_write(bus->gpio->pin, true);
        furi_delay_us(90);
    } else {
        furi_hal_gpio_write(bus->gpio->pin, false);
        furi_delay_us(90);
        furi_hal_gpio_write(bus->gpio->pin, true);
        //Waiting for the tire to rise
        uint32_t t = furi_get_tick();
        while(!furi_hal_gpio_read(bus->gpio->pin)) {
            //Exit if the tire does not rise
            if(furi_get_tick() - t > 10) return;
        }
    }
}

void unitemp_onewire_bus_send_byte(OneWireBus* bus, uint8_t data) {
    for(int i = 0; i < 8; i++) {
        unitemp_onewire_bus_send_bit(bus, (data & (1 << i)) != 0);
    }
}

void unitemp_onewire_bus_send_byteArray(OneWireBus* bus, uint8_t* data, uint8_t len) {
    for(uint8_t i = 0; i < len; i++) {
        unitemp_onewire_bus_send_byte(bus, data[i]);
    }
}

bool unitemp_onewire_bus_read_bit(OneWireBus* bus) {
    furi_delay_ms(1);
    furi_hal_gpio_write(bus->gpio->pin, false);
    furi_delay_us(2); //Low level duration, minimum 1 µs
    furi_hal_gpio_write(bus->gpio->pin, true);
    furi_delay_us(8); //Pause until sampling, no more than 15 μs in total
    bool r = furi_hal_gpio_read(bus->gpio->pin);
    furi_delay_us(80); //Wait until next time slot, minimum 60 µs from start low
    return r;
}

uint8_t unitemp_onewire_bus_read_byte(OneWireBus* bus) {
    uint8_t r = 0;
    for(uint8_t p = 8; p; p--) {
        r >>= 1;
        if(unitemp_onewire_bus_read_bit(bus)) r |= 0x80;
    }
    return r;
}

void unitemp_onewire_bus_read_byteArray(OneWireBus* bus, uint8_t* data, uint8_t len) {
    for(uint8_t i = 0; i < len; i++) {
        data[i] = unitemp_onewire_bus_read_byte(bus);
    }
}

static uint8_t onewire_CRC_update(uint8_t crc, uint8_t b) {
    for(uint8_t p = 8; p; p--) {
        crc = ((crc ^ b) & 1) ? (crc >> 1) ^ 0b10001100 : (crc >> 1);
        b >>= 1;
    }
    return crc;
}

bool unitemp_onewire_CRC_check(uint8_t* data, uint8_t len) {
    uint8_t crc = 0;
    for(uint8_t i = 0; i < len; i++) {
        crc = onewire_CRC_update(crc, data[i]);
    }
    return !crc;
}

char* unitemp_onewire_sensor_getModel(Sensor* sensor) {
    OneWireSensor* ow_sensor = sensor->instance;
    switch(ow_sensor->deviceID[0]) {
    case FC_DS18B20:
        return "DS18B20";
    case FC_DS18S20:
        return "DS18S20";
    case FC_DS1822:
        return "DS1822";
    default:
        return "unknown";
    }
}

bool unitemp_onewire_sensor_readID(OneWireSensor* instance) {
    if(!unitemp_onewire_bus_start(instance->bus)) return false;
    unitemp_onewire_bus_send_byte(instance->bus, 0x33); //Reading ROM
    unitemp_onewire_bus_read_byteArray(instance->bus, instance->deviceID, 8);
    if(!unitemp_onewire_CRC_check(instance->deviceID, 8)) {
        memset(instance->deviceID, 0, 8);
        return false;
    }
    instance->familyCode = instance->deviceID[0];
    return true;
}

void unitemp_onewire_bus_enum_init(void) {
    for(uint8_t p = 0; p < 8; p++) {
        onewire_enum[p] = 0;
    }
    onewire_enum_fork_bit = 65; //to the right of the right
}

uint8_t* unitemp_onewire_bus_enum_next(OneWireBus* bus) {
    furi_delay_ms(10);
    if(!onewire_enum_fork_bit) { //If there were no disagreements at the previous step
        UNITEMP_DEBUG("All devices on wire %d is found", unitemp_gpio_toInt(bus->gpio));
        return 0; //then we just leave without returning anything
    }
    if(!unitemp_onewire_bus_start(bus)) {
        UNITEMP_DEBUG("Wire %d is empty", unitemp_gpio_toInt(bus->gpio));
        return 0;
    }
    uint8_t bp = 8;
    uint8_t* pprev = &onewire_enum[0];
    uint8_t prev = *pprev;
    uint8_t next = 0;

    uint8_t p = 1;
    unitemp_onewire_bus_send_byte(bus, 0xF0);
    uint8_t newfork = 0;
    for(;;) {
        uint8_t not0 = unitemp_onewire_bus_read_bit(bus);
        uint8_t not1 = unitemp_onewire_bus_read_bit(bus);
        if(!not0) { //If bit zero is present in the addresses
            if(!not1) { //But bit 1 (fork) is also present
                if(p <
                   onewire_enum_fork_bit) { //If we are to the left of the past right conflict bit,
                    if(prev & 1) {
                        next |= 0x80; //then copy the bit value from the previous pass
                    } else {
                        newfork = p; //if zero, then remember the conflict location
                    }
                } else if(p == onewire_enum_fork_bit) {
                    next |=
                        0x80; //if at this place last time there was a right conflict with zero, print 1
                } else {
                    newfork =
                        p; //to the right - we transmit zero and remember the conflict location
                }
            } //otherwise we go, choosing zero in the address
        } else {
            if(!not1) { //Unit present
                next |= 0x80;
            } else { //There are no zeros or ones - an erroneous situation

                UNITEMP_DEBUG("Wrong wire %d situation", unitemp_gpio_toInt(bus->gpio));
                return 0;
            }
        }
        unitemp_onewire_bus_send_bit(bus, next & 0x80);
        bp--;
        if(!bp) {
            *pprev = next;
            if(p >= 64) break;
            next = 0;
            pprev++;
            prev = *pprev;
            bp = 8;
        } else {
            if(p >= 64) break;
            prev >>= 1;
            next >>= 1;
        }
        p++;
    }
    onewire_enum_fork_bit = newfork;
    return &onewire_enum[0];
}

void unitemp_onewire_bus_select_sensor(OneWireSensor* instance) {
    unitemp_onewire_bus_send_byte(instance->bus, 0x55);
    unitemp_onewire_bus_send_byteArray(instance->bus, instance->deviceID, 8);
}

bool unitemp_onewire_sensor_alloc(Sensor* sensor, char* args) {
    OneWireSensor* instance = malloc(sizeof(OneWireSensor));
    if(instance == NULL) {
        FURI_LOG_E(APP_NAME, "Sensor %s instance allocation error", sensor->name);
        return false;
    }
    sensor->instance = instance;
    //Address clearing
    memset(instance->deviceID, 0, 8);

    int gpio, addr_0, addr_1, addr_2, addr_3, addr_4, addr_5, addr_6, addr_7;
    sscanf(
        args,
        "%d %2X%2X%2X%2X%2X%2X%2X%2X",
        &gpio,
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

    instance->familyCode = instance->deviceID[0];

    instance->bus = unitemp_onewire_bus_alloc(unitemp_gpio_getFromInt(gpio));

    if(instance != NULL) {
        return true;
    }
    FURI_LOG_E(APP_NAME, "Sensor %s bus allocation error", sensor->name);
    free(instance);
    return false;
}

bool unitemp_onewire_sensor_free(Sensor* sensor) {
    if(((OneWireSensor*)sensor->instance)->bus != NULL) {
        if(((OneWireSensor*)sensor->instance)->bus->device_count == 0) {
            free(((OneWireSensor*)sensor->instance)->bus);
        }
    }

    free(sensor->instance);

    return true;
}

bool unitemp_onewire_sensor_init(Sensor* sensor) {
    OneWireSensor* instance = sensor->instance;
    if(instance == NULL || instance->bus == NULL) {
        FURI_LOG_E(APP_NAME, "Sensor pointer is null!");
        return false;
    }

    unitemp_onewire_bus_init(instance->bus);
    furi_delay_ms(1);

    if(instance->familyCode == FC_DS18B20 || instance->familyCode == FC_DS1822) {
        //Setting the bit depth to 10 bits
        if(!unitemp_onewire_bus_start(instance->bus)) return false;
        unitemp_onewire_bus_select_sensor(instance);
        unitemp_onewire_bus_send_byte(instance->bus, 0x4E); //Memory recording
        uint8_t buff[3];
        //Alarm values
        buff[0] = 0x4B; //Temperature lower limit value
        buff[1] = 0x46; //Upper temperature limit value
        //Configuration
        buff[2] = 0b01111111; //12 bit bit conversion
        unitemp_onewire_bus_send_byteArray(instance->bus, buff, 3);

        //Stores values ​​in EEPROM for automatic recovery after power failures
        if(!unitemp_onewire_bus_start(instance->bus)) return false;
        unitemp_onewire_bus_select_sensor(instance);
        unitemp_onewire_bus_send_byte(instance->bus, 0x48); //Write to EEPROM
    }

    return true;
}

bool unitemp_onewire_sensor_deinit(Sensor* sensor) {
    OneWireSensor* instance = sensor->instance;
    if(instance == NULL || instance->bus == NULL) return false;
    unitemp_onewire_bus_deinit(instance->bus);

    return true;
}

UnitempStatus unitemp_onewire_sensor_update(Sensor* sensor) {
    //Removing the special status from the sensor in passive power mode
    if(sensor->status == UT_SENSORSTATUS_EARLYPOOL) {
        return UT_SENSORSTATUS_POLLING;
    }

    OneWireSensor* instance = sensor->instance;
    uint8_t buff[9] = {0};
    if(sensor->status != UT_SENSORSTATUS_POLLING) {
        //If the sensor did not respond last time, check its presence on the tire
        if(sensor->status == UT_SENSORSTATUS_TIMEOUT || sensor->status == UT_SENSORSTATUS_BADCRC) {
            if(!unitemp_onewire_bus_start(instance->bus)) return UT_SENSORSTATUS_TIMEOUT;
            unitemp_onewire_bus_select_sensor(instance);
            unitemp_onewire_bus_send_byte(instance->bus, 0xBE); // Read Scratch-pad
            unitemp_onewire_bus_read_byteArray(instance->bus, buff, 9);
            if(!unitemp_onewire_CRC_check(buff, 9)) {
                UNITEMP_DEBUG("Sensor %s is not found", sensor->name);
                return UT_SENSORSTATUS_TIMEOUT;
            }
        }

        if(!unitemp_onewire_bus_start(instance->bus)) return UT_SENSORSTATUS_TIMEOUT;
        //Starting conversion on all sensors in passive power mode
        if(instance->bus->powerMode == PWR_PASSIVE) {
            unitemp_onewire_bus_send_byte(instance->bus, 0xCC); // skip addr
            //Setting a special status on all sensors of this bus so as not to start the conversion again
            for(uint8_t i = 0; i < unitemp_sensors_getActiveCount(); i++) {
                if(unitemp_sensor_getActive(i)->type->interface == &ONE_WIRE &&
                   ((OneWireSensor*)unitemp_sensor_getActive(i)->instance)->bus == instance->bus) {
                    unitemp_sensor_getActive(i)->status = UT_SENSORSTATUS_EARLYPOOL;
                }
            }

        } else {
            unitemp_onewire_bus_select_sensor(instance);
        }

        unitemp_onewire_bus_send_byte(instance->bus, 0x44); // convert t
        if(instance->bus->powerMode == PWR_PASSIVE) {
            furi_hal_gpio_write(instance->bus->gpio->pin, true);
            furi_hal_gpio_init(
                instance->bus->gpio->pin, GpioModeOutputPushPull, GpioPullUp, GpioSpeedVeryHigh);
        }
        return UT_SENSORSTATUS_POLLING;
    } else {
        if(instance->bus->powerMode == PWR_PASSIVE) {
            furi_hal_gpio_write(instance->bus->gpio->pin, true);
            furi_hal_gpio_init(
                instance->bus->gpio->pin, GpioModeOutputOpenDrain, GpioPullUp, GpioSpeedVeryHigh);
        }
        if(!unitemp_onewire_bus_start(instance->bus)) return UT_SENSORSTATUS_TIMEOUT;
        unitemp_onewire_bus_select_sensor(instance);
        unitemp_onewire_bus_send_byte(instance->bus, 0xBE); // Read Scratch-pad
        unitemp_onewire_bus_read_byteArray(instance->bus, buff, 9);
        if(!unitemp_onewire_CRC_check(buff, 9)) {
            UNITEMP_DEBUG("Failed CRC check: %s", sensor->name);
            return UT_SENSORSTATUS_BADCRC;
        }
        int16_t raw = buff[0] | ((int16_t)buff[1] << 8);
        if(instance->familyCode == FC_DS18S20) {
            //Pseudo-12-bit.
            //sensor->temp = ((float)raw / 2.0f) - 0.25f + (16.0f - buff[6]) / 16.0f;
            //Honest 9 bits
            sensor->temp = ((float)raw / 2.0f);
        } else {
            sensor->temp = (float)raw / 16.0f;
        }
    }

    return UT_SENSORSTATUS_OK;
}

bool unitemp_onewire_id_compare(uint8_t* id1, uint8_t* id2) {
    if(id1 == NULL || id2 == NULL) return false;
    for(uint8_t i = 0; i < 8; i++) {
        if(id1[i] != id2[i]) return false;
    }
    return true;
}
