#include "../warden_i.h"
#include <nfc/nfc_device.h>

void warden_scene_details_on_enter(void* context) {
    WardenApp* app = context;
    Widget* widget = app->widget;
    widget_reset(widget);

    const CardGrade* g = &app->grade;
    const CardReading* r = &app->reading;

    FuriString* s = furi_string_alloc();

    /* header line (bold) */
    furi_string_cat_printf(s, "\e#%s\n", g->card_name);
    furi_string_cat_printf(
        s, "Grade %s   %d/100   %s\n\n", g->letter, g->score, grader_band_label(g->band));

    /* findings */
    for(size_t i = 0; i < g->finding_num; i++) {
        furi_string_cat_printf(
            s, "%s %s\n", grader_severity_glyph(g->findings[i].sev), g->findings[i].text);
    }

    /* hard facts */
    furi_string_cat_str(s, "\n\e#Card facts\n");
    const char* tech = nfc_device_get_protocol_name(r->top);
    furi_string_cat_printf(s, "Tech: %s\n", tech ? tech : "Unknown");

    if(r->uid_len > 0) {
        furi_string_cat_str(s, "UID:");
        for(size_t i = 0; i < r->uid_len; i++) {
            furi_string_cat_printf(s, " %02X", r->uid[i]);
        }
        furi_string_cat_printf(s, "  (%d B)\n", (int)r->uid_len);
    } else {
        furi_string_cat_str(s, "UID: not read at this layer\n");
    }

    if(r->has_iso3a) {
        furi_string_cat_printf(
            s, "SAK: %02X   ATQA: %02X %02X\n", r->sak, r->atqa[0], r->atqa[1]);
    }
    if(r->protocol_num > 1) {
        furi_string_cat_str(s, "Stack:");
        for(size_t i = 0; i < r->protocol_num; i++) {
            const char* n = nfc_device_get_protocol_name(r->stack[i]);
            furi_string_cat_printf(s, " %s%s", n ? n : "?", (i + 1 < r->protocol_num) ? " >" : "");
        }
        furi_string_cat_str(s, "\n");
    }

    /* the plain-English verdict */
    furi_string_cat_printf(s, "\n\e#Verdict\n%s\n", g->verdict);

    widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(s));
    furi_string_free(s);

    view_dispatcher_switch_to_view(app->view_dispatcher, WardenViewWidget);
}

bool warden_scene_details_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void warden_scene_details_on_exit(void* context) {
    WardenApp* app = context;
    widget_reset(app->widget);
}
