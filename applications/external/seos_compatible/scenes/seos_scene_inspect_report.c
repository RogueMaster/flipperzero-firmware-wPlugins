#include "../seos_i.h"
#include "seos_scene.h"

/* First page of a finished inspection: the reader's versions and memory.
 *
 * Laid out on the stock widget at the coordinates the hand-drawn page used, so
 * the two render the same. */
void seos_scene_inspect_report_on_enter(void* context) {
    Seos* seos = context;
    Widget* widget = seos->widget;
    const SeosInspectData* d = &seos->seos_emulator->inspect;

    widget_add_string_element(
        widget, 64, 0, AlignCenter, AlignTop, FontPrimary, "Inspection Report");
    widget_add_line_element(widget, 0, 13, 128, 13);

    /* A value the reader never gave is left out rather than shown as zero. */
    uint8_t y = 24;
    char buf[40];

    if(d->sam_ver) {
        snprintf(buf, sizeof(buf), "SAM:  0x%04X", d->sam_ver);
        widget_add_string_element(widget, 4, y, AlignLeft, AlignBottom, FontSecondary, buf);
        y += 11;
    }
    if(d->core_ver) {
        snprintf(buf, sizeof(buf), "Core: 0x%04X", d->core_ver);
        widget_add_string_element(widget, 4, y, AlignLeft, AlignBottom, FontSecondary, buf);
        y += 11;
    }
    if(d->assembly_count || d->persist_mem) {
        if(d->persist_mem) {
            snprintf(
                buf,
                sizeof(buf),
                "Apps: %u   Mem: %luB",
                d->assembly_count,
                (unsigned long)d->persist_mem);
        } else {
            snprintf(buf, sizeof(buf), "Apps: %u", d->assembly_count);
        }
        widget_add_string_element(widget, 4, y, AlignLeft, AlignBottom, FontSecondary, buf);
    } else if(!d->cred_parsed) {
        widget_add_string_element(
            widget, 4, y, AlignLeft, AlignBottom, FontSecondary, "Cred table: timeout");
    }

    widget_add_button_element(
        widget, GuiButtonTypeRight, "Next Page", seos_scene_widget_callback, seos);

    view_dispatcher_switch_to_view(seos->view_dispatcher, SeosViewWidget);
}

bool seos_scene_inspect_report_on_event(void* context, SceneManagerEvent event) {
    Seos* seos = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeRight) {
            scene_manager_next_scene(seos->scene_manager, SeosSceneInspectCredentials);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        /* The scene before this one is the inspection itself; going back to it
         * would start another, so leave for the menu. */
        consumed = scene_manager_search_and_switch_to_previous_scene(
            seos->scene_manager, SeosSceneMainMenu);
    }
    return consumed;
}

void seos_scene_inspect_report_on_exit(void* context) {
    Seos* seos = context;

    widget_reset(seos->widget);
}
