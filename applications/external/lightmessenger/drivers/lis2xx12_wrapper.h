#ifndef _LIS2XX_WRAPPER_H_
#define _LIS2XX_WRAPPER_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>
#include <string.h>

#include "../app_params.h"

#include <furi_hal.h>
#include <furi_hal_info.h>

#define I2C_BUS &furi_hal_i2c_handle_external

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)                                                                   \
    ((byte) & 0x80 ? '1' : '0'), ((byte) & 0x40 ? '1' : '0'), ((byte) & 0x20 ? '1' : '0'),     \
        ((byte) & 0x10 ? '1' : '0'), ((byte) & 0x08 ? '1' : '0'), ((byte) & 0x04 ? '1' : '0'), \
        ((byte) & 0x02 ? '1' : '0'), ((byte) & 0x01 ? '1' : '0')

int32_t lis2dh12_init(void* stmdev);
void lis2dh12_set_sensitivity(void* stmdev, uint8_t sensitivity);
int32_t platform_write(void* handle, uint8_t reg, const uint8_t* bufp, uint16_t len);
int32_t platform_read(void* handle, uint8_t reg, uint8_t* bufp, uint16_t len);

#endif /* end of include guard: _LIS2XX_WRAPPER_H_ */
