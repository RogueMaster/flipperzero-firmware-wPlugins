#include <furi.h>
#include <furi_hal.h>

#include "PT2257.h"

#define TIMEOUT_MS 100

// NOTE: This driver uses an 8-bit I2C address byte (already left-shifted, like TEA5767_ADR).
// The Arduino reference library uses 0x88 as the PT2257 chip address byte.
#ifndef PT2257_I2C_ADDR_DEFAULT
#define PT2257_I2C_ADDR_DEFAULT 0x88
#endif

// Instruction base codes (from victornpb/evc_pt2257)
#define PT2257_EVC_OFF 0xFF

#define PT2257_EVC_2CH_1  0b11010000
#define PT2257_EVC_2CH_10 0b11100000

#define PT2257_EVC_L_1  0b10100000
#define PT2257_EVC_L_10 0b10110000

#define PT2257_EVC_R_1  0b00100000
#define PT2257_EVC_R_10 0b00110000

#define PT2257_EVC_MUTE 0b01111000

static uint8_t pt2257_i2c_addr = PT2257_I2C_ADDR_DEFAULT;

void pt2257_set_i2c_addr(uint8_t addr) {
    pt2257_i2c_addr = addr;
}

uint8_t pt2257_get_i2c_addr(void) {
    return pt2257_i2c_addr;
}

static bool pt2257_acquire_i2c(void) {
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    return furi_hal_i2c_is_device_ready(&furi_hal_i2c_handle_external, pt2257_i2c_addr, 5);
}

static void pt2257_release_i2c(void) {
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
}

static uint8_t pt2257_pack_level(uint8_t attenuation_db) {
    if(attenuation_db > 79) attenuation_db = 79;

    uint8_t tens = attenuation_db / 10; // 0..7
    uint8_t ones = attenuation_db % 10; // 0..9

    tens &= 0b00000111;
    return (uint8_t)((tens << 4) | ones); // 0BBBAAAA
}

static bool pt2257_write_bytes(const uint8_t* data, size_t len) {
    if(!data || len == 0) return false;

    bool ok = pt2257_acquire_i2c();
    if(ok) {
        ok =
            furi_hal_i2c_tx(&furi_hal_i2c_handle_external, pt2257_i2c_addr, data, len, TIMEOUT_MS);
    }
    pt2257_release_i2c();
    return ok;
}

bool pt2257_is_device_ready(void) {
    bool ok = pt2257_acquire_i2c();
    pt2257_release_i2c();
    return ok;
}

bool pt2257_set_attenuation_db(uint8_t attenuation_db) {
    uint8_t packed = pt2257_pack_level(attenuation_db);
    uint8_t ones = packed & 0x0F;
    uint8_t tens = (packed >> 4) & 0x0F;

    uint8_t msg[2] = {
        (uint8_t)(PT2257_EVC_2CH_10 | tens),
        (uint8_t)(PT2257_EVC_2CH_1 | ones),
    };
    return pt2257_write_bytes(msg, sizeof(msg));
}

bool pt2257_set_attenuation_left_db(uint8_t attenuation_db) {
    uint8_t packed = pt2257_pack_level(attenuation_db);
    uint8_t ones = packed & 0x0F;
    uint8_t tens = (packed >> 4) & 0x0F;

    uint8_t msg[2] = {
        (uint8_t)(PT2257_EVC_L_10 | tens),
        (uint8_t)(PT2257_EVC_L_1 | ones),
    };
    return pt2257_write_bytes(msg, sizeof(msg));
}

bool pt2257_set_attenuation_right_db(uint8_t attenuation_db) {
    uint8_t packed = pt2257_pack_level(attenuation_db);
    uint8_t ones = packed & 0x0F;
    uint8_t tens = (packed >> 4) & 0x0F;

    uint8_t msg[2] = {
        (uint8_t)(PT2257_EVC_R_10 | tens),
        (uint8_t)(PT2257_EVC_R_1 | ones),
    };
    return pt2257_write_bytes(msg, sizeof(msg));
}

bool pt2257_mute(bool enable) {
    uint8_t msg[1] = {(uint8_t)(PT2257_EVC_MUTE | (enable ? 1 : 0))};
    return pt2257_write_bytes(msg, sizeof(msg));
}

bool pt2257_off(void) {
    uint8_t msg[1] = {PT2257_EVC_OFF};
    return pt2257_write_bytes(msg, sizeof(msg));
}
