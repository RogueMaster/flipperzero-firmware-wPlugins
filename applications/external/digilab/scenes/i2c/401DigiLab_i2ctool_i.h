/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */

#ifndef _401DIGILAB_SCENE_I2CTOOL_I_H
#define _401DIGILAB_SCENE_I2CTOOL_I_H
#include <401_err.h>
#include "board.h"
#include <devicehelpers.h>

#define I2CDEV_LOWEST_ADDR     8
#define I2CDEV_HIGHEST_ADDR    127
#define I2CDEV_SPAN_ADDR       120
#define I2CDEV_MAX_SCAN_LENGHT 20
#define I2CDEV_MAX_OPTIONS     10

typedef struct {
    char* partno;
    char* desc;
    char* manufacturer;
    char* date;
    device_type_t type;
} i2cdevice_option_t;

typedef struct {
    uint8_t addr;
    i2cdevice_option_t options_map[I2CDEV_MAX_OPTIONS];
    uint8_t options;
    uint8_t option_selected;
    device_type_t* types_map;
    uint8_t types;
    bool known;
    bool loaded;
} i2cdevice_t;

#endif /* end of include guard: _401DIGILAB_SCENE_I2CTOOL_I_H */
