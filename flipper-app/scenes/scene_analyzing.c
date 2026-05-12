#include "scenes.h"
#include <string.h>
#include "../analyzer/sub_parser.h"
#include "../analyzer/features.h"
#include "../analyzer/classifier.h"
#include "../analyzer/report.h"

static bool bitraw_save_sidecar(BitrawApp* app, FuriString* out_path) {
    const char* src = furi_string_get_cstr(app->selected_path);
    if(!src || !*src) return false;

    furi_string_set(out_path, src);
    size_t dot = furi_string_search_rchar(out_path, '.', 0);
    if(dot != FURI_STRING_FAILURE) {
        furi_string_left(out_path, dot);
    }
    furi_string_cat_str(out_path, ".report.txt");

    File* file = storage_file_alloc(app->storage);
    bool ok = false;
    if(storage_file_open(
           file, furi_string_get_cstr(out_path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const char* text = furi_string_get_cstr(app->report);
        size_t len = strlen(text);
        size_t written = storage_file_write(file, text, len);
        ok = (written == len);
    }
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

void bitraw_scene_analyzing_on_enter(void* context) {
    BitrawApp* app = context;

    popup_reset(app->popup);
    popup_set_header(app->popup, "Analyzing", 64, 26, AlignCenter, AlignTop);
    popup_set_text(app->popup, "Parsing capture...", 64, 42, AlignCenter, AlignTop);
    popup_disable_timeout(app->popup);
    view_dispatcher_switch_to_view(app->view_dispatcher, BitrawViewPopup);

    sub_file_reset(&app->sub);
    furi_string_reset(app->parse_error);
    furi_string_reset(app->report);

    const char* path = furi_string_get_cstr(app->selected_path);
    SubParseStatus status = sub_parser_parse(app->storage, path, &app->sub, app->parse_error);

    bool ok = (status == SubParseOk || status == SubParseTruncated);
    if(ok) {
        features_extract(&app->sub, &app->fv);
        classifier_run(&app->fv, &app->result);
        if(app->sub.truncated) {
            classifier_add_warning(
                &app->result,
                "Capture exceeded on-device limits — analysis used a truncated subset");
        }
        report_format(path, &app->sub, &app->fv, &app->result, app->report);

        FuriString* sidecar_path = furi_string_alloc();
        bool saved = bitraw_save_sidecar(app, sidecar_path);
        furi_string_cat_printf(
            app->report,
            saved ? "Report saved: %s\n" : "Report save FAILED: %s\n",
            furi_string_get_cstr(sidecar_path));
        furi_string_free(sidecar_path);
    }

    view_dispatcher_send_custom_event(
        app->view_dispatcher, ok ? BitrawCustomEventAnalyzeDone : BitrawCustomEventAnalyzeFailed);
}

bool bitraw_scene_analyzing_on_event(void* context, SceneManagerEvent event) {
    BitrawApp* app = context;
    if(event.type == SceneManagerEventTypeBack) {
        return scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, BitrawSceneBrowser);
    }
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == BitrawCustomEventAnalyzeDone) {
            scene_manager_next_scene(app->scene_manager, BitrawSceneResult);
            return true;
        }
        if(event.event == BitrawCustomEventAnalyzeFailed) {
            popup_reset(app->popup);
            popup_set_header(app->popup, "Parse failed", 64, 14, AlignCenter, AlignTop);
            const char* msg = furi_string_empty(app->parse_error)
                                  ? "Could not read .sub file"
                                  : furi_string_get_cstr(app->parse_error);
            popup_set_text(app->popup, msg, 64, 30, AlignCenter, AlignTop);
            popup_set_timeout(app->popup, 2500);
            popup_enable_timeout(app->popup);
            return true;
        }
    }
    return false;
}

void bitraw_scene_analyzing_on_exit(void* context) {
    BitrawApp* app = context;
    popup_reset(app->popup);
}
