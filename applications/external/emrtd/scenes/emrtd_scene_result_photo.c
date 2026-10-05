/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The facial image out of DG2.
 *
 * The Flipper has a one bit screen and no JPEG decoder, so the picture cannot
 * be shown here and the screen says so rather than leaving an empty frame.
 * What it can do is say exactly where the file went.
 */
#include "../emrtd_i.h"

#include "../protocol/emrtd_lds.h"

static const char* emrtd_scene_result_photo_format(EmrtdImageFormat format) {
    switch(format) {
    case EmrtdImageJpeg:
        return "JPEG";
    case EmrtdImageJpeg2000:
        return "JPEG 2000";
    default:
        return "not recognised";
    }
}

void emrtd_scene_result_photo_on_enter(void* context) {
    furi_assert(context);
    Emrtd* app = context;
    Widget* widget = app->widget;
    const EmrtdReadResult* result = &app->result;

    FuriString* body = app->text_box_store;
    furi_string_reset(body);

    if(!result->has_face) {
        furi_string_cat_str(body, "DG2 held no image this\nreader could recognise.\n");
    } else {
        furi_string_cat_printf(
            body, "\e#Format\n%s\n", emrtd_scene_result_photo_format(result->face.format));
        furi_string_cat_printf(
            body,
            "\e#Size\n%u.%u kB\n",
            (unsigned)(result->face_size / 1024),
            (unsigned)(((result->face_size % 1024) * 10) / 1024));

        if(result->exported && result->export_path[0] != '\0') {
            furi_string_cat_printf(
                body,
                "\e#Saved as\n%.*s/face%s\n",
                (int)sizeof(result->export_path),
                result->export_path,
                result->face.suffix != NULL ? result->face.suffix : ".bin");
        } else if(!app->config.export_to_sd) {
            furi_string_cat_str(
                body,
                "\e#Not saved\nExport to SD is off in\nOptions, so the image was\nnot kept.\n");
        } else {
            furi_string_cat_str(body, "\e#Not saved\nThe SD card refused the\nwrite.\n");
        }

        furi_string_cat_str(
            body,
            "\n\e#Why it is not shown\nThe Flipper cannot decode a\n"
            "JPEG, and its screen is one\nbit deep. Copy the file off\n"
            "the card to look at it.\n");
    }

    widget_add_string_element(widget, 64, 1, AlignCenter, AlignTop, FontPrimary, "Photo");
    widget_add_text_scroll_element(widget, 0, 13, 128, 51, furi_string_get_cstr(body));

    view_dispatcher_switch_to_view(app->view_dispatcher, EmrtdViewWidget);
}

bool emrtd_scene_result_photo_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void emrtd_scene_result_photo_on_exit(void* context) {
    furi_assert(context);
    Emrtd* app = context;

    widget_reset(app->widget);
    furi_string_reset(app->text_box_store);
}
