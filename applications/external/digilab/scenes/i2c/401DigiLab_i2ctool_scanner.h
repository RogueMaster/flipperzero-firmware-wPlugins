/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#ifndef _401DIGILAB_SCENE_I2CTOOLSCANNER_H
#define _401DIGILAB_SCENE_I2CTOOLSCANNER_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <furi_hal_i2c.h>
#include <gui/gui.h>
//#include <gui/gui_i.h>
#include <gui/modules/variable_item_list.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <locale/locale.h>
#include <scenes/i2c/401DigiLab_i2ctool_i.h>
#include <app_params.h> // Application specific i2ctoolscanneruration
#include <devicehelpers.h>

typedef enum {
    I2C_VIEW_ADDR,
    I2C_VIEW_DETAIL,
    I2C_VIEW_COUNT,
    I2C_VIEW_SCAN,
    I2C_VIEW_UNKNOWN,
} i2cToolScannerView;

typedef struct {
    i2cdevice_t* devices_map;
    uint8_t devices;
    uint8_t device_selected;
    i2cToolScannerView screenview;
} i2CToolScannerModel;

typedef struct {
    View* view;
    i2CToolScannerModel* model;
} AppI2CToolScanner;

#include "401DigiLab_main.h"

AppI2CToolScanner* app_i2ctoolscanner_alloc();
void app_i2ctoolscanner_free(AppI2CToolScanner* appI2CToolScanner);
View* app_i2ctoolscanner_get_view(AppI2CToolScanner* appI2CToolScanner);
void app_i2ctoolscanner_render_callback(Canvas* canvas, void* ctx);
bool app_i2ctoolscanner_input_callback(InputEvent* input_event, void* ctx);
void app_scene_i2ctoolscanner_on_enter(void* ctx);
bool app_scene_i2ctoolscanner_on_event(void* ctx, SceneManagerEvent event);
void app_scene_i2ctoolscanner_on_exit(void* ctx);
#endif
