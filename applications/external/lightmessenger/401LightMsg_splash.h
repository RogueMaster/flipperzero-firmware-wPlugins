/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2024 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#ifndef _401LIGHTMSG_SCENE_SPLASH_H
#define _401LIGHTMSG_SCENE_SPLASH_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "drivers/sk6805.h"
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>

#include <401_light_msg_icons.h>

#define SPLASH_MAX_ABOUT_SCREENS      2
#define SPLASH_MAX_FLASHLIGHT_SCREENS 3

typedef struct AppSplash {
    View* view;
} AppSplash;

#include "401LightMsg_main.h"

typedef enum {
    AppSplashEventQuit,
    AppSplashEventRoll,
} AppSplashCustomEvents;

AppSplash* app_splash_alloc(void* ctx);
void app_splash_free(AppSplash* appSplash);
View* app_splash_get_view(AppSplash* appSplash);
void app_scene_splash_on_enter(void* context);
bool app_scene_splash_on_event(void* context, SceneManagerEvent event);
void app_scene_splash_on_exit(void* context);

#endif
