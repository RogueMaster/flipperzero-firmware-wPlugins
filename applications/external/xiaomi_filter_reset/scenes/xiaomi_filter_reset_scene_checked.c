// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file xiaomi_filter_reset_scene_checked.c
 * @brief Result scene for a read-only check: reports the filter's state, no write.
 *
 * Reaching this scene means authentication succeeded (a genuine Xiaomi filter) and
 * the usage counter was read. We report whether it is fresh (counter == 0) or has
 * been used. We deliberately do not show a life percentage: the counter is elapsed
 * usage in unknown units and the appliance's full-scale rating is model-specific and
 * not stored on the tag, so any percentage would be a guess.
 */
#include "../xiaomi_filter_reset_i.h"

// The popup keeps a pointer to the text rather than copying it, so back it with
// storage that outlives the scene. A single scene instance is active at a time.
static char xiaomi_filter_reset_checked_text[64];

static void xiaomi_filter_reset_scene_checked_popup_callback(void* context) {
    XiaomiFilterResetApp* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, XiaomiFilterResetCustomEventPopupTimeout);
}

void xiaomi_filter_reset_scene_checked_on_enter(void* context) {
    XiaomiFilterResetApp* app = context;
    Popup* popup = app->popup;

    char product[XIAOMI_FILTER_PRODUCT_CODE_SIZE];
    xiaomi_filter_worker_get_product_code(app->worker, product);

    uint32_t counter = 0;
    const bool have_counter = xiaomi_filter_worker_get_old_counter(app->worker, &counter);
    // The scan scene only routes here after a counter read, so have_counter is true; the
    // fallback keeps us honest if that ever changes (auth passed, but the value is unknown).
    const char* state = !have_counter ? "Genuine Xiaomi filter" :
                        counter == 0  ? "Already at 100%" :
                                        "Has been used";

    snprintf(
        xiaomi_filter_reset_checked_text,
        sizeof(xiaomi_filter_reset_checked_text),
        "Filter \"%s\"\n%s",
        product,
        state);

    popup_reset(popup);
    popup_set_header(popup, "Filter checked", 64, 10, AlignCenter, AlignTop);
    popup_set_text(popup, xiaomi_filter_reset_checked_text, 64, 30, AlignCenter, AlignTop);
    popup_set_context(popup, app);
    popup_set_callback(popup, xiaomi_filter_reset_scene_checked_popup_callback);
    popup_set_timeout(popup, 4000);
    popup_enable_timeout(popup);
    view_dispatcher_switch_to_view(app->view_dispatcher, XiaomiFilterResetViewPopup);

    notification_message(app->notifications, &sequence_single_vibro);
}

bool xiaomi_filter_reset_scene_checked_on_event(void* context, SceneManagerEvent event) {
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

void xiaomi_filter_reset_scene_checked_on_exit(void* context) {
    XiaomiFilterResetApp* app = context;
    popup_reset(app->popup);
}
