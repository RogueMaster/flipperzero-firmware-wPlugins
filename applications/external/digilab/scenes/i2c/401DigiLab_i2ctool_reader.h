/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#ifndef _401DIGILAB_SCENE_I2CTOOLREADER_H
#define _401DIGILAB_SCENE_I2CTOOLREADER_H

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <furi_hal_i2c.h>
#include <gui/gui.h>
//#include <gui/gui_i.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/number_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/popup.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <inttypes.h>
#include <locale/locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <scenes/i2c/401DigiLab_i2ctool_i.h>

#include "app_params.h" // Application specific i2ctoolreaderuration

#define I2C_BUS &furi_hal_i2c_handle_external

typedef enum {
    I2CReaderEventByteInput,
    I2CReaderEventNumberInput,
    I2CReaderEventExitPopup
} I2CReaderEvent;

typedef struct {
    i2cdevice_t* device;
    uint8_t* data;
    uint8_t data_len;
    uint8_t data_offset;
    uint8_t read_len;
    uint8_t read_offset;
} i2CToolReaderModel;

typedef struct {
    View* view;
    i2CToolReaderModel* model;
    ByteInput* InputReadOffset;
    NumberInput* InputReadLen;
    uint8_t addr[4];
    Popup* popup;
} AppI2CToolReader;

#include "401DigiLab_main.h"

AppI2CToolReader* app_i2ctoolreader_alloc();
void app_i2ctoolreader_free(void* ctx);
View* app_i2ctoolreader_get_view(AppI2CToolReader* appI2CToolReader);
void app_i2ctoolreader_render_callback(Canvas* canvas, void* ctx);
bool app_i2ctoolreader_input_callback(InputEvent* input_event, void* ctx);
void app_scene_i2ctoolreader_on_enter(void* ctx);
bool app_scene_i2ctoolreader_on_event(void* ctx, SceneManagerEvent event);
void app_scene_i2ctoolreader_on_exit(void* ctx);
#endif
