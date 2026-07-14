
#ifndef TASK_H
#define TASK_H
#include "../app.h"
#include <datetime/datetime.h>
#include <storage/storage.h>

#include "../structs.h"

void tasks_init(App* app);
bool tasks_add(App* app, const Task* task);
bool tasks_update(App* app, const Task* current_task);
bool tasks_remove(App* app, const Task* task);
void current_task_init(App* app);
void current_task_free(Task* task);
void current_task_update(App* app, const Task* current_task);
bool create_new_task(App* app);
void tasks_free(Tasks* tasks);
void task_remove(App* app, const char* task_id);
void current_task_empty(App* app);

TaskStatus string_to_task_status(const char* status_str);
const char* task_status_to_string(TaskStatus status);
#endif // TASK_H
