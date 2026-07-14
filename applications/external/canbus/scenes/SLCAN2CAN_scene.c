#include "app_user.h"

#define TAG "SLCAN 2 CAN"

bool started = false;

int32_t thread_SLCAN_callback(void* context) {
    App* app = context;

    FuriString* SLCAN_received = furi_string_alloc();
    FuriString* can_id_received = furi_string_alloc();
    FuriString* dlc_received = furi_string_alloc();
    CANFRAME frame_received;
    uint8_t* buffer = calloc(30, sizeof(buffer));

    while(true) {
        uint32_t events = furi_thread_flags_get();
        if(events & THREAD_SLCAN_STOP) {
            break;
        }

        uint32_t num_recived = furi_hal_cdc_receive(SLCAN_CDC_NUM, buffer, 30);
        if(num_recived && (*buffer == 't' || *buffer == 'T')) {
            furi_string_set_str(SLCAN_received, (char*)buffer);

            furi_string_set(can_id_received, SLCAN_received);
            furi_string_set(dlc_received, SLCAN_received);

            frame_received.ext = *buffer == 'T';

            furi_string_mid(can_id_received, 1, frame_received.ext ? 8 : 3);

            frame_received.data_lenght =
                (uint8_t)(furi_string_get_char(SLCAN_received, frame_received.ext ? 9 : 4) - '0');

            furi_string_mid(
                dlc_received, frame_received.ext ? 10 : 5, frame_received.data_lenght * 2);

            if(frame_received.ext) *(char*)furi_string_get_cstr(can_id_received) -= 8;

            for(int i = 0; i < frame_received.data_lenght * 2; i += 2) {
                frame_received.buffer[i / 2] = hex2uint8(
                    (char*)furi_string_get_cstr(dlc_received) + i,
                    (char*)furi_string_get_cstr(dlc_received) + i + 1);
            }

            frame_received.canId = 0;
            for(int i = 0; i < (frame_received.ext ? 8 : 3); i++) {
                frame_received.canId +=
                    hex2uint8_nibble((char*)(furi_string_get_cstr(can_id_received) + i))
                    << 4 * ((frame_received.ext ? 7 : 2) - i);
            }

            frame_can_queue_push(app->frame_queue, frame_received);
        }

        for(uint8_t i = 0; i < num_recived; i++)
            *(buffer + i) = '\0';
    }

    free(buffer);
    furi_string_free(dlc_received);
    furi_string_free(can_id_received);
    furi_string_free(SLCAN_received);

    return 0;
}

void dialog_SLCAN_callback(DialogExResult result, void* context) {
    App* app = context;

    switch(result) {
    case DialogExResultCenter:
        if(started) {
            started = false;

            dialog_ex_set_header(
                app->dialog_ex, "Start SLCAN-CAN\ncommunication", 64, 30, AlignCenter, AlignCenter);
            dialog_ex_set_center_button_text(app->dialog_ex, "Start");

            furi_thread_flags_set(furi_thread_get_id(app->thread_SLCAN), THREAD_SLCAN_STOP);
            furi_thread_join(app->thread_SLCAN);

            furi_thread_flags_set(furi_thread_get_id(app->thread), THREAD_SNIFFER_STOP);
            furi_thread_join(app->thread);

        } else {
            started = true;

            dialog_ex_set_header(
                app->dialog_ex, "Stop communication", 64, 30, AlignCenter, AlignCenter);
            dialog_ex_set_center_button_text(app->dialog_ex, "Stop");

            furi_thread_start(app->thread_SLCAN);
            furi_thread_start(app->thread);
        }
        break;
    default:
        break;
    }
}

void app_scene_SLCAN_2_CAN_on_enter(void* context) {
    furi_assert(context);
    App* app = context;

    if(mcp2515_init(app->mcp_can) != ERROR_OK) {
        draw_device_no_connected(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
    } else {
        *app->can_send_frame = true;

        app->thread_SLCAN = furi_thread_alloc_ex("SLCAN", 1024, thread_SLCAN_callback, app);
        app->thread = furi_thread_alloc_ex("SniffingWork", 1024, worker_sniffing, app);

        dialog_ex_reset(app->dialog_ex);
        dialog_ex_set_context(app->dialog_ex, app);
        dialog_ex_set_result_callback(app->dialog_ex, dialog_SLCAN_callback);

        dialog_ex_set_header(
            app->dialog_ex, "Start SLCAN-CAN\ncommunication", 64, 30, AlignCenter, AlignCenter);

        dialog_ex_set_center_button_text(app->dialog_ex, "Start");

        view_dispatcher_switch_to_view(app->view_dispatcher, DialogView);
    }
}

bool app_scene_SLCAN_2_CAN_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    bool consumed = false;

    return consumed;
}

void app_scene_SLCAN_2_CAN_on_exit(void* context) {
    furi_assert(context);
    App* app = context;

    *app->can_send_frame = false;

    if(started) {
        furi_thread_flags_set(furi_thread_get_id(app->thread_SLCAN), THREAD_SLCAN_STOP);
        furi_thread_join(app->thread_SLCAN);
        furi_thread_free(app->thread_SLCAN);

        furi_thread_flags_set(furi_thread_get_id(app->thread), THREAD_SNIFFER_STOP);
        furi_thread_join(app->thread);
        furi_thread_free(app->thread);

        started = false;
    }

    dialog_ex_reset(app->dialog_ex);
    widget_reset(app->widget);
}
