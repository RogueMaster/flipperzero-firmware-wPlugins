#pragma once

#include <gui/view.h>
#include "../helpers/flipcheckers_custom_event.h"

typedef struct FlipCheckersScene1 FlipCheckersScene1;

typedef void (*FlipCheckersScene1Callback)(FlipCheckersCustomEvent event, void* context);

void flipcheckers_scene_1_set_callback(
    FlipCheckersScene1* flipcheckers_scene_1,
    FlipCheckersScene1Callback callback,
    void* context);

View* flipcheckers_scene_1_get_view(FlipCheckersScene1* flipcheckers_static);

FlipCheckersScene1* flipcheckers_scene_1_alloc();

void flipcheckers_scene_1_free(FlipCheckersScene1* flipcheckers_static);
