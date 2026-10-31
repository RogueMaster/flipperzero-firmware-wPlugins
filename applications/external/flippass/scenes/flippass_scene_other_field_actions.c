#include "flippass_scene_other_field_actions.h"

#include "../flippass.h"
#include "../flippass_db.h"
#include "../kdbx/memzero.h"
#include "flippass_scene.h"
#include "flippass_scene_send_confirm.h"
#include "flippass_scene_status.h"
#include "flippass_scene_text_view.h"

#include <string.h>

enum {
    FlipPassSceneOtherFieldActionsEventRunAction = 0x200U,
};

typedef enum {
    FlipPassOtherFieldActionTypeBluetooth = 0,
    FlipPassOtherFieldActionShow,
    FlipPassOtherFieldActionTypeUsb,
} FlipPassOtherFieldAction;

static const char* flippass_other_field_safe_text(const char* value, const char* fallback) {
    return (value != NULL && value[0] != '\0') ? value : fallback;
}

static uint32_t flippass_other_field_ms_to_ticks(uint32_t ms) {
    const uint32_t tick_hz = furi_kernel_get_tick_frequency();
    if(tick_hz == 0U) {
        return 1U;
    }

    const uint64_t ticks = (((uint64_t)ms * tick_hz) + 999U) / 1000U;
    if(ticks == 0ULL) {
        return 1U;
    }
    return ticks > UINT32_MAX ? UINT32_MAX : (uint32_t)ticks;
}

static void flippass_other_field_clear_show_buffers(App* app) {
    memzero(app->status_title, sizeof(app->status_title));
    memzero(app->status_message, sizeof(app->status_message));
    memzero(app->text_view_title, sizeof(app->text_view_title));
    if(app->text_view_body != NULL) {
        const size_t body_len = furi_string_size(app->text_view_body);
        char* body = (char*)furi_string_get_cstr(app->text_view_body);
        if(body != NULL && body_len > 0U) {
            memzero(body, body_len);
        }
        furi_string_reset(app->text_view_body);
    }
}

static bool
    flippass_other_field_get_stored_value(App* app, const char** out_value, FuriString* error) {
    return flippass_db_get_other_field_value(
        app,
        app->active_entry,
        app->pending_other_field_mask,
        app->pending_other_custom_field,
        out_value,
        error);
}

void flippass_other_field_clear_show_state(App* app) {
    if(app == NULL) {
        return;
    }

    if(app->other_field_show_active) {
        flippass_other_field_clear_show_buffers(app);
    }
    app->other_field_show_active = false;
    app->other_field_show_totp_refresh = false;
    app->other_field_show_refresh_tick = 0U;
}

bool flippass_other_field_show_refresh_due(const App* app) {
    if(app == NULL || !app->other_field_show_active || !app->other_field_show_totp_refresh ||
       app->other_field_show_refresh_tick == 0U) {
        return false;
    }

    return (int32_t)(furi_get_tick() - app->other_field_show_refresh_tick) >= 0;
}

