/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * What the read produced, in four lines.
 *
 * The detail lives one press away; this screen answers the two questions
 * asked while the document is still being held against the case: did it open,
 * and is it on the card.
 */
#include "../emrtd_i.h"

#include "../protocol/emrtd_mrz.h"

static void
    emrtd_scene_read_success_button_callback(GuiButtonType result, InputType type, void* context) {
    furi_assert(context);
    Emrtd* app = context;

    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}

void emrtd_scene_read_success_on_enter(void* context) {
    furi_assert(context);
    Emrtd* app = context;
    Widget* widget = app->widget;
    const EmrtdReadResult* result = &app->result;

    emrtd_notify_success(app);

    uint8_t announced = 0;
    uint8_t read = 0;
    for(size_t i = 0; i < EmrtdFileCount; i++) {
        if(result->files[i].state == EmrtdFileStateAbsent) {
            continue;
        }
        announced++;
        if(result->files[i].state == EmrtdFileStateRead) {
            read++;
        }
    }

    FuriString* body = app->text_box_store;
    furi_string_reset(body);

    if(result->has_mrz) {
        char name[80];
        emrtd_mrz_full_name(&result->mrz, name, sizeof(name));
        furi_string_cat_printf(body, "%s\n", name);
    }

    furi_string_cat_printf(body, "Opened with %s\n", result->access.protocol);
    furi_string_cat_printf(body, "%u of %u files read\n", read, announced);

    if(result->hashes_checked > 0) {
        furi_string_cat_printf(
            body, "%u of %u hashes matched\n", result->hashes_matched, result->hashes_checked);
    }

    if(result->exported && result->export_path[0] != '\0') {
        furi_string_cat_printf(body, "Saved to %s", result->export_path);
    } else {
        furi_string_cat_str(body, "Not saved to the SD card");
    }

    widget_add_string_element(widget, 64, 1, AlignCenter, AlignTop, FontPrimary, "Document read");
    /* 34, not 36, so that no fourth line is drawn under the button; see the
     * error scene for the arithmetic. */
    widget_add_text_scroll_element(widget, 0, 13, 128, 34, furi_string_get_cstr(body));
    widget_add_button_element(
        widget, GuiButtonTypeCenter, "Details", emrtd_scene_read_success_button_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, EmrtdViewWidget);
}

bool emrtd_scene_read_success_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    Emrtd* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeCenter) {
            scene_manager_next_scene(app->scene_manager, EmrtdSceneResult);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        /* Never back into the read scene: that would start another read. */
        consumed =
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, EmrtdSceneStart);
    }

    return consumed;
}

void emrtd_scene_read_success_on_exit(void* context) {
    furi_assert(context);
    Emrtd* app = context;

    widget_reset(app->widget);
    furi_string_reset(app->text_box_store);
}
