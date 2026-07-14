/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#ifndef _401DIGILAB_SCENE_SPITOOL_H
#define _401DIGILAB_SCENE_SPITOOL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <gui/modules/submenu.h>
#include <gui/scene_manager.h>

#include <401_digilab_icons.h>

typedef struct AppSPITool {
    Submenu* submenu;
} AppSPITool;

#include "401DigiLab_main.h"

typedef enum {
    AppSPIToolIndex_Scanner,
    AppSPIToolIndex_Writer,
    AppSPIToolIndex_Reader
} AppSPIToolIndex;

Submenu* app_spitool_alloc();
void appSPITool_callback(void* context, uint32_t index);
void app_scene_spitool_on_enter(void* context);
bool app_scene_spitool_on_event(void* context, SceneManagerEvent event);
void app_scene_spitool_on_exit(void* context);
#endif
