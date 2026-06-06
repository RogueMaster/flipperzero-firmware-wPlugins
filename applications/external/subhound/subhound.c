#include "subhound_app.h"
#include "analyzer/sub_parser.h"
#include "analyzer/features.h"
#include "analyzer/classifier.h"
#include "analyzer/report.h"
#include <string.h>

#define TAG "Subhound"

#define SUBHOUND_DEFAULT_BROWSE_PATH EXT_PATH("subghz")

#define HEAPLOG(stage)                \
    FURI_LOG_I(                       \
        TAG,                          \
        "heap[%s]: free=%zu min=%zu", \
        stage,                        \
        memmgr_get_free_heap(),       \
        memmgr_get_minimum_free_heap())

/* Custom event IDs dispatched by Widget buttons / Submenu picks. */
typedef enum {
    SubhoundEvtOpenSections = 100,
    SubhoundEvtOpenFullReport,
    SubhoundEvtOpenMetrics,
    SubhoundEvtOpenReasoning,
    SubhoundEvtOpenPayload,
    SubhoundEvtOpenManchester,
    SubhoundEvtOpenWarnings,
} SubhoundEvt;

/* Tracks which view is showing so back-button can route per-view. */
static SubhoundView s_current_view;
static SubhoundView s_textbox_came_from;

static void subhound_switch_view(SubhoundApp* app, SubhoundView view) {
    s_current_view = view;
    view_dispatcher_switch_to_view(app->view_dispatcher, view);
}

/* ============================ summary card ============================ */

static void summary_button_cb(GuiButtonType type, InputType input, void* context) {
    if(input != InputTypeShort) return;
    SubhoundApp* app = context;
    if(type == GuiButtonTypeLeft) {
        /* Back to file picker. */
        view_dispatcher_stop(app->view_dispatcher);
    } else if(type == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, SubhoundEvtOpenSections);
    } else {
        view_dispatcher_send_custom_event(app->view_dispatcher, SubhoundEvtOpenFullReport);
    }
}

/* Persistent buffer for the scroll-text element. Widget element copies the
 * char*'s value at render time, so this must outlive widget_reset cycles. */
static char s_summary_body[256];

/* Build the at-a-glance overview card. 128x64 canvas, hand-laid.
 *
 * Layout:
 *   y=0-10  : classification label (FontPrimary, full width)
 *   y=11-18 : confidence + warnings count (FontSecondary)
 *   y=19    : horizontal separator
 *   y=20-49 : scrollable body (30px high)
 *   y=50-63 : button hints (Back / Menu / Full)
 */
static void subhound_build_summary(SubhoundApp* app) {
    widget_reset(app->summary);

    const char* label = subhound_label_name(app->result.label);
    const char* conf = subhound_confidence_name(app->result.confidence);

    /* Row 1: classification label on its own line. */
    widget_add_string_element(app->summary, 0, 0, AlignLeft, AlignTop, FontPrimary, label);

    /* Row 2: confidence (left) + warnings indicator (right). */
    widget_add_string_element(app->summary, 0, 11, AlignLeft, AlignTop, FontSecondary, conf);
    if(app->result.warning_count > 0) {
        char warn[16];
        snprintf(
            warn,
            sizeof(warn),
            "!%u warning%s",
            app->result.warning_count,
            app->result.warning_count == 1 ? "" : "s");
        widget_add_string_element(
            app->summary, 127, 11, AlignRight, AlignTop, FontSecondary, warn);
    }

    /* Separator below the header. */
    // widget_add_line_element(app->summary, 0, 19, 127, 19);

    /* Body: compact summary lines, scrollable when overflowing 30px. */
    FuriString* body = furi_string_alloc();
    report_format_summary_lines(&app->fv, &app->result, body);
    /* Strip a trailing newline so the scroll element doesn't render an extra empty row. */
    size_t blen = furi_string_size(body);
    while(blen > 0 && furi_string_get_char(body, blen - 1) == '\n') {
        furi_string_left(body, blen - 1);
        blen--;
    }
    strncpy(s_summary_body, furi_string_get_cstr(body), sizeof(s_summary_body) - 1);
    s_summary_body[sizeof(s_summary_body) - 1] = '\0';
    furi_string_free(body);
    widget_add_text_scroll_element(app->summary, 0, 21, 128, 28, s_summary_body);

    /* Bottom button hints. Keep all three labels <=4 chars so they don't
     * overlap on a 128px row (Center is rendered as a pill with padding). */
    widget_add_button_element(app->summary, GuiButtonTypeLeft, "Back", summary_button_cb, app);
    widget_add_button_element(app->summary, GuiButtonTypeCenter, "Menu", summary_button_cb, app);
    widget_add_button_element(app->summary, GuiButtonTypeRight, "Full", summary_button_cb, app);
}

