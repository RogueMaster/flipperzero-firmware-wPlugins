
#include "ui.h"

#include <notification/notification_messages.h>

#define LOG_TAG "gs1_rfid_parser_raw_data_scene"

void raw_data_scene_on_enter(void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    UI* ui = context;

    widget_reset(ui->raw_data_scene->raw_data_widget);

    FuriString* data_str = furi_string_alloc();
    furi_string_reserve(data_str, 1024);
    furi_string_cat_printf(data_str, "\e#EPC");

    for(uint8_t i = 0; i < ui->epc_data->epc_size; i += 8) {
        char epc_bytes[28];
        epc_bytes[0] = '\n';
        epc_bytes[1] = '\e';
        epc_bytes[2] = '*';

        for(uint8_t j = 0; j < 8 && i + j < ui->epc_data->epc_size; j++) {
            snprintf(&epc_bytes[j * 2 + 3], 4, "%02X", ui->epc_data->epc_data[i + j]);
        }

        epc_bytes[27] = '\0';
        furi_string_cat_printf(data_str, epc_bytes);
    }

    if(ui->user_mem_data->is_valid) {
        furi_string_cat_printf(data_str, "\n\e#User Memory");

        for(uint8_t i = 0; i < ui->user_mem_data->user_mem_size; i += 8) {
            char user_mem_bytes[28];
            user_mem_bytes[0] = '\n';
            user_mem_bytes[1] = '\e';
            user_mem_bytes[2] = '*';

            for(uint8_t j = 0; j < 8 && i + j < ui->user_mem_data->user_mem_size; j++) {
                snprintf(
                    &user_mem_bytes[j * 2 + 3], 4, "%02X", ui->user_mem_data->user_mem_data[i + j]);
            }

            user_mem_bytes[27] = '\0';
            furi_string_cat_printf(data_str, user_mem_bytes);
        }
    }

    widget_add_text_scroll_element(
        ui->raw_data_scene->raw_data_widget, 0, 0, 128, 64, furi_string_get_cstr(data_str));
    furi_string_free(data_str);

    notification_message(ui->notifications, &sequence_set_blue_255);

    view_dispatcher_switch_to_view(ui->view_dispatcher, View_RawDataDisplay);
}

bool raw_data_scene_on_event(void* context, SceneManagerEvent event) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    if(event.type == SceneManagerEventTypeCustom) {
        return true;
    }

    // Default event handler
    return false;
}

void raw_data_scene_on_exit(void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    UI* ui = context;
    widget_reset(ui->raw_data_scene->raw_data_widget);
    notification_message(ui->notifications, &sequence_reset_blue);
}

RawDataScene* raw_data_scene_alloc() {
    FURI_LOG_T(LOG_TAG, __func__);

    RawDataScene* raw_data_scene = malloc(sizeof(RawDataScene));
    raw_data_scene->raw_data_widget = widget_alloc();

    return raw_data_scene;
}

void raw_data_scene_free(RawDataScene* raw_data_scene) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(raw_data_scene);

    widget_free(raw_data_scene->raw_data_widget);
    free(raw_data_scene);
}
