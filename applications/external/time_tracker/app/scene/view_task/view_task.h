#ifndef VIEW_TASK_H
#define VIEW_TASK_H
#include "../../app.h"
#include <furi.h>
void submenu_callback_task_actions(void* context, uint32_t index);
void scene_on_enter_task_actions(void* context);
bool scene_on_event_task_actions(void* context, SceneManagerEvent event);
void scene_on_exit_task_actions(void* context);
#endif // VIEW_TASK_H