/* ============================ sections menu =========================== */

static void sections_item_cb(void* context, uint32_t event_id) {
    SubhoundApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event_id);
}

static void subhound_build_sections(SubhoundApp* app) {
    submenu_reset(app->sections);
    submenu_set_header(app->sections, subhound_label_name(app->result.label));

    submenu_add_item(
        app->sections, "Reasoning chain", SubhoundEvtOpenReasoning, sections_item_cb, app);
    submenu_add_item(app->sections, "Key metrics", SubhoundEvtOpenMetrics, sections_item_cb, app);

    if(report_section_has_content(ReportSectionPayload, &app->fv, &app->result)) {
        submenu_add_item(
            app->sections, "Payload (hex + bits)", SubhoundEvtOpenPayload, sections_item_cb, app);
    }
    if(report_section_has_content(ReportSectionManchester, &app->fv, &app->result)) {
        submenu_add_item(
            app->sections, "Manchester decode", SubhoundEvtOpenManchester, sections_item_cb, app);
    }
    if(report_section_has_content(ReportSectionWarnings, &app->fv, &app->result)) {
        char label[24];
        snprintf(label, sizeof(label), "Warnings (%u)", app->result.warning_count);
        submenu_add_item(app->sections, label, SubhoundEvtOpenWarnings, sections_item_cb, app);
    }
    submenu_add_item(
        app->sections, "Full report", SubhoundEvtOpenFullReport, sections_item_cb, app);
}

static void subhound_show_section(SubhoundApp* app, ReportSection section, SubhoundView origin) {
    furi_string_reset(app->section_text);
    report_format_section(
        section,
        furi_string_get_cstr(app->selected_path),
        &app->sub,
        &app->fv,
        &app->result,
        app->section_text);
    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_focus(app->text_box, TextBoxFocusStart);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->section_text));
    s_textbox_came_from = origin;
    subhound_switch_view(app, SubhoundViewTextBox);
}

/* ====================== custom event + navigation ===================== */

static bool subhound_custom_event_cb(void* context, uint32_t event) {
    SubhoundApp* app = context;
    switch(event) {
    case SubhoundEvtOpenSections:
        subhound_switch_view(app, SubhoundViewSections);
        return true;
    case SubhoundEvtOpenFullReport:
        text_box_reset(app->text_box);
        text_box_set_font(app->text_box, TextBoxFontText);
        text_box_set_focus(app->text_box, TextBoxFocusStart);
        text_box_set_text(app->text_box, furi_string_get_cstr(app->report));
        s_textbox_came_from = (s_current_view == SubhoundViewSections) ? SubhoundViewSections :
                                                                         SubhoundViewSummary;
        subhound_switch_view(app, SubhoundViewTextBox);
        return true;
    case SubhoundEvtOpenMetrics:
        subhound_show_section(app, ReportSectionMetrics, SubhoundViewSections);
        return true;
    case SubhoundEvtOpenReasoning:
        subhound_show_section(app, ReportSectionReasoning, SubhoundViewSections);
        return true;
    case SubhoundEvtOpenPayload:
        subhound_show_section(app, ReportSectionPayload, SubhoundViewSections);
        return true;
    case SubhoundEvtOpenManchester:
        subhound_show_section(app, ReportSectionManchester, SubhoundViewSections);
        return true;
    case SubhoundEvtOpenWarnings:
        subhound_show_section(app, ReportSectionWarnings, SubhoundViewSections);
        return true;
    }
    return false;
}

static bool subhound_navigation_callback(void* context) {
    SubhoundApp* app = context;
    switch(s_current_view) {
    case SubhoundViewLoading:
        /* Don't interrupt analysis. */
        return true;
    case SubhoundViewSummary:
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    case SubhoundViewSections:
        subhound_switch_view(app, SubhoundViewSummary);
        return true;
    case SubhoundViewTextBox:
        subhound_switch_view(app, s_textbox_came_from);
        return true;
    }
    return false;
}

/* ========================== analysis + sidecar ======================== */

