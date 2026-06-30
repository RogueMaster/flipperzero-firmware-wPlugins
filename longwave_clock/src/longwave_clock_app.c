#include "longwave_clock_app.h"
#include "app_state.h"
#include "flipper.h"
#include "scene_dcf77.h"
#include "scene_main_menu.h"
#include "scenes.h"

const char* TAG = "LongWaveClock";

/** entrypoint */
int32_t longwave_clock_app(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "Long Wave Clock app launched");

    FURI_LOG_D(TAG, "Allocating App* and AppState*");
    App* app = app_alloc();
    app->state = app_state_alloc();

    FURI_LOG_D(TAG, "Opening GUI record and attaching to it...");
    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, LWCMainMenuScene);

    FURI_LOG_D(TAG, "Handing over to view dispatcher.");
    view_dispatcher_run(app->view_dispatcher);
    FURI_LOG_D(TAG, "Returning from view dispatcher, freeing the app.");

    app_free(app);

    FURI_LOG_I(TAG, "Long Wave Clock app ended.");
    return 0;
}
