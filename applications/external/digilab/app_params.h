/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#ifndef APP_PARAMS_H_
#define APP_PARAMS_H_

#include <fonts/fonts.h>
// Digilab file and folder configurations
#define DIGILABCONF_APPS_DATA_FOLDER EXT_PATH("apps_assets")
#define DIGILABCONF_APP_ID           "401_digilab"
#define DIGILABCONF_SAVE_FOLDER      DIGILABCONF_APPS_DATA_FOLDER "/" DIGILABCONF_APP_ID
#define DIGILABCONF_SAVE_EXTENSION   ".json"
#define DIGILABCONF_CONFIG_FILE      DIGILABCONF_SAVE_FOLDER "/config" DIGILABCONF_SAVE_EXTENSION
#define DIGILABCONF_CALIBRATION_FILE DIGILABCONF_SAVE_FOLDER "/calibration"

#define DIGILABCONF_I2CDEV_ADDR_FILE_FMT DIGILABCONF_SAVE_FOLDER "/targets/%02X.json"
#define DIGILABCONF_SPIDEV_TESTS         DIGILABCONF_SAVE_FOLDER "/targets/spidev.json"

#define OSC_SCALE_V   1000
#define OSC_SCALE_MV  100
#define OSC_SCALE_Vin 100

#define SPI_MEM_SPI_TIMEOUT      500
#define I2C_MEM_I2C_TIMEOUT      500
#define CUSTOM_FONT_TINY_BOLD    u8g2_font_courB08_tf
#define CUSTOM_FONT_TINY_REGULAR u8g2_font_tom_thumb_4x6_mr
#define CUSTOM_FONT_HUGE_REGULAR u8g2_font_logisoso20_tr
#endif /* end of include guard: APP_PARAMS_H_ */
