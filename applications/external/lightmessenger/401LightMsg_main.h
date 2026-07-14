/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#ifndef _401LIGHTMSG_APP_MAIN_H
#define _401LIGHTMSG_APP_MAIN_H
#define I2C_BUS &furi_hal_i2c_handle_external
#include "app_params.h" // Application specific configuration
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "drivers/lis2dh12_reg.h"
#include "drivers/lis2xx12_wrapper.h"

#include "401_config.h"
#include "401_sign.h"
#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <locale/locale.h>
#include <notification/notification_messages.h>

typedef enum {
    AppSceneMainMenu,
    AppSceneBmpEditor,
    AppSceneSplash,
    AppSceneConfig,
    AppSceneSetText,
    AppSceneAcc,
    AppSceneNum,
} appScene;

typedef enum {
    lightMsgStateNotFound,
    lightMsgStateFound,
    lightMsgStateWriteSuccess,
    lightMsgStateReadSuccess,
    lightMsgStateWriteReadSuccess,
} lightMsgState;

typedef enum {
    AppViewPopup,
    AppViewMainMenu,
    AppViewBmpEditor,
    AppViewSplash,
    AppViewConfig,
    AppViewSetText,
    AppViewAcc,
} appViews;

typedef enum {
    AppStateSplash,
    AppStateAbout,
    AppStateFlashlight,
} AppState;

typedef struct {
    AppState app_state;
    uint8_t screen;
    char message[64];
} AppStateCtx;

#include "401LightMsg_acc.h"
#include "401LightMsg_bmp_editor.h"
#include "401LightMsg_main.h"
#include "401LightMsg_set_text.h"
#include "401LightMsg_splash.h"
#include "401LightMsg_config.h"
#include "401LightMsg_main_menu.h"

uint32_t app_Quit_callback(void* context);
uint32_t app_navigateTo_MainMenu_callback(void* context);
uint32_t app_navigateTo_Splash_callback(void* context);

typedef struct {
    Configuration* config;
    stmdev_ctx_t lis2dh12;
    lis2dh12_reg_t lis2dh12_reg;
    color_animation_callback shader;
} AppData;

typedef struct {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;
    l401_err err;
    AppSplash* sceneSplash;
    AppConfig* sceneConfig;
    TextInput* sceneSetText;
    AppBmpEditor* sceneBmpEditor;
    AppAcc* sceneAcc;
    Submenu* mainmenu;
    AppData* data;
} AppContext;

#endif
