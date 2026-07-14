/** initialise app data, scene manager, and view dispatcher */
#include "../storage/init.h"
#include "../app.h"
#include "../scene/scene.h"
#include "../tasks/task.h"

#define TAG "tracker_app"

App* init() {
    FURI_LOG_T(TAG, "init");
    App* app = malloc(sizeof(App));
    current_task_init(app);
    tasks_init(app);
    storage_init(app);
    scene_manager_init(app);
    view_dispatcher_init(app);
    return app;
}
