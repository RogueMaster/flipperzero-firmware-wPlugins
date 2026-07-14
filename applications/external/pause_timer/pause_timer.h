#pragma once

#include <furi.h>
#include <infrared.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>

#include "views/time_input.h"
#include "views/countdown.h"
#include "views/ir_learn.h"
#include "scenes/scene.h"

typedef struct {
    bool is_decoded;
    bool has_signal;

    InfraredMessage decoded_message;
    uint32_t* raw_timings;
    size_t raw_timings_size;
    uint32_t frequency;
    float duty_cycle;
} IrSignalStorage;

struct PauseTimerApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    PTTimeInput* time_input;
    CountdownUtils* countdown;
    IrLearnArgs* ir_learn;
    uint16_t current_timer_val;
    IrSignalStorage learned_ir_signal;
};

void free_ir_signal(IrSignalStorage* signal);
void countdown_back_callback(void* context);
void ir_learn_signal_learned_callback(void* context);
void ir_learn_back_callback(void* context);
