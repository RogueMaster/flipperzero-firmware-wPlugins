#!/usr/bin/env python3
"""Host-side checks of the real SCD4x driver with a simulated I2C bus.

Run with python3 tools/test_scd4x.py (requires a C compiler).
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
# Reuse the production sensor structures while replacing platform headers.
types = (ROOT / "sensors/unitemp/Sensors.h").read_text().split("/* Interface constants */")[0]
types = re.sub(r"^#include .*", "", types, flags=re.MULTILINE) + "\n#endif\n"
source = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#define UNITEMP
#define UNITEMP_I2C
#define UNUSED(x) (void)(x)
#define FURI_LOG_E(...) ((void)0)
#define FURI_LOG_I(...) ((void)0)
typedef struct { int unused; } GpioPin;
#include "sensor_types.h"
typedef struct { uint8_t minI2CAdr, maxI2CAdr; } I2CSensor;
const Interface I2C = {0};
static uint32_t ticks;
static uint16_t command;
static uint16_t commands[8];
static unsigned command_count;
static bool bus_ok = true, read_ok = true, ready = false;
static bool corrupt_ready = false;
static int corrupt_word = -1;
static uint16_t sample_co2 = 500;
static void furi_delay_ms(uint32_t ms) { ticks += ms; }
static uint32_t furi_get_tick(void) { return ticks; }
static bool unitemp_i2c_writeArray(I2CSensor* s, uint8_t n, uint8_t* b) {
    (void)s;
    assert(n == 2 || n == 5);
    command = ((uint16_t)b[0] << 8) | b[1];
    if(command_count < 8) commands[command_count++] = command;
    return bus_ok;
}
static bool unitemp_i2c_readArray(I2CSensor*, uint8_t, uint8_t*);
#include "DRIVER_PATH"
static void pack(uint8_t* b, uint16_t value) {
    b[0] = value >> 8;
    b[1] = value;
    b[2] = computeCRC8(b, 2);
}
static bool unitemp_i2c_readArray(I2CSensor* s, uint8_t n, uint8_t* b) {
    (void)s;
    if(!bus_ok || !read_ok) return false;
    if(command == COMMAND_GET_DATA_READY_STATUS) {
        assert(n == 3);
        pack(b, ready ? 0x8001 : 0x8000);
        if(corrupt_ready) b[2] ^= 1;
    } else {
        assert(command == COMMAND_READ_MEASUREMENT && n == 9);
        // Values match the datasheet's example; CRCs follow section 3.11.
        const uint8_t reference[] = {0x01, 0xf4, 0x33, 0x66, 0x67, 0xa2, 0x5e, 0xb9, 0x3c};
        memcpy(b, reference, sizeof(reference));
        if(sample_co2 != 500) pack(b, sample_co2);
        if(corrupt_word >= 0) b[corrupt_word * 3 + 2] ^= 1;
    }
    return true;
}
int main(void) {
    uint8_t crc_reference[] = {0xbe, 0xef};
    assert(computeCRC8(crc_reference, 2) == 0x92);
    I2CSensor bus = {0};
    Sensor sensor = {.instance = &bus, .co2 = -1};
    assert(unitemp_SCD40_alloc(&sensor, ""));
    assert(bus.minI2CAdr == 0xc4 && bus.maxI2CAdr == 0xc4);
    assert(unitemp_SCD40_init(&sensor));
    assert(commands[0] == 0x3f86 && commands[1] == 0x21b1);
    assert(ticks == 500 && sensor.status == UT_SENSORSTATUS_POLLING);
    // Waiting for a sample must not overwrite the previous values.
    assert(unitemp_SCD40_update(&sensor) == UT_SENSORSTATUS_POLLING);
    assert(sensor.co2 == -1);
    ready = true;
    assert(unitemp_SCD40_update(&sensor) == UT_SENSORSTATUS_OK);
    assert(sensor.co2 == 500 && fabsf(sensor.temp - 25) < 0.01f);
    assert(fabsf(sensor.hum - 37) < 0.01f);
    assert(sensor.last_valid_tick == ticks);
    float last_temp = sensor.temp, last_hum = sensor.hum;
    uint32_t last_valid = sensor.last_valid_tick;
    sample_co2 = 900;
    for(corrupt_word = 0; corrupt_word < 3; corrupt_word++) {
        assert(unitemp_SCD40_update(&sensor) == UT_SENSORSTATUS_ERROR);
        assert(sensor.co2 == 500 && sensor.temp == last_temp && sensor.hum == last_hum);
        assert(sensor.last_valid_tick == last_valid);
    }
    corrupt_word = -1;
    sample_co2 = 0;
    assert(unitemp_SCD40_update(&sensor) == UT_SENSORSTATUS_ERROR);
    assert(sensor.co2 == 500);
    sample_co2 = 500;
    corrupt_ready = true;
    assert(unitemp_SCD40_update(&sensor) == UT_SENSORSTATUS_ERROR);
    assert(sensor.last_valid_tick == last_valid);
    corrupt_ready = false;
    read_ok = false;
    assert(unitemp_SCD40_update(&sensor) == UT_SENSORSTATUS_ERROR);
    assert(sensor.co2 == 500);
    read_ok = true;
    bus_ok = false;
    assert(unitemp_SCD40_update(&sensor) == UT_SENSORSTATUS_ERROR);
    assert(sensor.co2 == 500);
    assert(!unitemp_SCD40_init(&sensor));
    puts("SCD4x: startup, not-ready, datasheet sample, CRC failures, zero sample and I2C read/write failures passed");
}
'''
source = source.replace("DRIVER_PATH", str(ROOT / "sensors/unitemp/sensors/SCD40.c"))
with tempfile.TemporaryDirectory(prefix="air-stats-test-") as directory:
    work = Path(directory)
    (work / "sensor_types.h").write_text(types)
    (work / "test.c").write_text(source)
    subprocess.run(["cc", "-std=c11", "-I", str(work), str(work / "test.c"),
                    "-o", str(work / "test"), "-lm"], check=True)
    subprocess.run([str(work / "test")], check=True)
