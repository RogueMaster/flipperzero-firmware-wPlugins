/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */

#ifndef _401DIGILAB_SCENE_SPITOOL_I_H
#define _401DIGILAB_SCENE_SPITOOL_I_H

#include <401_err.h>
#include "board.h"
#include "devicehelpers.h"

#define SPIDEV_MAX_SCAN_LENGHT 20
#define SPIDEV_MAX_OPTIONS     10

typedef struct {
    char* partno;
    char* desc;
    char* manufacturer;
    char* date;
    device_type_t type;
} spidevice_t;

#endif /* end of include guard: _401DIGILAB_SCENE_SPITOOL_I_H */