static bool subhound_save_sidecar(SubhoundApp* app, FuriString* out_path) {
    const char* src = furi_string_get_cstr(app->selected_path);
    if(!src || !*src) return false;

    furi_string_set(out_path, src);
    size_t dot = furi_string_search_rchar(out_path, '.', 0);
    if(dot != FURI_STRING_FAILURE) furi_string_left(out_path, dot);
    furi_string_cat_str(out_path, ".report.txt");
    FURI_LOG_I(TAG, "sidecar: %s", furi_string_get_cstr(out_path));

    File* file = storage_file_alloc(app->storage);
    bool ok = false;
    if(storage_file_open(file, furi_string_get_cstr(out_path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const char* text = furi_string_get_cstr(app->report);
        size_t len = strlen(text);
        ok = storage_file_write(file, text, len) == len;
    }
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

/* Write a machine-readable metadata sidecar (<stem>.bra) next to the .sub. */
static bool subhound_save_bra_metadata(SubhoundApp* app) {
    const char* src = furi_string_get_cstr(app->selected_path);
    if(!src || !*src) return false;

    FuriString* path = furi_string_alloc_set(src);
    size_t dot = furi_string_search_rchar(path, '.', 0);
    if(dot != FURI_STRING_FAILURE) furi_string_left(path, dot);
    furi_string_cat_str(path, ".bra");

    FuriString* body = furi_string_alloc();
    furi_string_cat_printf(body, "label=%s\n", subhound_label_name(app->result.label));
    furi_string_cat_printf(
        body, "confidence=%s\n", subhound_confidence_name(app->result.confidence));
    furi_string_cat_printf(body, "frequency_hz=%lu\n", (unsigned long)app->fv.frequency);
    furi_string_cat_printf(body, "te_us=%.0f\n", (double)app->fv.te_us);
    furi_string_cat_printf(body, "seg_count=%u\n", app->fv.seg_count);
    furi_string_cat_printf(body, "signal_quality=%.3f\n", (double)app->fv.signal_quality);
    furi_string_cat_printf(body, "rolling_code=%d\n", app->fv.rolling_code ? 1 : 0);
    furi_string_cat_printf(body, "fixed_code=%d\n", app->fv.fixed_code ? 1 : 0);
    furi_string_cat_printf(body, "crc_valid=%d\n", app->fv.crc_valid ? 1 : 0);
    if(app->fv.pwm_decoded_count > 0) {
        furi_string_cat_str(body, "payload_hex=");
        uint16_t total_bits = (uint16_t)((app->fv.pwm_decoded_count + 7u) / 8u * 8u);
        for(uint16_t i = 0; i < total_bits; i += 8) {
            uint8_t byte = 0;
            for(uint8_t j = 0; j < 8; j++) {
                uint8_t bit = (uint16_t)(i + j) < app->fv.pwm_decoded_count ?
                                  app->fv.pwm_decoded_bits[i + j] :
                                  0;
                byte = (uint8_t)((byte << 1) | (bit & 1u));
            }
            furi_string_cat_printf(body, i == 0 ? "%02X" : " %02X", byte);
        }
        furi_string_cat_str(body, "\n");
    }
    if(app->fv.has_gps) {
        furi_string_cat_printf(
            body, "lat=%.6f\nlon=%.6f\n", (double)app->fv.lat, (double)app->fv.lon);
    }

    File* file = storage_file_alloc(app->storage);
    bool ok = false;
    if(storage_file_open(file, furi_string_get_cstr(path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const char* text = furi_string_get_cstr(body);
        size_t len = strlen(text);
        ok = storage_file_write(file, text, len) == len;
    }
    storage_file_close(file);
    storage_file_free(file);
    FURI_LOG_I(TAG, "bra sidecar: %s (%s)", furi_string_get_cstr(path), ok ? "ok" : "fail");

    furi_string_free(body);
    furi_string_free(path);
    return ok;
}

static bool subhound_run_analysis(SubhoundApp* app) {
    sub_file_reset(&app->sub);
    furi_string_reset(app->parse_error);
    furi_string_reset(app->report);

    HEAPLOG("analyze-start");

    const char* path = furi_string_get_cstr(app->selected_path);
    FURI_LOG_I(TAG, "parse: %s", path);
    SubParseStatus status = sub_parser_parse(app->storage, path, &app->sub, app->parse_error);
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
            &app->result, "Capture exceeded on-device limits - analysis used a truncated subset");
    }

    FURI_LOG_I(TAG, "report");
    report_format(path, &app->sub, &app->fv, &app->result, app->report);

    FuriString* sidecar_path = furi_string_alloc();
    bool saved = subhound_save_sidecar(app, sidecar_path);
    furi_string_cat_printf(
        app->report,
        saved ? "Report saved: %s\n" : "Report save FAILED: %s\n",
        furi_string_get_cstr(sidecar_path));
    furi_string_free(sidecar_path);

    subhound_save_bra_metadata(app);

    HEAPLOG("analyze-end");
    return true;
}

/* ============================ alloc / free ============================ */

static SubhoundApp* subhound_app_alloc(void) {
    SubhoundApp* app = malloc(sizeof(SubhoundApp));
    furi_check(app, "Subhound: out of memory");
    memset(app, 0, sizeof(*app));

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, subhound_navigation_callback);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, subhound_custom_event_cb);

    app->loading = loading_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SubhoundViewLoading, loading_get_view(app->loading));

    app->summary = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SubhoundViewSummary, widget_get_view(app->summary));

    app->sections = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SubhoundViewSections, submenu_get_view(app->sections));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SubhoundViewTextBox, text_box_get_view(app->text_box));

    app->selected_path = furi_string_alloc();
    app->report = furi_string_alloc();
    app->section_text = furi_string_alloc();
    app->parse_error = furi_string_alloc();

    sub_file_init(&app->sub);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void subhound_app_free(SubhoundApp* app) {
    sub_file_reset(&app->sub);

    view_dispatcher_remove_view(app->view_dispatcher, SubhoundViewTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, SubhoundViewSections);
    view_dispatcher_remove_view(app->view_dispatcher, SubhoundViewSummary);
    view_dispatcher_remove_view(app->view_dispatcher, SubhoundViewLoading);
    text_box_free(app->text_box);
    submenu_free(app->sections);
    widget_free(app->summary);
    loading_free(app->loading);
    view_dispatcher_free(app->view_dispatcher);

    furi_string_free(app->selected_path);
    furi_string_free(app->report);
    furi_string_free(app->section_text);
    furi_string_free(app->parse_error);

    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);

    free(app);
}

