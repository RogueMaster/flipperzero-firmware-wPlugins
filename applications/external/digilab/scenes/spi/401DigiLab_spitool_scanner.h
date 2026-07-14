/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#ifndef _401DIGILAB_SCENE_SPITOOL_SCANNER_H
#define _401DIGILAB_SCENE_SPITOOL_SCANNER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <gui/modules/submenu.h>
#include <gui/scene_manager.h>
#include <furi_hal_spi_config.h>

#include <401_digilab_icons.h>
#include <locale/locale.h>
#include <scenes/spi/401DigiLab_spitool_i.h>
#include <app_params.h> // Application specific i2ctoolscanneruration
#include <devicehelpers.h>

typedef enum {
    SPI_VIEW_DETAIL,
    SPI_VIEW_COUNT,
    SPI_VIEW_SCAN,
    SPI_VIEW_UNKNOWN,
} spiToolScannerView;

typedef struct {
    spidevice_t* devices_map;
    uint8_t devices;
    uint8_t device_selected;
    spiToolScannerView screenview;
} SPIToolScannerModel;

typedef struct AppSPIToolScanner {
    View* view;
    SPIToolScannerModel* model;
} AppSPIToolScanner;

#include "401DigiLab_main.h"

typedef enum {
    AppSPIToolScannerIndex_Scanner,
    AppSPIToolScannerIndex_Writer,
    AppSPIToolScannerIndex_Reader
} AppSPIToolScannerIndex;

AppSPIToolScanner* app_spitoolscanner_alloc();
void app_spitoolscanner_callback(void* context, uint32_t index);
void app_scene_spitoolscanner_on_enter(void* context);
bool app_scene_spitoolscanner_on_event(void* context, SceneManagerEvent event);
void app_scene_spitoolscanner_on_exit(void* context);
void app_spitoolscanner_render_callback(Canvas* canvas, void* ctx);
bool app_spitoolscanner_input_callback(InputEvent* input_event, void* ctx);
View* app_spitoolscanner_get_view(AppSPIToolScanner* appSpitoolScanner);
void app_spitoolscanner_free(AppSPIToolScanner* appSpitoolScanner);

#endif