static bool flippass_other_field_prepare_show_value(
    App* app,
    uint32_t return_scene,
    bool* out_use_text_view,
    FuriString* error) {
    const char* value = NULL;
    char otp_code[FLIPPASS_OTP_CODE_MAX_CHARS + 1U];
    uint32_t refresh_ms = 0U;
    size_t value_len = 0U;
    bool ok = false;

    if(out_use_text_view != NULL) {
        *out_use_text_view = false;
    }
    otp_code[0] = '\0';
    flippass_other_field_clear_show_buffers(app);

    if(app->pending_other_otp_kind != FlipPassOtpKindNone) {
        kdbx_entry_clear_loaded_fields(app->active_entry);
        ok = flippass_otp_generate_code_with_refresh(
            app,
            app->active_entry,
            app->pending_other_otp_kind,
            false,
            otp_code,
            &refresh_ms,
            error);
        value = otp_code;
    } else {
        ok = flippass_other_field_get_stored_value(app, &value, error);
    }

    if(!ok) {
        memzero(otp_code, sizeof(otp_code));
        return false;
    }

    value_len = (value != NULL) ? strlen(value) : 0U;
    if(app->pending_other_field_mask == KDBXEntryFieldNotes || value_len > 80U) {
        flippass_scene_text_view_show(
            app,
            app->pending_other_field_name,
            flippass_other_field_safe_text(value, "Not set"),
            return_scene);
        if(out_use_text_view != NULL) {
            *out_use_text_view = true;
        }
    } else {
        flippass_scene_status_show(
            app,
            app->pending_other_field_name,
            flippass_other_field_safe_text(value, "Not set"),
            return_scene);
    }

    app->other_field_show_active = true;
    app->other_field_show_totp_refresh = app->pending_other_otp_kind == FlipPassOtpKindTime &&
                                         refresh_ms > 0U;
    app->other_field_show_refresh_tick =
        app->other_field_show_totp_refresh ?
            furi_get_tick() + flippass_other_field_ms_to_ticks(refresh_ms) :
            0U;
    if(app->active_entry != NULL) {
        kdbx_entry_clear_loaded_fields(app->active_entry);
    }
    memzero(otp_code, sizeof(otp_code));
    return true;
}

static void
    flippass_scene_other_field_actions_dialog_callback(DialogExResult result, void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, result);
}

static FlipPassEntryAction
    flippass_scene_other_field_actions_map_type_action(FlipPassOtherFieldAction action) {
    return (action == FlipPassOtherFieldActionTypeBluetooth) ?
               FlipPassEntryActionTypeOtherBluetooth :
               FlipPassEntryActionTypeOtherUsb;
}

static FlipPassOutputTransport flippass_scene_other_field_actions_transport(const App* app) {
    return (app->pending_entry_action == FlipPassEntryActionTypeOtherBluetooth) ?
               FlipPassOutputTransportBluetooth :
               FlipPassOutputTransportUsb;
}

static const char* flippass_scene_other_field_actions_typing_status_title(const App* app) {
    return flippass_scene_other_field_actions_transport(app) == FlipPassOutputTransportBluetooth ?
               "Bluetooth Typing Failed" :
               "USB Typing Failed";
}

void flippass_other_field_begin_type_action(App* app, bool bluetooth, uint32_t run_event) {
    const FlipPassOtherFieldAction action = bluetooth ? FlipPassOtherFieldActionTypeBluetooth :
                                                        FlipPassOtherFieldActionTypeUsb;

    flippass_other_field_clear_show_state(app);
    app->entry_action_keep_type_menu_prewarm = false;
    app->entry_action_restore_type_drawer = false;
    app->other_field_action_selected_index = (uint32_t)action;
    app->pending_entry_action = flippass_scene_other_field_actions_map_type_action(action);
    flippass_typing_begin(app);
    flippass_entry_action_prepare_pending(app);
    flippass_progress_begin(app, "Typing Field", "Connecting", 5U);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewLoading);
    view_dispatcher_send_custom_event(app->view_dispatcher, run_event);
}

void flippass_other_field_show_selected_value(App* app, uint32_t return_scene) {
    bool use_text_view = false;
    FuriString* error = furi_string_alloc();

    if(!flippass_other_field_prepare_show_value(app, return_scene, &use_text_view, error)) {
        flippass_scene_status_show(
            app, "Field Load Failed", furi_string_get_cstr(error), return_scene);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        furi_string_free(error);
        return;
    }

    if(use_text_view) {
        scene_manager_next_scene(app->scene_manager, FlipPassScene_TextView);
    } else {
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
    }

    furi_string_free(error);
}

