/*
 * Barcode input scene.
 */

#include "../tagtinker_app.h"

static void unsupported_tag_back_cb(GuiButtonType type, InputType input_type, void* ctx) {
    UNUSED(type);
    UNUSED(input_type);
    TagTinkerApp* app = ctx;
    scene_manager_previous_scene(app->scene_manager);
}

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

    app->barcode_valid = tagtinker_is_barcode_valid(app->barcode);

    if(!app->barcode_valid) {
        popup_reset(app->popup);
        popup_set_header(app->popup, "Invalid Barcode", 64, 20, AlignCenter, AlignCenter);
        popup_set_text(app->popup, "Format: Letter + 16 digits", 64, 40, AlignCenter, AlignCenter);
        popup_set_timeout(app->popup, 2000);
        popup_enable_timeout(app->popup);
        popup_set_callback(app->popup, NULL);
        view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewPopup);
        return true;
    }

    TagTinkerTagProfile profile;
    if(!tagtinker_barcode_to_profile(app->barcode, &profile)) {
        widget_reset(app->widget);
        widget_add_string_element(
            app->widget, 64, 10, AlignCenter, AlignTop, FontPrimary, "Unsupported Tag");
        widget_add_string_multiline_element(
            app->widget,
            64,
            30,
            AlignCenter,
            AlignTop,
            FontSecondary,
            "No profile detected\nfor this barcode.");
        widget_add_button_element(
            app->widget, GuiButtonTypeLeft, "Back", unsupported_tag_back_cb, app);
        view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewWidget);
        return true;
    }

    app->selected_target = tagtinker_ensure_target(app, app->barcode);

    if(app->selected_target >= 0) {
        tagtinker_select_target(app, (uint8_t)app->selected_target);
    }

    uint32_t target_scene =
        scene_manager_get_scene_state(app->scene_manager, TagTinkerSceneBarcodeInput);
    scene_manager_next_scene(app->scene_manager, target_scene);
    return true;
}

void tagtinker_scene_barcode_input_on_exit(void* ctx) {
    TagTinkerApp* app = ctx;
    numlock_input_reset(app->numlock);
}
