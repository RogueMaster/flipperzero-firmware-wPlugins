#include "bitraw_analyzer_app.h"
#include "analyzer/sub_parser.h"
#include "analyzer/features.h"
#include "analyzer/classifier.h"
#include "analyzer/report.h"
#include <string.h>

#define TAG "BitRaw"

#define BITRAW_DEFAULT_BROWSE_PATH EXT_PATH("subghz")

#define HEAPLOG(stage)                                                  \
    FURI_LOG_I(                                                         \
        TAG,                                                            \
        "heap[%s]: free=%zu min=%zu",                                   \
        stage,                                                          \
        memmgr_get_free_heap(),                                         \
        memmgr_get_minimum_free_heap())

static bool bitraw_navigation_callback(void* context) {
    BitrawApp* app = context;
    FURI_LOG_I(TAG, "nav: stop dispatcher");
    view_dispatcher_stop(app->view_dispatcher);
    return true;
}

static BitrawApp* bitraw_app_alloc(void) {
    BitrawApp* app = malloc(sizeof(BitrawApp));
    furi_check(app, "BinRAW Analyzer: out of memory");
    memset(app, 0, sizeof(*app));

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, bitraw_navigation_callback);

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BitrawViewTextBox, text_box_get_view(app->text_box));

    app->selected_path = furi_string_alloc();
    app->report = furi_string_alloc();
    app->parse_error = furi_string_alloc();

    sub_file_init(&app->sub);

    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void bitraw_app_free(BitrawApp* app) {
    sub_file_reset(&app->sub);

    view_dispatcher_remove_view(app->view_dispatcher, BitrawViewTextBox);
    text_box_free(app->text_box);
    view_dispatcher_free(app->view_dispatcher);

    furi_string_free(app->selected_path);
    furi_string_free(app->report);
    furi_string_free(app->parse_error);

    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);

    free(app);
}

static bool bitraw_save_sidecar(BitrawApp* app, FuriString* out_path) {
    const char* src = furi_string_get_cstr(app->selected_path);
    if(!src || !*src) return false;

    furi_string_set(out_path, src);
    size_t dot = furi_string_search_rchar(out_path, '.', 0);
    if(dot != FURI_STRING_FAILURE) furi_string_left(out_path, dot);
    furi_string_cat_str(out_path, ".report.txt");
    FURI_LOG_I(TAG, "sidecar: %s", furi_string_get_cstr(out_path));

    File* file = storage_file_alloc(app->storage);
    bool ok = false;
    if(storage_file_open(
           file, furi_string_get_cstr(out_path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const char* text = furi_string_get_cstr(app->report);
        size_t len = strlen(text);
        ok = storage_file_write(file, text, len) == len;
    }
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

static bool bitraw_run_analysis(BitrawApp* app) {
    sub_file_reset(&app->sub);
    furi_string_reset(app->parse_error);
    furi_string_reset(app->report);

    HEAPLOG("analyze-start");

    const char* path = furi_string_get_cstr(app->selected_path);
    FURI_LOG_I(TAG, "parse: %s", path);
    SubParseStatus status =
        sub_parser_parse(app->storage, path, &app->sub, app->parse_error);
    FURI_LOG_I(
        TAG,
        "parse end status=%d segs=%u truncated=%d",
        (int)status,
        app->sub.segment_count,
        app->sub.truncated);
    if(status != SubParseOk && status != SubParseTruncated) return false;

    FURI_LOG_I(TAG, "features");
    features_extract(&app->sub, &app->fv);

    FURI_LOG_I(TAG, "classify");
    classifier_run(&app->fv, &app->result);
    FURI_LOG_I(TAG, "label=%d", (int)app->result.label);

    if(app->sub.truncated) {
        classifier_add_warning(
            &app->result,
            "Capture exceeded on-device limits - analysis used a truncated subset");
    }

    FURI_LOG_I(TAG, "report");
    report_format(path, &app->sub, &app->fv, &app->result, app->report);

    FuriString* sidecar_path = furi_string_alloc();
    bool saved = bitraw_save_sidecar(app, sidecar_path);
    furi_string_cat_printf(
        app->report,
        saved ? "Report saved: %s\n" : "Report save FAILED: %s\n",
        furi_string_get_cstr(sidecar_path));
    furi_string_free(sidecar_path);

    HEAPLOG("analyze-end");
    return true;
}

static bool bitraw_pick_file(BitrawApp* app) {
    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, ".sub", NULL);
    options.base_path = BITRAW_DEFAULT_BROWSE_PATH;

    FuriString* preselect = furi_string_alloc_set(BITRAW_DEFAULT_BROWSE_PATH);
    bool picked =
        dialog_file_browser_show(app->dialogs, app->selected_path, preselect, &options);
    furi_string_free(preselect);
    return picked;
}

int32_t bitraw_analyzer_app(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "=== app start ===");
    BitrawApp* app = bitraw_app_alloc();
    HEAPLOG("post-alloc");

    while(true) {
        FURI_LOG_I(TAG, "loop: file picker");
        if(!bitraw_pick_file(app)) {
            FURI_LOG_I(TAG, "loop: picker cancelled, exit");
            break;
        }
        FURI_LOG_I(TAG, "loop: picked %s", furi_string_get_cstr(app->selected_path));

        bool ok = bitraw_run_analysis(app);
        const char* text;
        if(ok) {
            text = furi_string_get_cstr(app->report);
        } else if(!furi_string_empty(app->parse_error)) {
            text = furi_string_get_cstr(app->parse_error);
        } else {
            text = "Could not read .sub file";
        }
        FURI_LOG_I(TAG, "loop: show text (ok=%d, len=%u)", ok, (unsigned)strlen(text));

        text_box_reset(app->text_box);
        text_box_set_font(app->text_box, TextBoxFontText);
        text_box_set_focus(app->text_box, TextBoxFocusStart);
        text_box_set_text(app->text_box, text);
        view_dispatcher_switch_to_view(app->view_dispatcher, BitrawViewTextBox);

        FURI_LOG_I(TAG, "loop: dispatcher run");
        view_dispatcher_run(app->view_dispatcher);
        FURI_LOG_I(TAG, "loop: dispatcher returned (back pressed)");
    }

    bitraw_app_free(app);
    FURI_LOG_I(TAG, "=== app exit ===");
    return 0;
}
