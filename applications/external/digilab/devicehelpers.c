/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */

#include <devicehelpers.h>
#include <string.h>

const char* device_type_str[DEVICE_TYPE_COUNT] = {
    "ACC", "ADC",  "AMP",  "CAM",    "CLOCK", "CONVERTER", "COP",  "CTRL",
    "DAC", "DISP", "DIST", "DRIVER", "GAUGE", "GYRO",      "IMU",  "IOEXP",
    "MAG", "MEM",  "MUX",  "READER", "RX",    "SENSOR",    "TEMP", "TOUCH",
};

void device_getTypeStr(device_type_t type, char* str, size_t len) {
    if(type < DEVICE_TYPE_COUNT && str != NULL) {
        strncpy(str, device_type_str[type - 1], len - 1);
        str[len - 1] = '\0'; // Ensure null termination
    } else {
        // Handle invalid type or null pointer
        if(str != NULL) {
            strncpy(str, "Unknown", len - 1);
            str[len - 1] = '\0'; // Ensure null termination
        }
    }
}
