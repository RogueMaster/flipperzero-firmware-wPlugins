#ifndef CSV_H
#define CSV_H

#include "../app.h"

bool write_task_to_csv(App* app, const Task* task);
bool read_tasks_from_csv(App* app);
bool delete_task_from_csv(App* app, const char* task_id);
bool find_and_replace_task_in_csv(App* app, const Task* current_task);
#endif // CSV_H
