// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file xiaomi_filter_reset_scene_success.c
 * @brief Success scene: confirms the filter was reset.
 */
#include "../xiaomi_filter_reset_i.h"

// The popup keeps a pointer to the text rather than copying it, so back it with
// storage that outlives the scene. A single scene instance is active at a time.
static char xiaomi_filter_reset_success_text[64];

static void xiaomi_filter_reset_scene_success_popup_callback(void* context) {
    XiaomiFilterResetApp* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, XiaomiFilterResetCustomEventPopupTimeout);
}

void xiaomi_filter_reset_scene_success_on_enter(void* context) {
    XiaomiFilterResetApp* app = context;
    Popup* popup = app->popup;

    char product[XIAOMI_FILTER_PRODUCT_CODE_SIZE];
    xiaomi_filter_worker_get_product_code(app->worker, product);
    // "Already fresh" is a claim about the tag's prior state, so only make it when the
    // pre-reset counter was actually read and was zero. If the read failed we don't know
    // the prior state, so fall back to the always-true "restored" message.
    uint32_t old_counter = 0;
    const bool was_fresh = xiaomi_filter_worker_get_old_counter(app->worker, &old_counter) &&
                           old_counter == 0;

    snprintf(
        xiaomi_filter_reset_success_text,
        sizeof(xiaomi_filter_reset_success_text),
        was_fresh ? "Filter \"%s\"\nwas already at 100%%" :
                    "Filter \"%s\"\nlife restored to 100%%",
        product);

    popup_reset(popup);
    popup_set_header(popup, was_fresh ? "Already fresh" : "Done!", 64, 10, AlignCenter, AlignTop);
    popup_set_text(popup, xiaomi_filter_reset_success_text, 64, 30, AlignCenter, AlignTop);
    popup_set_context(popup, app);
    popup_set_callback(popup, xiaomi_filter_reset_scene_success_popup_callback);
    popup_set_timeout(popup, 4000);
    popup_enable_timeout(popup);
    view_dispatcher_switch_to_view(app->view_dispatcher, XiaomiFilterResetViewPopup);

    notification_message(app->notifications, &sequence_success);
}

bool xiaomi_filter_reset_scene_success_on_event(void* context, SceneManagerEvent event) {
    XiaomiFilterResetApp* app = context;
    bool consumed = false;

    if((event.type == SceneManagerEventTypeCustom &&
        event.event == XiaomiFilterResetCustomEventPopupTimeout) ||
       event.type == SceneManagerEventTypeBack) {
        consumed = true;
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, XiaomiFilterResetSceneStart);
    }

    return consumed;
}

void xiaomi_filter_reset_scene_success_on_exit(void* context) {
    XiaomiFilterResetApp* app = context;
    popup_reset(app->popup);
}
