#include "../nfc_apdu_runner.h"
#include "nfc_apdu_runner_scene.h"

// 日志场景进入回调
void nfc_apdu_runner_scene_logs_on_enter(void* context) {
    NfcApduRunner* app = context;
    TextBox* text_box = app->text_box;

    text_box_reset(text_box);
    text_box_set_font(text_box, TextBoxFontText);

    FuriString* text = furi_string_alloc();

    furi_string_cat_str(text, "Execution Log:\n\n");

    for(uint32_t i = 0; i < app->log_count; i++) {
        if(app->log_entries[i].is_error) {
            furi_string_cat_str(text, "[ERROR] ");
        } else {
            furi_string_cat_str(text, "[INFO] ");
        }
        furi_string_cat_str(text, app->log_entries[i].message);
        furi_string_cat_str(text, "\n");
    }

    if(app->log_count == 0) {
        furi_string_cat_str(text, "No logs available.\nRun a script first.");
    }

    text_box_set_text(text_box, furi_string_get_cstr(text));
    furi_string_free(text);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcApduRunnerViewTextBox);
}

// 日志场景事件回调
bool nfc_apdu_runner_scene_logs_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

// 日志场景退出回调
void nfc_apdu_runner_scene_logs_on_exit(void* context) {
    NfcApduRunner* app = context;
    text_box_reset(app->text_box);
}
