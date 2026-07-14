/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#ifndef _401LIGHTMSG_SCENE_SET_TEXT_H
#define _401LIGHTMSG_SCENE_SET_TEXT_H

#include "app_params.h" // Application specific configuration

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <gui/modules/text_input.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>

typedef enum {
    SetTextInputSaveEvent,
} SetTextInputEvent;

void app_scene_set_text_callback(void* context);
void app_scene_set_text_on_enter(void* context);
bool app_scene_set_text_on_event(void* context, SceneManagerEvent event);
void app_scene_set_text_on_exit(void* ctx);
#endif
