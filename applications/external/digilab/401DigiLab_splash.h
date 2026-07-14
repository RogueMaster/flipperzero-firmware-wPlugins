/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#ifndef _401DIGILAB_SCENE_SPLASH_H
#define _401DIGILAB_SCENE_SPLASH_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

//#include <assets_icons.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>

#include <401_digilab_icons.h>

typedef struct AppSplash {
    View* view;
} AppSplash;

#include "401DigiLab_main.h"

typedef enum {
    AppSplashEventQuit,
    AppSplashEventRoll,
} AppSplashCustomEvents;

bool app_splash_input_callback(InputEvent* input_event, void* ctx);
void app_splash_render_callback(Canvas* canvas, void* ctx);
AppSplash* app_splash_alloc();
void app_splash_free(AppSplash* appSplash);
View* app_splash_get_view(AppSplash* appSplash);
void app_scene_splash_on_enter(void* context);
bool app_scene_splash_on_event(void* context, SceneManagerEvent event);
void app_scene_splash_on_exit(void* context);

#endif