bool flippass_other_field_refresh_visible_show(App* app, uint32_t return_scene) {
    bool use_text_view = false;
    FuriString* error = furi_string_alloc();
    const bool ok =
        flippass_other_field_prepare_show_value(app, return_scene, &use_text_view, error);
    UNUSED(use_text_view);

    if(!ok) {
        flippass_other_field_clear_show_state(app);
        flippass_scene_status_show(
            app, "Field Load Failed", furi_string_get_cstr(error), return_scene);
    }

    furi_string_free(error);
    return ok;
}

void flippass_other_field_run_pending_type_action(App* app, uint32_t failure_return_scene) {
    FuriString* error = furi_string_alloc();
    const bool typed = flippass_entry_action_execute_pending(app, error);
    const bool canceled = !typed && flippass_typing_should_cancel(app);

    flippass_typing_end(app);

    if(typed) {
        flippass_other_field_clear_show_state(app);
        app->entry_action_keep_type_menu_prewarm = false;
        app->entry_action_restore_type_drawer = false;
        flippass_progress_update(app, "Done", "Field sent.", 100U);
        flippass_progress_reset(app);
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, FlipPassScene_DbEntries);
    } else if(canceled) {
        flippass_other_field_clear_show_state(app);
        app->entry_action_keep_type_menu_prewarm = false;
        app->entry_action_restore_type_drawer = false;
        flippass_progress_reset(app);
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, FlipPassScene_DbEntries);
    } else {
        flippass_other_field_clear_show_state(app);
        app->entry_action_keep_type_menu_prewarm = false;
        app->entry_action_restore_type_drawer = false;
        flippass_progress_reset(app);
        flippass_scene_status_show(
            app,
            flippass_scene_other_field_actions_typing_status_title(app),
            furi_string_get_cstr(error),
            failure_return_scene);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
    }

    furi_string_free(error);
}

void flippass_scene_other_field_actions_on_enter(void* context) {
    App* app = context;

    if(app->active_entry == NULL ||
       (app->pending_other_field_mask == 0U && app->pending_other_custom_field == NULL &&
        app->pending_other_otp_kind == FlipPassOtpKindNone)) {
        flippass_scene_status_show(
            app,
            "No Field Selected",
            "Open an alternate field from the entry browser first.",
            FlipPassScene_DbEntries);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        return;
    }

    flippass_ui_ensure_dialog_ex(app);
    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_header(
        app->dialog_ex,
        flippass_other_field_safe_text(app->pending_other_field_name, "Other Field"),
        64,
        3,
        AlignCenter,
        AlignTop);
    dialog_ex_set_text(
        app->dialog_ex, "BT = type, Show = view, USB = type.", 64, 24, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(app->dialog_ex, "BT");
    dialog_ex_set_center_button_text(app->dialog_ex, "Show");
    dialog_ex_set_right_button_text(app->dialog_ex, "USB");
    dialog_ex_set_result_callback(
        app->dialog_ex, flippass_scene_other_field_actions_dialog_callback);
    dialog_ex_set_context(app->dialog_ex, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDialogEx);
}

bool flippass_scene_other_field_actions_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom && event.event == DialogExResultLeft) {
        flippass_other_field_begin_type_action(
            app, true, FlipPassSceneOtherFieldActionsEventRunAction);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom && event.event == DialogExResultCenter) {
        app->other_field_action_selected_index = FlipPassOtherFieldActionShow;
        flippass_other_field_show_selected_value(app, FlipPassScene_OtherFieldActions);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom && event.event == DialogExResultRight) {
        flippass_other_field_begin_type_action(
            app, false, FlipPassSceneOtherFieldActionsEventRunAction);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == FlipPassSceneOtherFieldActionsEventRunAction) {
        flippass_other_field_run_pending_type_action(app, FlipPassScene_OtherFieldActions);
        return true;
    }

    return false;
}

void flippass_scene_other_field_actions_on_exit(void* context) {
    App* app = context;
    if(app->dialog_ex != NULL) {
        dialog_ex_reset(app->dialog_ex);
    }
    flippass_output_release_all(app);
    flippass_output_cleanup(app);
}
