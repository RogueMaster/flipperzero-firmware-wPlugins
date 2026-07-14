#include <furi.h>
#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>

#include "app/app.h"
#include "app/free/free.h"
#include "app/init/init.h"
#include "app/structs.h"

/** go to trace log level in the dev environment */
void set_log_level() {
#ifdef FURI_DEBUG
    furi_log_set_level(FuriLogLevelTrace);
#else
    furi_log_set_level(FuriLogLevelInfo);
#endif
}

/** entrypoint */
int32_t app(void* p) {
    UNUSED(p);
    set_log_level();

    // create the app context struct, scene manager, and view dispatcher
    FURI_LOG_I(TAG, "Flipper postman app starting...");
    App* app = init();

    // set the scene and launch the main loop
    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, SetUpDialog);
    FURI_LOG_T(TAG, "Starting dispatcher...");
    view_dispatcher_run(app->view_dispatcher);

    // free all memory
    FURI_LOG_I(TAG, "Flipper postman app finishing...");
    furi_record_close(RECORD_GUI);
    app_free(app);
    return 0;
}
