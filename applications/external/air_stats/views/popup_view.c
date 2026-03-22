/*
 * popup_view.c — popup notification view.
 * Adapted from _ref_unitemp/views/Popup_view.c
 * Note: app->popup is allocated in co2_app.c and registered as ViewPopup.
 */
#include "../air_stats_i.h"

static uint32_t _prev_view_id;

static void _popup_callback(void* context) {
    UNUSED(context);
    view_dispatcher_switch_to_view(app->view_dispatcher, _prev_view_id);
}

void view_popup(const Icon* icon, char* header, char* message, uint32_t prev_view_id) {
    _prev_view_id = prev_view_id;
    popup_reset(app->popup);

    if(icon != NULL) {
        popup_set_icon(app->popup, 0, 64 - icon_get_height(icon), icon);
    }

    popup_set_header(app->popup, header, 64, 6, AlignCenter, AlignCenter);
    popup_set_text(
        app->popup,
        message,
        64,
        32,
        AlignCenter,
        AlignCenter);

    popup_set_timeout(app->popup, 3000);
    popup_set_callback(app->popup, _popup_callback);
    popup_enable_timeout(app->popup);

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewPopup);
}
