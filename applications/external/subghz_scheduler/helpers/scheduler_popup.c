#include "scheduler_popup.h"

static void scheduler_popup_timeout_callback(void* context) {
    SchedulerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, SchedulerEventPopupDone);
}

void scheduler_popup_success_show(SchedulerApp* app, const char* message) {
    popup_reset(app->popup);

    popup_set_header(app->popup, "Success!", 64, 10, AlignLeft, AlignTop);
    popup_set_text(app->popup, message, 64, 42, AlignLeft, AlignCenter);

    popup_set_context(app->popup, app);
    popup_set_callback(app->popup, scheduler_popup_timeout_callback);

    popup_set_timeout(app->popup, SCHEDULER_POPUP_TIMEOUT_MS);
    popup_enable_timeout(app->popup);

    popup_set_icon(app->popup, 10, 8, &I_icon_36x48px);

    view_dispatcher_switch_to_view(app->view_dispatcher, SchedulerAppViewPopup);
}
