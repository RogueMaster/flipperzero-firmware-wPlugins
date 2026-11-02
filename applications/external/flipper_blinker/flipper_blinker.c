#include "app.h"

int32_t flipper_blinker_app(void* p) {
    UNUSED(p);

    FlipperBlinkerApp* app = flipper_blinker_app_alloc();
    flipper_blinker_show_main_menu(app);
    view_dispatcher_run(app->view_dispatcher);
    flipper_blinker_app_free(app);

    return 0;
}
