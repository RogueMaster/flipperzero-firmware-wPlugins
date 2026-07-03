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

#include "onewire_sensor.h"
#include "./sensors/DS18x2x.h"
#include <furi.h>
#include <furi_hal.h>

const SensorConnectionInterface unitemp_1w = {
    .name = "1-wire",
    .allocator = unitemp_ds18x2x_sensor_alloc,
    .mem_releaser = unitemp_ds18x2x_sensor_free,
    .updater = unitemp_ds18x2x_sensor_update};

UnitempOneWireBus* unitemp_onewire_bus_alloc(const SensorGpioPin* gpio_pin) {
    if(gpio_pin == NULL) {
        return NULL;
    }

    //Checking for bus presence on this port
    for(uint8_t i = 0; i < unitemp_sensors_get_count(); i++) {
        if(unitemp_sensors_get(i)->model->interface == &unitemp_1w &&
           ((OneWireSensor*)unitemp_sensors_get(i)->instance)->bus->bus_pin == gpio_pin) {
            //If there is already a bus on this port, then return a pointer to the bus
            return ((OneWireSensor*)unitemp_sensors_get(i)->instance)->bus;
        }
    }

    UnitempOneWireBus* bus = malloc(sizeof(UnitempOneWireBus));
    bus->bus_pin = gpio_pin;
    bus->host = onewire_host_alloc(gpio_pin->pin);
    bus->devices_count = 0;
    UNITEMP_DEBUG("one wire bus (port %d) allocated", gpio_pin->num);

    return bus;
}

void unitemp_onewire_bus_free(UnitempOneWireBus* unitemp_one_wire_bus) {
    if(unitemp_one_wire_bus != NULL) {
        if(unitemp_one_wire_bus->devices_count == 0) {
            if(unitemp_one_wire_bus->host != NULL) onewire_host_free(unitemp_one_wire_bus->host);
            free(unitemp_one_wire_bus);
        }
    }
}

bool unitemp_onewire_bus_init(UnitempOneWireBus* bus) {
    if(bus == NULL) return false;
    bus->devices_count++;
    UNITEMP_DEBUG("Device to bus %s added. Total: %d", bus->bus_pin->name, bus->devices_count);
    //Output if the bus has already been initialized
    if(bus->devices_count > 1) return true;

    unitemp_gpio_lock(bus->bus_pin, &unitemp_1w);

    onewire_host_start(bus->host);

    LL_GPIO_SetPinPull(bus->bus_pin->pin->port, bus->bus_pin->pin->pin, LL_GPIO_PULL_UP);
    furi_delay_ms(100);
    return true;
}

bool unitemp_onewire_bus_deinit(UnitempOneWireBus* bus) {
    if(bus->devices_count == 0) return true;
    bus->devices_count--;
    UNITEMP_DEBUG("Device removed from bus %s. Total: %d", bus->bus_pin->name, bus->devices_count);
    if(bus->devices_count == 0) {
        unitemp_gpio_unlock(bus->bus_pin);
        onewire_host_stop(bus->host);
        return true;
    } else {
        return false;
    }
}

bool unitemp_onewire_bus_start(UnitempOneWireBus* bus) {
    return onewire_host_reset(bus->host);
}

void unitemp_onewire_bus_select_device(UnitempOneWireBus* bus, uint8_t* device_id) {
    unitemp_onewire_bus_write(bus, 0x55);
    unitemp_onewire_bus_write_bytes(bus, device_id, 8);
}

void unitemp_onewire_bus_write(UnitempOneWireBus* bus, uint8_t data) {
    onewire_host_write(bus->host, data);
}

void unitemp_onewire_bus_write_bytes(UnitempOneWireBus* bus, uint8_t* data, uint8_t len) {
    onewire_host_write_bytes(bus->host, data, len);
}

void unitemp_onewire_bus_read_bytes(UnitempOneWireBus* bus, uint8_t* data, uint8_t len) {
    onewire_host_read_bytes(bus->host, data, len);
}

