/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#ifndef _401LIGHTMSG_SCENE_ACC_H
#define _401LIGHTMSG_SCENE_ACC_H
#include "bmp.h"
#include "font.h"
#include <furi_hal.h>
#include <furi.h>
#include "drivers/sk6805.h"
#include <dialogs/dialogs.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
// #include "401LightMsg.h"

// Display modes
typedef enum {
    APPACC_DISPLAYMODE_TEXT,
    APPACC_DISPLAYMODE_BITMAP,
    APPACC_DISPLAYMODE_CUSTOM,
} AppAccDisplayMode;

// AppAcc application structure
typedef struct AppAcc {
    View* view;
    uint32_t counter;
    FuriThread* accThread;
    DialogsApp* dialogs;
    AppAccDisplayMode displayMode;
    FuriString* bitmapPath;
    bitmapMatrix* bitmapMatrix;
    uint16_t cycles; // Cycles counter
    uint16_t cyclesAvg; // Average cycles
    uint16_t cyclesCenter;
    uint32_t swipes_count;
    bool direction;
    // void *data;
} AppAcc;

#include "401LightMsg_main.h"

AppAcc* app_acc_alloc(void* ctx);
View* app_acc_get_view(AppAcc* appAcc);
void app_acc_render_callback(Canvas* canvas, void* model);
void app_acc_free(AppAcc* appAcc);

void app_scene_acc_on_enter(void* context);
bool app_scene_acc_on_event(void* context, SceneManagerEvent event);
void app_scene_acc_on_exit(void* context);
#endif
