/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * What went wrong, and what to do about it.
 *
 * A chip that refuses to open says nothing about which of the three values
 * was mistyped, so when the key is the suspect this screen repeats all three
 * exactly as they were used and offers the way back to them. Guessing at the
 * cause would be worse than useless here.
 */
#include "../emrtd_i.h"

#include "../protocol/emrtd_mrz.h"

static void
    emrtd_scene_read_error_button_callback(GuiButtonType result, InputType type, void* context) {
    furi_assert(context);
    Emrtd* app = context;

    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}

/**
 * True when the credentials are the first thing to suspect: the two verdicts
 * on the key the worker itself treats as final, and credentials that are not
 * there at all. A chip that announces no PACE is not one of them - its hint
 * points at Options, and the values would only be a distraction.
 */
static bool emrtd_scene_read_error_blames_key(EmrtdError error) {
    return error == EmrtdErrorWrongKey || error == EmrtdErrorPaceFailed ||
           error == EmrtdErrorInvalidInput;
}

void emrtd_scene_read_error_on_enter(void* context) {
    furi_assert(context);
    Emrtd* app = context;
    Widget* widget = app->widget;
    const EmrtdError error = app->result.error;
    const EmrtdCredentials* credentials = &app->config.credentials;

    emrtd_notify_error(app);

    FuriString* body = app->text_box_store;
    furi_string_reset(body);
    furi_string_cat_str(body, emrtd_error_hint(error));

    if(emrtd_scene_read_error_blames_key(error)) {
        char birth[16];
        char expiry[16];
        emrtd_mrz_format_date(credentials->date_of_birth, false, birth, sizeof(birth));
        emrtd_mrz_format_date(credentials->date_of_expiry, true, expiry, sizeof(expiry));

        furi_string_cat_printf(
            body,
            "\n\n\e#Values used\nNumber: %s\nBorn: %s\nExpires: %s",
            credentials->document_number[0] != '\0' ? credentials->document_number : "not set",
            credentials->date_of_birth[0] != '\0' ? birth : "not set",
            credentials->date_of_expiry[0] != '\0' ? expiry : "not set");

        if(credentials->has_can && credentials->can[0] != '\0') {
            furi_string_cat_printf(body, "\nCAN: %s", credentials->can);
            /* Which of the two passwords was wrong is not knowable here: a key
             * failure means no driver got in. So the screen says what each
             * protocol would have used and leaves the suspicion to the user. */
            furi_string_cat_str(
                body, "\n\nPACE uses the CAN alone;\nBAC has no key but the\nthree values above.");
        } else {
            /* The card access number, where a document prints one, is the
             * easier way in: one value to type, and no check digit. */
            furi_string_cat_str(
                body,
                "\n\nAll three come from the\nMRZ, the lines with <<<.\n"
                "If the document prints a\nCAN, it replaces all three.");
        }
    }

    /* The two numbers behind the refusal. Without them a report says only
     * that memory ran out, which is the one thing already known. */
    if(error == EmrtdErrorOutOfMemory) {
        furi_string_cat_printf(
            body,
            "\n\n\e#What it found\nFree: %zu B\nLargest piece: %zu B\nComputer: %s",
            app->heap_free,
            app->heap_largest_block,
            app->heap_host_connected ? "connected" : "not connected");
    }

    /* Which file it died on is the difference between a broken document and a
     * group this reader cannot open. */
    bool placed = false;
    if(error != EmrtdErrorNone && app->result.error_file < EmrtdFileCount) {
        const EmrtdFileInfo* info = emrtd_file_info(app->result.error_file);
        if(info != NULL &&
           app->result.files[app->result.error_file].state == EmrtdFileStateFailed) {
            furi_string_cat_printf(body, "\n\nIt stopped on %s.", info->name);
            placed = true;
        }
    }
    /*
     * A read that stopped before any file has no file to name, and those are
     * the stops worth telling apart: a chip that goes quiet while it computes
     * PACE is a different problem from one that never opened a session. The
     * read screen showed the stage, but it is gone by the time this is drawn.
     */
    const char* stopped = emrtd_worker_stopped_text(&app->result);
    if(!placed && stopped != NULL) {
        furi_string_cat_printf(body, "\n\n%s", stopped);
    }

    widget_add_string_element(
        widget, 64, 1, AlignCenter, AlignTop, FontPrimary, emrtd_error_text(error));
    /*
     * 34 rather than 36: the scroll element draws a line while its descender
     * still fits, so at 36 a fourth line starts at y=46 and runs into the
     * buttons, which begin at y=52. At 34 exactly three lines show, and the
     * number of scroll steps is the same.
     */
    widget_add_text_scroll_element(widget, 0, 13, 128, 34, furi_string_get_cstr(body));
    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Retry", emrtd_scene_read_error_button_callback, app);

    if(emrtd_scene_read_error_blames_key(error)) {
        widget_add_button_element(
            widget, GuiButtonTypeRight, "Document", emrtd_scene_read_error_button_callback, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, EmrtdViewWidget);
}

bool emrtd_scene_read_error_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    Emrtd* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            /* Back into the read scene, which starts a fresh attempt. */
            consumed = scene_manager_previous_scene(app->scene_manager);
        } else if(event.event == GuiButtonTypeRight) {
            /*
             * The details screen may not be on the stack at all when the read
             * was started straight from the first menu, so the stack is wound
             * back to the menu and the screen opened from there.
             */
            if(scene_manager_has_previous_scene(app->scene_manager, EmrtdSceneDocument)) {
                scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, EmrtdSceneDocument);
            } else {
                scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, EmrtdSceneStart);
                scene_manager_next_scene(app->scene_manager, EmrtdSceneDocument);
            }
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed =
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, EmrtdSceneStart);
    }

    return consumed;
}

void emrtd_scene_read_error_on_exit(void* context) {
    furi_assert(context);
    Emrtd* app = context;

    widget_reset(app->widget);
    furi_string_reset(app->text_box_store);
}
