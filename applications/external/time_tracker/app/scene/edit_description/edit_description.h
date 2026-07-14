#ifndef EDIT_DESCRIPTION_H
#define EDIT_DESCRIPTION_H

#include "../../app.h"
#include "../../structs.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/elements.h>
#include <gui/modules/text_input.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

void scene_on_enter_task_description_input(void* context);
bool scene_on_event_task_description_input(void* context, SceneManagerEvent event);
void scene_on_exit_task_description_input(void* context);

#endif // EDIT_DESCRIPTION_H
