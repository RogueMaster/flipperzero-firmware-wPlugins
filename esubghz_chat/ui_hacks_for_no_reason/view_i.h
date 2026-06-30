/**
 * @file view_i.h
 * GUI: internal View API
 */

#pragma once

#include <gui/view.h>
#include <furi.h>

typedef struct {
    FuriMutex* mutex;
    uint8_t data[];
} ViewModelLocking;

struct View {
    ViewDrawCallback draw_callback;
    ViewInputCallback input_callback;
    ViewCustomCallback custom_callback;

    ViewModelType model_type;
    ViewNavigationCallback previous_callback;
    ViewCallback enter_callback;
    ViewCallback exit_callback;
    ViewOrientation orientation;

    ViewUpdateCallback update_callback;
    void* update_callback_context;

    void* model;
    void* context;
};
