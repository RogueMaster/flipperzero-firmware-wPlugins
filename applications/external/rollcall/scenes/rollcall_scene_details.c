#include "../rollcall_i.h"

void rollcall_scene_details_on_enter(void* context) {
    RollCallApp* app = context;
    Widget* widget = app->widget;
    widget_reset(widget);

    const RcVerdict* v = &app->verdict;

    FuriString* s = furi_string_alloc();

    /* header + one-line verdict */
    furi_string_cat_printf(s, "\e#%s\n", v->protocol);
    furi_string_cat_printf(
        s, "Grade %s   %s   %s\n\n", v->letter, rc_class_label(v->cls), rc_health_label(v->health));

    /* the plain-English explanation */
    furi_string_cat_printf(s, "%s\n\n", v->detail);

    /* per-press ledger with fingerprints so the roll (or lack of it) is visible */
    furi_string_cat_str(s, "\e#Presses seen\n");
    if(app->capture_count == 0) {
        furi_string_cat_str(s, "None decoded.\n");
    } else {
        for(uint8_t i = 0; i < app->capture_count; i++) {
            RcCapture* c = &app->captures[i];
            /* mark whether this parcel is new or a repeat of an earlier one */
            const char* mark = "new";
            for(uint8_t j = 0; j < i; j++) {
                if(app->captures[j].fingerprint == c->fingerprint) {
                    mark = "same";
                    break;
                }
            }
            uint32_t hi = (uint32_t)(c->fingerprint >> 32);
            uint32_t lo = (uint32_t)(c->fingerprint & 0xFFFFFFFF);
            furi_string_cat_printf(s, "%d. %s", i + 1, c->protocol);
            if(c->bits > 0) furi_string_cat_printf(s, " %ubit", c->bits);
            furi_string_cat_printf(s, "  %ddBm\n", (int)c->rssi);
            furi_string_cat_printf(
                s, "   %08lX%08lX (%s)\n", (unsigned long)hi, (unsigned long)lo, mark);
        }
    }

    /* what the fingerprint is */
    furi_string_cat_str(
        s,
        "\n\e#The fingerprint\n"
        "A hash of each decoded parcel\n"
        "(key + counter). Rolling codes\n"
        "change it every press; fixed\n"
        "codes repeat it forever.\n\n");

    /* ethics */
    furi_string_cat_str(
        s,
        "\e#Use it right\n"
        "Only test remotes you own or are\n"
        "authorised to check. RollCall only\n"
        "listens - it never transmits,\n"
        "replays or clones.\n");

    widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(s));
    furi_string_free(s);

    view_dispatcher_switch_to_view(app->view_dispatcher, RollCallViewWidget);
}

bool rollcall_scene_details_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void rollcall_scene_details_on_exit(void* context) {
    RollCallApp* app = context;
    widget_reset(app->widget);
}
