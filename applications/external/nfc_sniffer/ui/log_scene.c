
#include "ui_i.h"
#include "missing_api.h"

#include <notification/notification_messages.h>

#define LOG_TAG "nfc_sniffer_log_scene"

#define LOG_TEXT_SIZE 4097

static void append_text(char* str, size_t length, UI* ui) {
    storage_file_write(ui->log_file, str, length);

    size_t log_text_length = furi_string_size(ui->log_scene->log_text) + length;

    // Prevent overflowing the logging buffer
    if(log_text_length >= LOG_TEXT_SIZE) {
        size_t cutOffset = log_text_length / 4;

        while(cutOffset < log_text_length &&
              furi_string_get_char(ui->log_scene->log_text, cutOffset) != '\n') {
            cutOffset++;
        }
        cutOffset++;

        furi_string_right(ui->log_scene->log_text, cutOffset);
    }

    furi_string_cat_printf(ui->log_scene->log_text, "%s", str);
    text_box_set_text(ui->log_scene->log_display, furi_string_get_cstr(ui->log_scene->log_text));

    notification_message(ui->notifications, &sequence_display_backlight_on);
}

static NfcCommand nfc_sniffer_callback(NfcLoggerEvent event, void* context) {
    FURI_LOG_T(LOG_TAG, __func__);

    UI* ui = (UI*)context;
    const BitBuffer* data = (const BitBuffer*)event.data;

    size_t buffer_length = bit_buffer_get_size_bytes(data);

    if(buffer_length == 0) {
        // Only got a EOF
        append_text("\n<EOF>", 6, ui);
    } else {
        char output[2 * buffer_length + 2];
        output[0] = '\n';
        output[2 * buffer_length + 2] = '\0';

        for(size_t i = 0; i < buffer_length; i++) {
            snprintf(&output[i * 2 + 1], 3, "%02X", bit_buffer_get_byte(data, i));
        }

        append_text(output, 2 * buffer_length + 1, ui);
    }

    return NfcCommandContinue;
}

void log_scene_on_enter(void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    UI* ui = context;

    ui->log_scene->nfc_sniffer = nfc_sniffer_alloc(ui->nfc, ui->nfc_protocol);
    nfc_sniffer_start(ui->log_scene->nfc_sniffer, nfc_sniffer_callback, ui);

    notification_message(ui->notifications, &sequence_blink_start_cyan);

    text_box_reset(ui->log_scene->log_display);
    text_box_set_font(ui->log_scene->log_display, TextBoxFontText);
    text_box_set_focus(ui->log_scene->log_display, TextBoxFocusEnd);
    text_box_set_text(ui->log_scene->log_display, furi_string_get_cstr(ui->log_scene->log_text));

    furi_string_reset(ui->log_scene->log_text);

    char title[25];
    snprintf(title, 25, "%s Commands:", PROTOCOL_NAMES[ui->nfc_protocol]);
    append_text(title, local_strnlen(title, 25), ui);

    view_dispatcher_switch_to_view(ui->view_dispatcher, View_LogDisplay);
}

bool log_scene_on_event(void* context, SceneManagerEvent event) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    if(event.type == SceneManagerEventTypeCustom) {
        return true;
    }

    // Default event handler
    return false;
}

void log_scene_on_exit(void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    UI* ui = context;
    text_box_reset(ui->log_scene->log_display);

    notification_message(ui->notifications, &sequence_blink_stop);

    nfc_sniffer_stop(ui->log_scene->nfc_sniffer);
    nfc_sniffer_free(ui->log_scene->nfc_sniffer);
}

LogScene* log_scene_alloc() {
    FURI_LOG_T(LOG_TAG, __func__);

    LogScene* log_scene = malloc(sizeof(LogScene));
    log_scene->log_display = text_box_alloc();

    log_scene->log_text = furi_string_alloc();
    furi_string_reserve(log_scene->log_text, LOG_TEXT_SIZE);

    return log_scene;
}

void log_scene_free(LogScene* log_scene) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(log_scene);

    text_box_free(log_scene->log_display);
    furi_string_free(log_scene->log_text);
    free(log_scene);
}
