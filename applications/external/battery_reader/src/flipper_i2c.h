#ifndef FLIPPER_I2C_H
#define FLIPPER_I2C_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <furi_hal.h>

#define I2C_TIMEOUT_MS          200
#define I2C_SCAN_TIMEOUT_MS     10
#define SBS_WAKE_TIMEOUT_MS     200
#define SBS_WAKE_ATTEMPTS       3
#define SBS_I2C_ADDRESS         0x0B
#define I2C_SCAN_FIRST_ADDRESS  0x08
#define I2C_SCAN_LAST_ADDRESS   0x77
#define I2C_SCAN_MAX_DEVICES    (I2C_SCAN_LAST_ADDRESS - I2C_SCAN_FIRST_ADDRESS + 1)
#define BMP280_ADDRESS_LOW      0x76
#define BMP280_ADDRESS_HIGH     0x77
#define BMP280_CHIP_ID_REGISTER 0xD0
#define BMP280_CHIP_ID          0x58

typedef enum {
    I2C_OK = 0,
    I2C_ERROR_TIMEOUT,
    I2C_ERROR_NACK,
    I2C_ERROR_BUSY,
    I2C_ERROR_INVALID
} I2CResult;

typedef struct {
    uint8_t dev_addr;
    bool use_pec;
} BMSI2C;

void bms_i2c_init(BMSI2C* i2c, uint8_t addr);
void bms_i2c_set_pec(BMSI2C* i2c, bool enable);

I2CResult bms_i2c_write_byte(BMSI2C* i2c, uint8_t cmd, uint8_t value);
I2CResult bms_i2c_write_word_raw(BMSI2C* i2c, uint8_t cmd, uint16_t value);
I2CResult bms_i2c_read_word(BMSI2C* i2c, uint8_t cmd, uint16_t* value);
I2CResult bms_i2c_write_word(BMSI2C* i2c, uint8_t cmd, uint16_t value);
I2CResult
    bms_i2c_read_block(BMSI2C* i2c, uint8_t cmd, uint8_t* buf, uint8_t* len, uint8_t max_len);
I2CResult bms_i2c_write_block(BMSI2C* i2c, uint8_t cmd, uint8_t* data, uint8_t len);
I2CResult bms_i2c_scan(uint8_t* found_addresses, size_t capacity, size_t* found_count);
I2CResult bms_i2c_read_register(uint8_t address, uint8_t reg, uint8_t* value);
I2CResult
    bms_i2c_read_after_write(BMSI2C* i2c, uint8_t cmd, uint8_t* buf, uint8_t* len, uint8_t max_len);

#endif
