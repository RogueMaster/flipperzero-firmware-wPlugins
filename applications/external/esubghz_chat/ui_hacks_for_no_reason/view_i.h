/* NOT a firmware header - app-local copy of a firmware private header. It must
 * stay byte-identical to the firmware's. See the sync notes at the top of
 * view_port_i.h before changing anything in this directory.
 *
 * No field of struct View is read by this app; it only compares View pointers,
 * and this copy exists so view_dispatcher_i.h compiles. Synced to RM firmware at FAP API 88.27; still re-diff it on an API bump.
 */
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

    ViewAsciiCallback ascii_callback;
};
