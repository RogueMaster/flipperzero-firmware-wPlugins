// #pragma GCC optimize("Ofast")
// #pragma GCC optimize("-funroll-all-loops")
#pragma once
#include "ftdi_gpio.h"

//ftdi_gpio_set_b0 - CLK
//ftdi_gpio_set_b1 - MOSI
//ftdi_gpio_set_b2 - MISO
//ftdi_gpio_set_b3 - CS

static inline void ftdi_mpsse_data_write_bytes_nve_msb(uint8_t* data, uint32_t size) {
    uint8_t mask = 0x80;
    for(uint32_t i = 0; i < size; i++) {
        for(uint8_t j = 0; j < 8; j++) {
            ftdi_gpio_set_b1(data[i] & mask);
            ftdi_gpio_set_b0(1);
            data[i] <<= 1;
            ftdi_gpio_set_b0(0);
        }
    }
    ftdi_gpio_set_b0(0);
    ftdi_gpio_set_b1(0);
}

static inline void ftdi_mpsse_data_read_bytes_nve_msb(uint8_t* data, uint32_t size) {
    memset(data, 0, size);
    for(uint32_t i = 0; i < size; i++) {
        uint8_t mask = 0x80;
        for(uint8_t j = 0; j < 8; j++) {
            ftdi_gpio_set_b0(1);
            data[i] |= ftdi_gpio_get_b2() ? mask : 0;
            ftdi_gpio_set_b0(0);
            mask >>= 1;
        }
    }
}

static inline void ftdi_mpsse_data_write_bytes_nve_lsb(uint8_t* data, uint32_t size) {
    uint8_t mask = 0x01;
    for(uint32_t i = 0; i < size; i++) {
        for(uint8_t j = 0; j < 8; j++) {
            ftdi_gpio_set_b1(data[i] & mask);
            ftdi_gpio_set_b0(1);
            data[i] >>= 1;
            ftdi_gpio_set_b0(0);
        }
    }
    ftdi_gpio_set_b0(0);
    ftdi_gpio_set_b1(0);
}

static inline void ftdi_mpsse_data_read_bytes_nve_lsb(uint8_t* data, uint32_t size) {
    memset(data, 0, size);
    for(uint32_t i = 0; i < size; i++) {
        uint8_t mask = 0x01;
        for(uint8_t j = 0; j < 8; j++) {
            ftdi_gpio_set_b0(1);
            data[i] |= ftdi_gpio_get_b2() ? mask : 0;
            ftdi_gpio_set_b0(0);
            mask <<= 1;
        }
    }
}

static inline void ftdi_mpsse_data_write_bytes_pve_msb(uint8_t* data, uint32_t size) {
    uint8_t mask = 0x80;
    ftdi_gpio_set_b0(1);
    for(uint32_t i = 0; i < size; i++) {
        for(uint8_t j = 0; j < 8; j++) {
            ftdi_gpio_set_b1(data[i] & mask);
            ftdi_gpio_set_b0(0);
            data[i] <<= 1;
            ftdi_gpio_set_b0(1);
        }
    }
    ftdi_gpio_set_b0(1);
    ftdi_gpio_set_b1(0);
}

static inline void ftdi_mpsse_data_read_bytes_pve_msb(uint8_t* data, uint32_t size) {
    memset(data, 0, size);
    ftdi_gpio_set_b0(0);
    for(uint32_t i = 0; i < size; i++) {
        uint16_t mask = 0x80;
        for(uint8_t j = 0; j < 8; j++) {
            ftdi_gpio_set_b0(1);
            data[i] |= ftdi_gpio_get_b2() ? mask : 0;
            ftdi_gpio_set_b0(0);
            mask >>= 1;
        }
    }
    ftdi_gpio_set_b0(1);
}

static inline void ftdi_mpsse_data_write_bytes_pve_lsb(uint8_t* data, uint32_t size) {
    uint8_t mask = 0x01;
    ftdi_gpio_set_b0(1);
    for(uint32_t i = 0; i < size; i++) {
        for(uint8_t j = 0; j < 8; j++) {
            ftdi_gpio_set_b1(data[i] & mask);
            ftdi_gpio_set_b0(0);
            data[i] >>= 1;
            ftdi_gpio_set_b0(1);
        }
    }
    ftdi_gpio_set_b0(1);
    ftdi_gpio_set_b1(0);
}

static inline void ftdi_mpsse_data_read_bytes_pve_lsb(uint8_t* data, uint32_t size) {
    memset(data, 0, size);
    ftdi_gpio_set_b0(0);
    for(uint32_t i = 0; i < size; i++) {
        uint16_t mask = 0x01;
        for(uint8_t j = 0; j < 8; j++) {
            ftdi_gpio_set_b0(1);
            data[i] |= ftdi_gpio_get_b2() ? mask : 0;
            ftdi_gpio_set_b0(0);
            mask <<= 1;
        }
    }
    ftdi_gpio_set_b0(1);
}