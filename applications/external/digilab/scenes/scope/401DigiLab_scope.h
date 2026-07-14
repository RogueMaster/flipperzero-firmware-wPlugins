/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#ifndef _401DIGILAB_SCENE_OSC_H
#define _401DIGILAB_SCENE_OSC_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <gui/gui.h>
//#include <gui/gui_i.h>
#include "lib/pulse_reader/pulse_reader.h"
#include <gui/modules/variable_item_list.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <lib/toolbox/value_index.h>
#include <locale/locale.h>
#include <ringbuffer/ringbuffer.h>
#include <osc.h>
#include "../../drivers/sk6805.h"
#include "app_params.h" // Application specific oscuration

#define L401DIGILAB_HW_BRIDGE_RATIO 0.435

typedef struct {
    uint16_t raw;
    float voltage;
    OscWindow* oscWindow;
    bool capture;
    double BridgeFactor;
    bool inversion;
    uint16_t inversion_count;
} ScopeModel;

typedef enum {
    ScopeEventRedraw
} ScopeEvent;

typedef struct {
    View* view;
    VariableItemList* list_config;
    VariableItem* list_config_item;
    FuriTimer* timer;
    FuriHalAdcHandle* adc;
    ScopeModel* model;
} AppScope;

#include "401DigiLab_main.h"

AppScope* app_scope_alloc();
void app_scope_free(AppScope* appScope);
View* app_scope_get_view(AppScope* appScope);
void app_scope_render_callback(Canvas* canvas, void* ctx);
bool app_scope_input_callback(InputEvent* input_event, void* ctx);
void app_scene_scope_on_enter(void* ctx);
bool app_scene_scope_on_custom_event(uint32_t event, void* ctx);
bool app_scene_scope_on_event(void* ctx, SceneManagerEvent event);
void app_scene_scope_on_exit(void* ctx);
#endif
