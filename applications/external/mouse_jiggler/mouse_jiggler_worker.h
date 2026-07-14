#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <furi.h>
#include <furi_hal.h>

#define MOUSE_JIGGLER_MOUSE_MOVE_DX 5
#define MOUSE_JIGGLER_DELAY_MS      50
#define MOUSE_JIGGLER_INTERVAL_N    12

static const uint32_t MOUSE_JIGGLER_INTERVAL_MS[MOUSE_JIGGLER_INTERVAL_N] =
    {250, 500, 1000, 2500, 5000, 7500, 15000, 30000, 45000, 60000, 150000, 300000};

typedef struct MouseJigglerWorker_t {
    FuriThread* thread;
    bool is_running;
    bool is_jiggle;
    uint32_t delay_value;
    uint8_t delay_index;
    int8_t mouse_move_dx;
} MouseJigglerWorker;

MouseJigglerWorker* mouse_jiggler_worker_alloc();

void mouse_jiggler_worker_free(MouseJigglerWorker* instance);

void mouse_jiggler_worker_start(MouseJigglerWorker* instance);

void mouse_jiggler_worker_stop(MouseJigglerWorker* instance);

void mouse_jiggler_worker_toggle_jiggle(MouseJigglerWorker* instance);

bool mouse_jiggler_worker_increase_delay(MouseJigglerWorker* instance);

bool mouse_jiggler_worker_decrease_delay(MouseJigglerWorker* instance);
