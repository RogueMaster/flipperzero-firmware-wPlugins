/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#ifndef _401LIGHTMSG_SCENE_CONFIG_H
#define _401LIGHTMSG_SCENE_CONFIG_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "drivers/sk6805.h"
#include <gui/modules/variable_item_list.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <lib/toolbox/value_index.h>
#include <locale/locale.h>

#include "app_params.h" // Application specific configuration

#include "401_config.h"

typedef struct AppConfig {
    View* view;
    FuriTimer* timer;
    VariableItemList* list;
} AppConfig;

#include "401LightMsg_main.h"

typedef enum {
    LightMsg_OrientationWheelUp = 0,
    LightMsg_OrientationWheelDown = 1
} LightMsg_Orientation;

extern const LightMsg_Orientation lightmsg_orientation_value[];
extern const char* const lightmsg_orientation_text[];

typedef enum {
    LightMsg_BrightnessLow,
    LightMsg_BrightnessModerate,
    LightMsg_BrightnessBright,
    LightMsg_BrightnessBlinding
} LightMsg_Brightness_index;

extern const double lightmsg_brightness_value[];
extern const char* const lightmsg_brightness_text[];

extern const uint8_t lightmsg_sensitivity_value[];
extern const char* const lightmsg_sensitivity_text[];

#define COLOR_RED    0
#define COLOR_ORANGE 1
#define COLOR_YELLOW 2
#define COLOR_GREEN  3
#define COLOR_CYAN   4
#define COLOR_BLUE   5
#define COLOR_PURPLE 6

// Flat colors index->value
extern const uint32_t lightmsg_colors_flat[];

typedef enum {
    LightMsg_ColorRed,
    LightMsg_ColorOrange,
    LightMsg_ColorYellow,
    LightMsg_ColorGreen,
    LightMsg_ColorCyan,
    LightMsg_ColorBlue,
    LightMsg_ColorPurple,
    LightMsg_ColorNyancat,
    LightMsg_ColorRainbow
} LightMsg_Color_index;

// Animation labels
extern const char* const lightmsg_color_text[];
extern color_animation_callback lightmsg_color_value[];

void LightMsg_color_cb_flat(uint16_t tick, bool direction, uint32_t* result, void* ctx);
void LightMsg_color_cb_directional(uint16_t tick, bool direction, uint32_t* result, void* ctx);
void LightMsg_color_cb_nyancat(uint16_t tick, bool direction, uint32_t* result, void* ctx);
void LightMsg_color_cb_vaporwave(uint16_t tick, bool direction, uint32_t* result, void* ctx);
void LightMsg_color_cb_sparkle(uint16_t tick, bool direction, uint32_t* result, void* ctx);
void LightMsg_color_cb_rainbow(uint16_t tick, bool direction, uint32_t* result, void* ctx);

// Custom configuration
typedef enum {
    LightMsg_MirrorDisabled = 0,
    LightMsg_MirrorEnabled = 1
} LightMsg_Mirror;

extern const LightMsg_Mirror lightmsg_mirror_value[];
extern const char* const lightmsg_mirror_text[];

// Speed (ms) to change text being display (0=never, 1 sec, 0.5 sec)
extern const uint32_t lightmsg_speed_value[];
extern const char* const lightmsg_speed_text[];

// Delay in microseconds (us) to wait after rendering a column
extern const uint32_t lightmsg_width_value[];
extern const char* const lightmsg_width_text[];

AppConfig* app_config_alloc(void* ctx);
void app_config_free(AppConfig* appConfig);
View* app_config_get_view(AppConfig* appConfig);
void app_scene_config_on_enter(void* ctx);
bool app_scene_config_on_event(void* ctx, SceneManagerEvent event);
void app_scene_config_on_exit(void* ctx);
#endif
