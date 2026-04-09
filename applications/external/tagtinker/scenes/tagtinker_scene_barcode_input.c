/*
 * Barcode input scene.
 */

#include "../tagtinker_app.h"

static void numlock_done(void* ctx, const char* barcode) {
    TagTinkerApp* app = ctx;
    strncpy(app->barcode, barcode, TAGTINKER_BC_LEN);
    app->barcode[TAGTINKER_BC_LEN] = '\0';
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void tagtinker_scene_barcode_input_on_enter(void* ctx) {
    TagTinkerApp* app = ctx;

    numlock_input_reset(app->numlock);
    numlock_input_set_callback(app->numlock, numlock_done, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewNumlock);
}

bool tagtinker_scene_barcode_input_on_event(void* ctx, SceneManagerEvent event) {
    TagTinkerApp* app = ctx;
    if(event.type != SceneManagerEventTypeCustom) return false;

    app->barcode_valid = tagtinker_barcode_to_plid(app->barcode, app->plid);

    if(!app->barcode_valid) {
        FURI_LOG_W(TAGTINKER_TAG, "Invalid barcode: %s", app->barcode);
        popup_reset(app->popup);
        popup_set_header(app->popup, "Invalid Barcode", 64, 20, AlignCenter, AlignCenter);
        popup_set_text(app->popup,
            "Format: Letter + 16 digits",
            64, 40, AlignCenter, AlignCenter);
        popup_set_timeout(app->popup, 2000);
        popup_enable_timeout(app->popup);
        popup_set_callback(app->popup, NULL);
        view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewPopup);
        return true;
    }

    FURI_LOG_I(TAGTINKER_TAG, "Barcode: %s -> PLID %02X%02X%02X%02X",
        app->barcode, app->plid[3], app->plid[2], app->plid[1], app->plid[0]);

    app->selected_target = tagtinker_ensure_target(app, app->barcode);

    if(app->selected_target >= 0) {
        tagtinker_select_target(app, (uint8_t)app->selected_target);
    }

    uint32_t target_scene = scene_manager_get_scene_state(
        app->scene_manager, TagTinkerSceneBarcodeInput);
    scene_manager_next_scene(app->scene_manager, target_scene);
    return true;
}

void tagtinker_scene_barcode_input_on_exit(void* ctx) {
    TagTinkerApp* app = ctx;
    numlock_input_reset(app->numlock);
}
