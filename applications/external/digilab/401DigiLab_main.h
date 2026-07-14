/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#ifndef _401DIGILAB_APP_MAIN_H
#define _401DIGILAB_APP_MAIN_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "app_params.h" // Application specific configuration

#include <board.h>
#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <locale/locale.h>
#include <notification/notification_messages.h>
#include "401_config.h"
#include "401_sign.h"

typedef enum {
    AppEventTypeTick,
    AppEventTypeKey,
} AppEventType;

typedef enum {
    AppCustomEventQuit
} AppCustomEvent;

typedef struct {
    AppEventType type; // The reason for this event.
    InputEvent input; // This data is specific to keypress data.
} AppEvent;

typedef enum {
    AppSceneProbe,
    AppSceneI2CToolScanner,
    AppSceneI2CToolReader,
    AppSceneSPITool,
    AppSceneSPIToolScanner,
    AppSceneScope,
    AppSceneScopeConfig,
    AppSceneMainMenu,
    AppSceneSplash,
    AppSceneConfig,
    AppSceneNum,
} appScene;

typedef enum {
    digiLabStateNotFound,
    digiLabStateFound,
    digiLabStateWriteSuccess,
    digiLabStateReadSuccess,
    digiLabStateWriteReadSuccess,
} digiLabState;

typedef enum {
    AppViewProbe,
    AppViewI2CToolScanner,
    AppViewI2CToolReader,
    AppViewI2CToolReaderByteInput,
    AppViewI2CToolReaderNumberInput,
    AppViewI2CToolWriterPopup,
    AppViewSPITool,
    AppViewSPIToolScanner,
    AppViewScope,
    AppViewScopeConfig,
    AppViewMainMenu,
    AppViewSplash,
    AppViewConfig,
} appViews;

typedef enum {
    AppStateSplash,
    AppStateAbout,
} AppState;

typedef struct {
    AppState app_state;
    uint8_t screen;
    char message[64];
} AppStateCtx;

#include <401DigiLab_config.h>
#include <scenes/i2c/401DigiLab_i2ctool_reader.h>
#include <scenes/i2c/401DigiLab_i2ctool_scanner.h>
#include <scenes/spi/401DigiLab_spitool.h>
#include <scenes/spi/401DigiLab_spitool_scanner.h>
#include <401DigiLab_main.h>
#include <401DigiLab_main_menu.h>
#include <401DigiLab_probe.h>
#include <scenes/scope/401DigiLab_scope.h>
#include <scenes/scope/401DigiLab_scope_config.h>

#include "401DigiLab_splash.h"

uint32_t app_Quit_callback(void* context);
uint32_t app_navigateTo_MainMenu_callback(void* context);
uint32_t app_navigateTo_Splash_callback(void* context);

typedef struct {
    Configuration* config;
} AppData;

typedef struct {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;
    l401_err err;
    AppProbe* sceneProbe;
    AppI2CToolScanner* sceneI2CToolScanner;
    AppI2CToolReader* sceneI2CToolReader;
    i2cdevice_t* i2cToolDevice;
    Submenu* spitoolmenu;
    AppSPIToolScanner* sceneSPIToolScanner;
    AppScope* sceneScope;
    AppScopeConfig* sceneScopeConfig;
    AppSplash* sceneSplash;
    AppConfig* sceneConfig;
    Submenu* mainmenu;
    AppData* data;
} AppContext;

l401_err check_hat(AppContext* app);
#endif
