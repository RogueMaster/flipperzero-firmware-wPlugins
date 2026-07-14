/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#ifndef APP_PARAMS_H_
#define APP_PARAMS_H_

// General Light Message configurations
#define LIGHTMSG_LED_ROWS           16
#define LIGHTMSG_MAX_TEXT_LEN       64
#define LIGHTMSG_MAX_BITMAPPATH_LEN 128
#define LIGHTMSG_MAX_BITMAPNAME_LEN 32

// SK6805 LED configurations
#define SK6805_LED_COUNT      LIGHTMSG_LED_ROWS // Number of LEDs on the backlight board
#define SK6805_LED_PIN        &led_pin // Connection port for LEDs
#define LED_MAX_BRIGHTNESS    32
#define LED_OFFSET_BRIGHTNESS (LED_MAX_BRIGHTNESS >> 1)

// Light Message file and folder configurations
#define LIGHTMSGCONF_APPS_DATA_FOLDER EXT_PATH("apps_assets")
#define LIGHTMSGCONF_APP_ID           "401_light_msg"
#define LIGHTMSGCONF_SAVE_FOLDER      LIGHTMSGCONF_APPS_DATA_FOLDER "/" LIGHTMSGCONF_APP_ID
#define LIGHTMSGCONF_SAVE_EXTENSION   ".json"
#define LIGHTMSGCONF_CONFIG_FILE      LIGHTMSGCONF_SAVE_FOLDER "/config" LIGHTMSGCONF_SAVE_EXTENSION

// Interrupt pins configuration
#define LGHTMSG_INT1PIN &gpio_ext_pa4 // Header pin 4 = INT1
#define LGHTMSG_INT2PIN &gpio_swclk // Header pin 10 = INT2

// LIS2DH12 accelerometer configurations
#define PARAM_INT1_DURATION           0x10
#define PARAM_INT2_DURATION           0x10
#define PARAM_INT1_THRESHOLD          60
#define PARAM_INT2_THRESHOLD          60
#define PARAM_INT_POLARITY            PROPERTY_DISABLE
#define PARAM_ACCELEROMETER_HIGH_PASS LIS2DH12_LIGHT // High Pass Filter
#define PARAM_ACCELEROMETER_SCALE     LIS2DH12_8g // Set scale to +/-2g
#define PARAM_OUTPUT_DATA_RATE        LIS2DH12_ODR_5kHz376_LP_1kHz344_NM_HP // Set output data rate
#define PARAM_OPERATION_MODE          LIS2DH12_HR_12bit // Set operation mode to high-resolution mode

// Additional configurations
#define PARAM_SHADER_RAINBOW_IS_SINE 0
#define PARAM_BRIGHTNESS_CONFIG_EN   0

#define PARAM_BMP_EDITOR_MIN_RES_W 16
#define PARAM_BMP_EDITOR_MAX_RES_W 41
#define PARAM_BMP_EDITOR_MIN_RES_H 16
#define PARAM_BMP_EDITOR_MAX_RES_H 16
#endif /* end of include guard: APP_PARAMS_H_ */
