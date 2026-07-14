/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#ifndef _401DIGILAB_SCENE_PROBE_H
#define _401DIGILAB_SCENE_PROBE_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <furi_hal_spi.h>
//#include <furi_hal_nfc_i.h>
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
#include "drivers/sk6805.h"
#include "app_params.h" // Application specific probeuration

#define NFCV_FC           (13560000.0f)
#define PULSE_DURATION_NS (128.0f * 1000000000.0f / NFCV_FC) /* ns */

#define BARGRAPH_WIDTH       7
#define BARGRAPH_HEIGHT      64
#define BARGRAPH_MARGIN      2
#define BARGRAPH_MAX_VOLTAGE 12.0
#define BARGRAPH_5vLabel     BARGRAPH_HEIGHT - (5 * BARGRAPH_HEIGHT / BARGRAPH_MAX_VOLTAGE)
#define BARGRAPH_3vLabel     BARGRAPH_HEIGHT - (3 * BARGRAPH_HEIGHT / BARGRAPH_MAX_VOLTAGE)
#define BARGRAPH_12vLabel    BARGRAPH_HEIGHT - (12 * BARGRAPH_HEIGHT / BARGRAPH_MAX_VOLTAGE)

typedef struct {
    bool adc_inited;
    FuriHalAdcHandle* adc;
    uint16_t raw;
    double voltage;
    double frequency;
    double period;
    uint32_t pulse_width;
    bool pulse_edge;
    RingBuffer* rb;
    bool inf;
    double BridgeFactor;
} ProbeModel;

typedef struct {
    View* view;
    FuriTimer* timer;
    FuriHalAdcHandle* adc;
    PulseReader* pulse_reader;
    ProbeModel* model;
} AppProbe;

#include "401DigiLab_main.h"

AppProbe* app_probe_alloc();
void app_probe_free(AppProbe* appProbe);
View* app_probe_get_view(AppProbe* appProbe);
void app_probe_render_callback(Canvas* canvas, void* ctx);
bool app_probe_input_callback(InputEvent* input_event, void* ctx);
void app_scene_probe_on_enter(void* ctx);
bool app_scene_probe_on_event(void* ctx, SceneManagerEvent event);
void app_scene_probe_on_exit(void* ctx);
#endif
