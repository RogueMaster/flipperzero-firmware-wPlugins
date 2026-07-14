#ifndef TASK_CONTINUE_H
#define TASK_CONTINUE_H
#include "../../app.h"
#include <gui/modules/dialog_ex.h>

void task_continue_timer_callback(void* context);
void scene_on_enter_task_continue(void* context);
bool scene_on_event_task_continue(void* context, SceneManagerEvent event);
void scene_on_exit_task_continue(void* context);
#endif // TASK_CONTINUE_H
