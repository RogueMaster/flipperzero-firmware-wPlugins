/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#ifndef _401DIGILAB_SCENE_MAINMENU_H
#define _401DIGILAB_SCENE_MAINMENU_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <gui/modules/submenu.h>
#include <gui/scene_manager.h>

#include <401_digilab_icons.h>

typedef struct AppMainMenu {
    Submenu* submenu;
} AppMainMenu;

#include "401DigiLab_main.h"

typedef enum {
    AppMainMenuIndex_Probe,
    AppMainMenuIndex_I2CTool,
    AppMainMenuIndex_SPITool,
    AppMainMenuIndex_Osc,
    AppMainMenuIndex_Splash,
    AppMainMenuIndex_About,
    AppMainMenuIndex_Config,
} AppMainMenuIndex;

AppMainMenu* app_mainmenu_alloc();
void appMainMenu_callback(void* context, uint32_t index);
void app_scene_mainmenu_on_enter(void* context);
bool app_scene_mainmenu_on_event(void* context, SceneManagerEvent event);
void app_scene_mainmenu_on_exit(void* context);
#endif
