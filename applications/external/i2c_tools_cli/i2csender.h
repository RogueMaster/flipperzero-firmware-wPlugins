#pragma once

#include <furi.h>
#include <furi_hal.h>
#include "i2cscanner.h"

#define I2C_MAX_READ_LEN 32
#define I2C_MIN_READ_LEN 1

typedef enum {
    SENDER_MODE_READ,
    SENDER_MODE_WRITE,
} SenderMode;

typedef enum {
    DISPLAY_HEX,
    DISPLAY_ASCII,
} DisplayMode;

typedef struct {
    uint8_t address_idx;
    uint8_t value; // register address (used in both READ and WRITE)
    uint8_t recv[I2C_MAX_READ_LEN];
    uint8_t read_len; // 1..32 (READ mode)
    uint8_t write_data; // byte to write (WRITE mode)
    uint8_t recv_scroll; // byte offset for paginated display
    SenderMode mode;
    DisplayMode display;
    bool must_send;
    bool sended;
    bool error;

    i2cScanner* scanner;
} i2cSender;

void i2c_send(i2cSender* i2c_sender);

i2cSender* i2c_sender_alloc();
void i2c_sender_free(i2cSender* i2c_sender);
