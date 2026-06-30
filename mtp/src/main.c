#include <stdio.h>
#include <furi.h>
#include <gui/gui.h>
#include "main.h"
#include "scenes/register.h"

int main() {
    App* app = malloc(sizeof(App));
    furi_assert(app != NULL, "Failed to allocate memory for the app");

    Gui* gui = furi_record_open(RECORD_GUI);
    furi_assert(gui != NULL, "Failed to open the GUI record");

    register_scenes(app);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    // The default scene is always the first one!
    scene_manager_next_scene(app->scene_manager, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, 0);

    view_dispatcher_run(app->view_dispatcher);

    FURI_LOG_I("DemoApp", "Exiting application.");
    free_scenes(app);

    FURI_LOG_I("DemoApp", "Freed app.");

    return 0;
}

// Stub entrypoint due to gcc complaining about
// mismatching main function signature.
int32_t entrypoint(void* p) {
    UNUSED(p);
    return main();
}
