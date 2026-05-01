#pragma once

#include <gui/scene_manager.h>

typedef enum {
    FlipPassOtherFieldsModeType = 0,
    FlipPassOtherFieldsModeEdit = 1,
    FlipPassOtherFieldsModeEditNoAuto = 2,
} FlipPassOtherFieldsMode;

void flippass_scene_other_fields_on_enter(void* context);
bool flippass_scene_other_fields_on_event(void* context, SceneManagerEvent event);
void flippass_scene_other_fields_on_exit(void* context);