/* ============================== run loop ============================== */

static bool subhound_pick_file(SubhoundApp* app) {
    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, ".sub", NULL);
    options.base_path = SUBHOUND_DEFAULT_BROWSE_PATH;

    FuriString* preselect = furi_string_alloc_set(SUBHOUND_DEFAULT_BROWSE_PATH);
    bool picked = dialog_file_browser_show(app->dialogs, app->selected_path, preselect, &options);
    furi_string_free(preselect);
    return picked;
}

static void subhound_show_parse_error(SubhoundApp* app, const char* fallback) {
    const char* text =
        !furi_string_empty(app->parse_error) ? furi_string_get_cstr(app->parse_error) : fallback;
    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_focus(app->text_box, TextBoxFocusStart);
    text_box_set_text(app->text_box, text);
    s_textbox_came_from = SubhoundViewSummary;
    subhound_switch_view(app, SubhoundViewTextBox);
}

int32_t subhound_app(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "=== app start ===");
    SubhoundApp* app = subhound_app_alloc();
    HEAPLOG("post-alloc");

    while(true) {
        FURI_LOG_I(TAG, "loop: file picker");
        if(!subhound_pick_file(app)) {
            FURI_LOG_I(TAG, "loop: picker cancelled, exit");
            break;
        }
        FURI_LOG_I(TAG, "loop: picked %s", furi_string_get_cstr(app->selected_path));

        /* Briefly show the spinner so feedback is immediate. The synchronous
         * analysis call below blocks the GUI thread; ViewDispatcher only
         * paints once we yield (via view_dispatcher_run). For large captures
         * the analysis itself can be ~1s — the loading view at least shows
         * during the next event-tick gap before render. */
        s_current_view = SubhoundViewLoading;
        view_dispatcher_switch_to_view(app->view_dispatcher, SubhoundViewLoading);

        bool ok = subhound_run_analysis(app);

        if(ok) {
            subhound_build_summary(app);
            subhound_build_sections(app);
            subhound_switch_view(app, SubhoundViewSummary);
        } else {
            subhound_show_parse_error(app, "Could not read .sub file");
        }

        FURI_LOG_I(TAG, "loop: dispatcher run");
        view_dispatcher_run(app->view_dispatcher);
        FURI_LOG_I(TAG, "loop: dispatcher returned (back pressed)");
    }

    subhound_app_free(app);
    FURI_LOG_I(TAG, "=== app exit ===");
    return 0;
}
