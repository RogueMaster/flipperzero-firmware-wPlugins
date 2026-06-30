#pragma once

#include <gui/view.h>
#include "../helpers/flip_tdi_types.h"
#include "../helpers/flip_tdi_event.h"

typedef struct FlipTDIViewMainType FlipTDIViewMainType;

typedef void (*FlipTDIViewMainTypeCallback)(FlipTDICustomEvent event, void* context);

void flip_tdi_view_main_set_callback(
    FlipTDIViewMainType* instance,
    FlipTDIViewMainTypeCallback callback,
    void* context);

FlipTDIViewMainType* flip_tdi_view_main_alloc();

void flip_tdi_view_main_free(FlipTDIViewMainType* instance);

View* flip_tdi_view_main_get_view(FlipTDIViewMainType* instance);
