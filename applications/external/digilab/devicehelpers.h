/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#ifndef _DEVICEHELPERS_H_
#define _DEVICEHELPERS_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

typedef enum {
    DEVICE_TYPE_ACC = 1,
    DEVICE_TYPE_ADC = 2,
    DEVICE_TYPE_AMP = 3,
    DEVICE_TYPE_CAM = 4,
    DEVICE_TYPE_CLOCK = 5,
    DEVICE_TYPE_CONVERTER = 6,
    DEVICE_TYPE_COP = 7,
    DEVICE_TYPE_CTRL = 8,
    DEVICE_TYPE_DAC = 9,
    DEVICE_TYPE_DISP = 10,
    DEVICE_TYPE_DIST = 11,
    DEVICE_TYPE_DRIVER = 12,
    DEVICE_TYPE_GAUGE = 13,
    DEVICE_TYPE_GYRO = 14,
    DEVICE_TYPE_IMU = 15,
    DEVICE_TYPE_IOEXP = 16,
    DEVICE_TYPE_MAG = 17,
    DEVICE_TYPE_MEM = 18,
    DEVICE_TYPE_MUX = 19,
    DEVICE_TYPE_READER = 20,
    DEVICE_TYPE_RX = 21,
    DEVICE_TYPE_SENSOR = 22,
    DEVICE_TYPE_TEMP = 23,
    DEVICE_TYPE_TOUCH = 24,
    DEVICE_TYPE_COUNT
} device_type_t;

void device_getTypeStr(device_type_t type, char* str, size_t len);

#endif /* end of include guard: _DEVICEHELPERS_H_ */
