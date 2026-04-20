#include "../disney_toolbox_app.h"
#include "disney_toolbox_icons.h"

#define LOC_BC_ANIMATION_FRAMES 5
#define LOC_BC_ANIMATION_MOD 2

static const Icon* loc_bc_animation[] = {
    &I_location_frame_0,  //
    &I_location_frame_1,  //
    &I_location_frame_2,  //
    &I_location_frame_3,  //
    &I_location_frame_4,  //
};

static void droid_location_broadcast_callback(DialogExResult result, void* context) {
    DisneyToolboxApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, result);
}

static void droid_location_broadcast_update(DisneyToolboxApp* app) {
    DialogEx* dialog_ex = app->dialog_ex;

    dialog_ex_set_header(dialog_ex, "Location Broadcaster", 64, 0, AlignCenter, AlignTop);

    FuriString* status = app->status_string;
    furi_string_reset(status);

    const DroidLocationInfo* info = &droid_location_info[app->selected_droid_location];
    furi_string_cat_printf(status, "Location: %s\n", info->long_name);
    furi_string_cat_printf(status, "Interval: %s\n",
                           loc_interval_info[app->droid_loc_interval_idx].name);
    furi_string_cat_printf(status, "Distance: %s",
                           loc_rssi_info[app->droid_loc_rssi_idx].long_name);

    dialog_ex_set_text(dialog_ex, furi_string_get_cstr(status), 0, 11, AlignLeft, AlignTop);

    dialog_ex_set_center_button_text(dialog_ex, "Stop");

    uint8_t frame = app->animation_counter / LOC_BC_ANIMATION_MOD;
    if (frame >= LOC_BC_ANIMATION_FRAMES) frame = LOC_BC_ANIMATION_FRAMES - 1;
    dialog_ex_set_icon(dialog_ex, 90, 25, loc_bc_animation[frame]);

    dialog_ex_set_result_callback(dialog_ex, droid_location_broadcast_callback);
    dialog_ex_set_context(dialog_ex, app);
}

void disney_toolbox_app_scene_droid_location_broadcast_on_enter(void* context) {
    DisneyToolboxApp* app = context;

    // Start broadcasting
    furi_hal_bt_extra_beacon_stop();
    furi_check(furi_hal_bt_extra_beacon_set_config(&app->beacon_config));
    furi_check(furi_hal_bt_extra_beacon_set_data(app->beacon_data, app->beacon_data_len));
    furi_check(furi_hal_bt_extra_beacon_start());
    app->is_beacon_active = true;
    notification_message_block(app->notifications, &sequence_set_blue_255);

    droid_location_broadcast_update(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, DisneyToolboxAppViewDialog);
}

bool disney_toolbox_app_scene_droid_location_broadcast_on_event(void* context,
                                                                SceneManagerEvent event) {
    DisneyToolboxApp* app = context;
    bool consumed = false;

    if (event.type == SceneManagerEventTypeCustom) {
        if (event.event == DialogExResultCenter) {
            furi_hal_bt_extra_beacon_stop();
            app->is_beacon_active = false;
            notification_message_block(app->notifications, &sequence_reset_blue);
            app->animation_counter = 0;
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    } else if (event.type == SceneManagerEventTypeTick) {
        if (app->is_beacon_active) {
            uint8_t max = (LOC_BC_ANIMATION_FRAMES * LOC_BC_ANIMATION_MOD) - 1;
            app->animation_counter = (app->animation_counter + 1) % (max + 1);
            uint8_t frame = app->animation_counter / LOC_BC_ANIMATION_MOD;
            dialog_ex_set_icon(app->dialog_ex, 90, 25, loc_bc_animation[frame]);
        }
        consumed = true;
    }

    return consumed;
}

void disney_toolbox_app_scene_droid_location_broadcast_on_exit(void* context) {
    DisneyToolboxApp* app = context;

    if (app->is_beacon_active) {
        furi_hal_bt_extra_beacon_stop();
        app->is_beacon_active = false;
        notification_message_block(app->notifications, &sequence_reset_blue);
    }

    app->animation_counter = 0;
    dialog_ex_reset(app->dialog_ex);
}
