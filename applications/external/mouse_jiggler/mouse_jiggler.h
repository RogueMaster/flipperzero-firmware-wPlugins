#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include "mouse_jiggler_worker.h"

typedef struct MouseJiggler_t {
    MouseJigglerWorker* worker;
    FuriMessageQueue* event_queue;
    FuriMutex* mutex;
    ViewPort* view_port;
    Gui* gui;
    bool exit;
} MouseJiggler;

MouseJiggler* mouse_jiggler_alloc();

void mouse_jiggler_free(MouseJiggler* instance);

int32_t mouse_jiggler_app();
