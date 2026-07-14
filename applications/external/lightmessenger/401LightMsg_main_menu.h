/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#ifndef _401LIGHTMSG_SCENE_MAINMENU_H
#define _401LIGHTMSG_SCENE_MAINMENU_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <gui/modules/submenu.h>
#include <gui/scene_manager.h>

#include <401_light_msg_icons.h>

typedef struct AppMainMenu {
    Submenu* submenu;
} AppMainMenu;

#include "401LightMsg_main.h"

typedef enum {
    AppMainMenuIndex_Splash,
    AppMainMenuIndex_About,
    AppMainMenuIndex_Flashlight,
    AppMainMenuIndex_Config,
    AppMainMenuIndex_SetText,
    AppMainMenuIndex_BmpEditor,
    AppMainMenuIndex_Acc_Text,
    AppMainMenuIndex_Acc_Bitmap,
} AppMainMenuIndex;

AppMainMenu* app_mainmenu_alloc();
void appMainMenu_callback(void* context, uint32_t index);
void app_scene_mainmenu_on_enter(void* context);
bool app_scene_mainmenu_on_event(void* context, SceneManagerEvent event);
void app_scene_mainmenu_on_exit(void* context);
#endif
