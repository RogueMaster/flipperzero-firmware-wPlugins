#include <furi.h>
#include <furi_hal.h>

#include "PT2259.h"

#define TIMEOUT_MS 100
#define PT2259_I2C_ADDR_DEFAULT 0x88
#define PT2259_CLEAR_REGISTERS 0xF0
#define PT2259_ALL_MUTE_OFF 0x74
#define PT2259_ALL_MUTE_ON 0x77
#define PT2259_EVC_2CH_1 0xD0
#define PT2259_EVC_2CH_10 0xE0

static uint8_t pt2259_i2c_addr = PT2259_I2C_ADDR_DEFAULT;

void pt2259_set_i2c_addr(uint8_t addr) {
    pt2259_i2c_addr = addr;
}

uint8_t pt2259_get_i2c_addr(void) {
    return pt2259_i2c_addr;
}

static bool pt2259_acquire_i2c(void) {
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    return furi_hal_i2c_is_device_ready(&furi_hal_i2c_handle_external, pt2259_i2c_addr, 5);
}

static void pt2259_release_i2c(void) {
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
}

static bool pt2259_write_bytes(const uint8_t* data, size_t len) {
    if(!data || len == 0) return false;

    bool ok = pt2259_acquire_i2c();
    if(ok) {
        ok = furi_hal_i2c_tx(&furi_hal_i2c_handle_external, pt2259_i2c_addr, data, len, TIMEOUT_MS);
    }
    pt2259_release_i2c();
    return ok;
}

static uint8_t pt2259_clamp_attenuation(uint8_t attenuation_db) {
    return (attenuation_db > 79) ? 79 : attenuation_db;
}

bool pt2259_is_device_ready(void) {
    bool ok = pt2259_acquire_i2c();
    pt2259_release_i2c();
    return ok;
}

bool pt2259_init(void) {
    const uint8_t clear_msg[] = {PT2259_CLEAR_REGISTERS};
    const uint8_t init_msg[] = {PT2259_ALL_MUTE_OFF, PT2259_EVC_2CH_10, PT2259_EVC_2CH_1};

    if(!pt2259_write_bytes(clear_msg, sizeof(clear_msg))) return false;
    return pt2259_write_bytes(init_msg, sizeof(init_msg));
}

bool pt2259_set_attenuation_db(uint8_t attenuation_db) {
    attenuation_db = pt2259_clamp_attenuation(attenuation_db);

    uint8_t tens = attenuation_db / 10;
    uint8_t ones = attenuation_db % 10;
    uint8_t msg[] = {
        (uint8_t)(PT2259_EVC_2CH_10 | tens),
        (uint8_t)(PT2259_EVC_2CH_1 | ones),
    };

    return pt2259_write_bytes(msg, sizeof(msg));
}

bool pt2259_set_mute(bool enable) {
    const uint8_t msg[] = {enable ? PT2259_ALL_MUTE_ON : PT2259_ALL_MUTE_OFF};
    return pt2259_write_bytes(msg, sizeof(msg));
}