void unitemp_onewire_bus_strong_mode(UnitempOneWireBus* bus, bool state) {
    if(state) {
        furi_hal_gpio_write(bus->bus_pin->pin, true);
        furi_hal_gpio_init(
            bus->bus_pin->pin, GpioModeOutputPushPull, GpioPullUp, GpioSpeedVeryHigh);
    } else {
        furi_hal_gpio_write(bus->bus_pin->pin, true);
        furi_hal_gpio_init(
            bus->bus_pin->pin, GpioModeOutputOpenDrain, GpioPullUp, GpioSpeedVeryHigh);
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

bool unitemp_onewire_sensor_read_id(OneWireSensor* instance) {
    if(!unitemp_onewire_bus_start(instance->bus)) return false;
    unitemp_onewire_bus_write(instance->bus, 0x33); //Reading ROM
    unitemp_onewire_bus_read_bytes(instance->bus, instance->deviceID, 8);
    if(!unitemp_onewire_CRC_check(instance->deviceID, 8)) {
        memset(instance->deviceID, 0, 8);
        return false;
    }
    instance->family_code = instance->deviceID[0];
    return true;
}

char* unitemp_onewire_sensor_get_fc_name(Sensor* sensor) {
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

bool unitemp_onewire_id_compare(uint8_t* id1, uint8_t* id2) {
    if(id1 == NULL || id2 == NULL) return false;
    for(uint8_t i = 0; i < 8; i++) {
        if(id1[i] != id2[i]) return false;
    }
    return true;
}

bool unitemp_onewire_id_exist(uint8_t* id) {
    if(id == NULL) return false;
    for(uint8_t i = 0; i < unitemp_sensors_get_count(); i++) {
        if(unitemp_sensors_get(i)->model->interface == &unitemp_1w) {
            if(unitemp_onewire_id_compare(
                   id, ((OneWireSensor*)(unitemp_sensors_get(i)->instance))->deviceID)) {
                return true;
            }
        }
    }
    return false;
}

bool unitemp_onewire_scan(OneWireSensor* ow_sensor) {
    UNITEMP_DEBUG(
        "Devices on bus %d: %d", ow_sensor->bus->bus_pin->num, ow_sensor->bus->devices_count);

    //One wire bus scan
    unitemp_onewire_bus_init(ow_sensor->bus);
    unitemp_onewire_bus_strong_mode(ow_sensor->bus, false);

    uint8_t* id = NULL;
    do {
        do {
            id = unitemp_onewire_bus_enum_next(ow_sensor->bus);
        } while(unitemp_onewire_id_exist(id));

        if(id == NULL) {
            unitemp_onewire_bus_enum_init();
            id = unitemp_onewire_bus_enum_next(ow_sensor->bus);
            if(unitemp_onewire_id_exist(id)) {
                do {
                    id = unitemp_onewire_bus_enum_next(ow_sensor->bus);
                } while(unitemp_onewire_id_exist(id) && id != NULL);
            }
            if(id == NULL) {
                memset(ow_sensor->deviceID, 0, 8);
                ow_sensor->family_code = 0;
                break;
            }
        }
    } while(0);

    unitemp_onewire_bus_deinit(ow_sensor->bus);
    if(id == NULL) return false;

    memcpy(ow_sensor->deviceID, id, 8);
    ow_sensor->family_code = id[0];

    UNITEMP_DEBUG(
        "Found sensor's ID: %02X%02X%02X%02X%02X%02X%02X%02X",
        id[0],
        id[1],
        id[2],
        id[3],
        id[4],
        id[5],
        id[6],
        id[7]);

    if(ow_sensor->family_code != 0) {
        return true;
    }
    return false;
}

//Variables for storing the intermediate result of a bus scan
//found eight-byte address
static uint8_t onewire_enum[8] = {0};
//the last zero bit where there was ambiguity (numbering from one)
static uint8_t onewire_enum_fork_bit = 65;
void unitemp_onewire_bus_enum_init(void) {
    for(uint8_t p = 0; p < 8; p++) {
        onewire_enum[p] = 0;
    }
    onewire_enum_fork_bit = 65; //to the right of the right
}

uint8_t* unitemp_onewire_bus_enum_next(UnitempOneWireBus* bus) {
    if(!onewire_enum_fork_bit) { //If there were no disagreements at the previous step
        UNITEMP_DEBUG("All devices on wire %s is found", bus->bus_pin->name);
        return 0; //then we just leave without returning anything
    }
    if(!unitemp_onewire_bus_start(bus)) {
        UNITEMP_DEBUG("Wire %s is empty", bus->bus_pin->name);
        return 0;
    }
    uint8_t bp = 8;
    uint8_t* pprev = &onewire_enum[0];
    uint8_t prev = *pprev;
    uint8_t next = 0;

    uint8_t p = 1;
    unitemp_onewire_bus_write(bus, 0xF0);
    uint8_t newfork = 0;
    for(;;) {
        uint8_t not0 = onewire_host_read_bit(bus->host);
        uint8_t not1 = onewire_host_read_bit(bus->host);
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

                UNITEMP_DEBUG("Wrong wire %s situation", bus->bus_pin->name);
                return 0;
            }
        }
        onewire_host_write_bit(bus->host, next & 0x80);
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
