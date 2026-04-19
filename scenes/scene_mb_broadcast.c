#include "../disney_toolbox_app.h"
#include "disney_toolbox_icons.h"

#define MB_BROADCAST_ANIMATION_FRAMES 5
#define MB_BROADCAST_ANIMATION_MOD 2

static const Icon* mb_broadcast_animation[] = {
    &I_broadcast_frame_1, &I_broadcast_frame_2, &I_broadcast_frame_3,
    &I_broadcast_frame_4, &I_broadcast_frame_5,
};

static void disney_toolbox_app_scene_mb_broadcast_dialog_callback(DialogExResult result,
                                                                  void* context) {
    DisneyToolboxApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, result);
}

static void disney_toolbox_app_scene_mb_broadcast_update(DisneyToolboxApp* app) {
    DialogEx* dialog_ex = app->dialog_ex;

    const char* header =
        app->preset_mode ? app->preset_name : magicband_code_info[app->selected_code_type].name;
    dialog_ex_set_header(dialog_ex, header, 64, 0, AlignCenter, AlignTop);

    FuriString* status = app->status_string;
    furi_string_reset(status);

    for (uint8_t i = 6; i < app->beacon_data_len; i++) {
        furi_string_cat_printf(status, "%02X", app->beacon_data[i]);
    }

    dialog_ex_set_text(dialog_ex, furi_string_get_cstr(status), 42, 30, AlignCenter, AlignCenter);

    dialog_ex_set_center_button_text(dialog_ex, app->is_beacon_active ? "Stop" : "Start");

    uint8_t frame = app->animation_counter / MB_BROADCAST_ANIMATION_MOD;
    if (frame >= MB_BROADCAST_ANIMATION_FRAMES) frame = MB_BROADCAST_ANIMATION_FRAMES - 1;
    dialog_ex_set_icon(dialog_ex, 86, 13, mb_broadcast_animation[frame]);

    dialog_ex_set_result_callback(dialog_ex, disney_toolbox_app_scene_mb_broadcast_dialog_callback);
    dialog_ex_set_context(dialog_ex, app);
}

static void disney_toolbox_app_apply_beacon_data(DisneyToolboxApp* app) {
    furi_hal_bt_extra_beacon_stop();

    if (!app->preset_mode) {
        app->beacon_data_len =
            magicband_code_generate(app->selected_code_type, &app->code_params, app->beacon_data);
    }

    furi_check(furi_hal_bt_extra_beacon_set_config(&app->beacon_config));
    furi_check(furi_hal_bt_extra_beacon_set_data(app->beacon_data, app->beacon_data_len));

    if (app->is_beacon_active) {
        furi_check(furi_hal_bt_extra_beacon_start());
    }
}

void disney_toolbox_app_scene_mb_broadcast_on_enter(void* context) {
    DisneyToolboxApp* app = context;

    disney_toolbox_app_scene_mb_broadcast_update(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, DisneyToolboxAppViewDialog);
}

bool disney_toolbox_app_scene_mb_broadcast_on_event(void* context, SceneManagerEvent event) {
    DisneyToolboxApp* app = context;
    bool consumed = false;

    if (event.type == SceneManagerEventTypeCustom) {
        if (event.event == DialogExResultCenter) {
            app->is_beacon_active = !app->is_beacon_active;

            disney_toolbox_app_apply_beacon_data(app);

            if (app->is_beacon_active) {
                notification_message_block(app->notifications, &sequence_set_blue_255);
            } else {
                notification_message_block(app->notifications, &sequence_reset_blue);
                app->animation_counter = 0;
            }

            disney_toolbox_app_scene_mb_broadcast_update(app);
            consumed = true;
        }
    } else if (event.type == SceneManagerEventTypeTick) {
        if (app->is_beacon_active) {
            uint8_t max = (MB_BROADCAST_ANIMATION_FRAMES * MB_BROADCAST_ANIMATION_MOD) - 1;
            app->animation_counter = (app->animation_counter + 1) % (max + 1);
            uint8_t frame = app->animation_counter / MB_BROADCAST_ANIMATION_MOD;
            dialog_ex_set_icon(app->dialog_ex, 86, 13, mb_broadcast_animation[frame]);
        }
        consumed = true;
    }

    return consumed;
}

void disney_toolbox_app_scene_mb_broadcast_on_exit(void* context) {
    DisneyToolboxApp* app = context;

    if (app->is_beacon_active) {
        furi_hal_bt_extra_beacon_stop();
        app->is_beacon_active = false;
        notification_message_block(app->notifications, &sequence_reset_blue);
    }

    app->preset_mode = false;
    app->animation_counter = 0;
    dialog_ex_reset(app->dialog_ex);
}
