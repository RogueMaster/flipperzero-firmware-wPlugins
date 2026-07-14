#ifndef EDIT_NAME_H
#define EDIT_NAME_H

#include "../../app.h"
#include "../../structs.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/elements.h>
#include <gui/modules/text_input.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

#define TAG "edit_task_name"

// Function declarations
void scene_on_enter_task_name_input(void* context);
bool scene_on_event_task_name_input(void* context, SceneManagerEvent event);
void scene_on_exit_task_name_input(void* context);

#endif // EDIT_NAME_H
