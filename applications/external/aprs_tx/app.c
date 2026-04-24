#include "src/ui/ui_i.h"

int32_t flipperham_app(void *p)
{
    UNUSED(p);

    FlipperHamApp *app = flipperham_app_alloc();

    while (1)
    {
        app->send_requested = false;
        view_dispatcher_switch_to_view(app->view_dispatcher, app->return_view);
        view_dispatcher_run(app->view_dispatcher);

        if (!app->send_requested)
            break;

        flipperham_send_hardcoded_message(app);
    }

    flipperham_app_free(app);
    return 0;
}
