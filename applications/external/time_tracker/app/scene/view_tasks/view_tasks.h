#ifndef VIEW_TASKS_H
#define VIEW_TASKS_H

#include "../../app.h"
#include <furi.h>

// Function declarations
void submenu_callback_view_tasks(void* context, uint32_t index);
void scene_on_enter_view_tasks(void* context);
bool scene_on_event_view_tasks(void* context, SceneManagerEvent event);
void scene_on_exit_view_tasks(void* context);

#endif // VIEW_TASKS_H
