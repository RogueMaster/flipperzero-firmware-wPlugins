#include "live_test.h"
#include "i2c_worker.h"

#include "live_adxl345.h"
#include "live_aht.h"
#include "live_apds9960.h"
#include "live_bh1750.h"
#include "live_bno055.h"
#include "live_ds3231.h"
#include "live_mlx90614.h"
#include "live_mpu6050.h"
#include "live_sht.h"
#include "live_ssd1306.h"
#include "live_vl6180x.h"

#include <furi.h>
#include <string.h>

// The one place the tests and the bus are wired together. Everything above
// this line is written against the pointer table; only this file knows the
// functions have names. That is what lets a test move to a .fal without a
// single character changing in it.
static const LiveTestI2c live_test_i2c_table = {
    .device_ready = i2c_worker_device_ready,
    .read_reg = i2c_worker_read_reg,
    .write_reg = i2c_worker_write_reg,
    .read_mem = i2c_worker_read_mem,
    .read_reg16_addr = i2c_worker_read_reg16_addr,
    .write_reg16_addr = i2c_worker_write_reg16_addr,
    .write_raw = i2c_worker_write_raw,
    .read_raw = i2c_worker_read_raw,
};

const LiveTestI2c* live_test_i2c(void) {
    return &live_test_i2c_table;
}

// The registry. One line per part. Order is display order in any future list;
// lookup is by chip name, so it does not otherwise matter.
static const LiveTest* const live_tests[] = {
    &live_test_adxl345,
    &live_test_aht,
    &live_test_apds9960,
    &live_test_bh1750,
    &live_test_bno055,
    &live_test_ds3231,
    &live_test_mlx90614,
    &live_test_mpu6050,
    &live_test_mpu6500,
    &live_test_mpu9250,
    &live_test_sht,
    &live_test_ssd1306,
    &live_test_vl6180x,
};

bool live_test_has_addr(const LiveTest* test, uint8_t addr7) {
    if(!test || addr7 == LIVE_TEST_ADDR_NONE) return false;
    for(size_t i = 0; i < LIVE_TEST_MAX_ADDRS; i++) {
        if(test->addrs[i] == LIVE_TEST_ADDR_NONE) break;
        if(test->addrs[i] == addr7) return true;
    }
    return false;
}

const LiveTest* live_test_for_chip(const char* chip_name) {
    if(!chip_name) return NULL;
    for(size_t i = 0; i < COUNT_OF(live_tests); i++) {
        if(strcmp(live_tests[i]->chip, chip_name) == 0) return live_tests[i];
    }
    return NULL;
}

size_t live_test_count(void) {
    return COUNT_OF(live_tests);
}

const LiveTest* live_test_get(size_t index) {
    if(index >= COUNT_OF(live_tests)) return NULL;
    return live_tests[index];
}
