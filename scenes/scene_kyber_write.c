#include <gui/icon_i.h>

#include "../disney_toolbox_app.h"
#include "disney_toolbox_icons.h"

static const uint8_t kyber_write_animation_length = 4;
static const uint8_t kyber_write_animation_mod = 2;
static const Icon* kyber_write_animation[] = {
    &I_crystal_write_frame_0,
    &I_crystal_write_frame_1,
    &I_crystal_write_frame_2,
    &I_crystal_write_frame_3,
};

static void kyber_write_callback(LFRFIDWorkerWriteResult result, void* context) {
    DisneyToolboxApp* app = context;
    uint32_t event = 0;

    if (result == LFRFIDWorkerWriteOK) {
        event = DisneyToolboxEventWriteOK;
    } else if (result == LFRFIDWorkerWriteProtocolCannotBeWritten) {
        event = DisneyToolboxEventWriteProtocolCannotBeWritten;
    } else if (result == LFRFIDWorkerWriteFobCannotBeWritten) {
        event = DisneyToolboxEventWriteFobCannotBeWritten;
    } else if (result == LFRFIDWorkerWriteTooLongToWrite) {
        event = DisneyToolboxEventWriteTooLongToWrite;
    }

    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

void disney_toolbox_app_scene_kyber_write_on_enter(void* context) {
    DisneyToolboxApp* app = context;
    Popup* popup = app->popup;

    app->protocol_id = LFRFIDProtocolEM4100;

    popup_set_icon(popup, 5, 2,
                   kyber_write_animation[app->animation_counter / kyber_write_animation_mod]);
    popup_set_header(popup, "Writing", 94, 16, AlignCenter, AlignTop);
    const char* write_label;
    const uint8_t* em4100_data;
    if (app->kyber_series == 2) {
        write_label = CrystalsSeries2[app->selected_s2].crystal_color;
        em4100_data = CrystalsSeries2[app->selected_s2].em4100;
    } else {
        write_label = CrystalsSeries1[app->selected_s1].blade_color;
        em4100_data = CrystalsSeries1[app->selected_s1].em4100;
    }

    popup_set_text(popup, write_label, 94, 29, AlignCenter, AlignTop);

    const uint8_t write_data[5] = {0x00, 0x00, 0x00, em4100_data[0], em4100_data[1]};
    protocol_dict_set_data(app->protocol_dict, app->protocol_id, write_data, 5);

    popup_set_context(popup, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, DisneyToolboxAppViewPopup);

    lfrfid_worker_start_thread(app->lfworker);
    lfrfid_worker_write_start(app->lfworker, (LFRFIDProtocol)app->protocol_id, kyber_write_callback,
                              app);
    notification_message(app->notifications, &sequence_blink_start_magenta);
}

bool disney_toolbox_app_scene_kyber_write_on_event(void* context, SceneManagerEvent event) {
    DisneyToolboxApp* app = context;
    Popup* popup = app->popup;
    bool consumed = false;

    if (event.type == SceneManagerEventTypeCustom) {
        if (event.event == DisneyToolboxEventWriteOK) {
            notification_message(app->notifications, &sequence_success);
            scene_manager_next_scene(app->scene_manager, DisneyToolboxAppSceneKyberWriteSuccess);
            consumed = true;
        } else if (event.event == DisneyToolboxEventWriteProtocolCannotBeWritten) {
            app->write_animation_paused = true;
            popup_set_icon(popup, 0, 0, NULL);
            popup_set_header(popup, "Error", 64, 3, AlignCenter, AlignTop);
            popup_set_text(popup, "This protocol\ncannot be written", 3, 17, AlignLeft, AlignTop);
            notification_message(app->notifications, &sequence_blink_start_red);
            consumed = true;
        } else if ((event.event == DisneyToolboxEventWriteFobCannotBeWritten) ||
                   (event.event == DisneyToolboxEventWriteTooLongToWrite)) {
            app->write_animation_paused = true;
            popup_set_icon(popup, 0, 0, NULL);
            popup_set_header(popup, "Still Trying to Write...", 64, 0, AlignCenter, AlignTop);
            popup_set_text(popup,
                           "Make sure this\n"
                           "card is writable\n"
                           "and not protected",
                           0, 13, AlignLeft, AlignTop);
            notification_message(app->notifications, &sequence_blink_start_yellow);
            consumed = true;
        }
    } else if (event.type == SceneManagerEventTypeTick && !app->write_animation_paused) {
        if (app->animation_counter == 0) {
            app->animation_counter++;
            app->animation_counter_direction = false;
        } else if (app->animation_counter ==
                   ((kyber_write_animation_length * kyber_write_animation_mod) - 1)) {
            app->animation_counter--;
            app->animation_counter_direction = true;
        } else {
            app->animation_counter += app->animation_counter_direction ? -1 : 1;
        }
        popup_set_icon(popup, 5, 2,
                       kyber_write_animation[app->animation_counter / kyber_write_animation_mod]);
    }

    return consumed;
}

void disney_toolbox_app_scene_kyber_write_on_exit(void* context) {
    DisneyToolboxApp* app = context;
    notification_message(app->notifications, &sequence_blink_stop);
    popup_reset(app->popup);

    app->animation_counter = 0;
    app->animation_counter_direction = false;
    app->write_animation_paused = false;

    lfrfid_worker_stop(app->lfworker);
    lfrfid_worker_stop_thread(app->lfworker);
}
