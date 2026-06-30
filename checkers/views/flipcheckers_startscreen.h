#pragma once

#include <gui/view.h>
#include "../helpers/flipcheckers_custom_event.h"

typedef struct FlipCheckersStartscreen FlipCheckersStartscreen;

typedef void (*FlipCheckersStartscreenCallback)(FlipCheckersCustomEvent event, void* context);

void flipcheckers_startscreen_set_callback(
    FlipCheckersStartscreen* flipcheckers_startscreen,
    FlipCheckersStartscreenCallback callback,
    void* context);

View* flipcheckers_startscreen_get_view(FlipCheckersStartscreen* flipcheckers_static);

FlipCheckersStartscreen* flipcheckers_startscreen_alloc();

void flipcheckers_startscreen_free(FlipCheckersStartscreen* flipcheckers_static);
