/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The document itself: what it is, who issued it and how long it is good for.
 */
#include "../emrtd_i.h"

#include "../protocol/emrtd_mrz.h"

static const char* emrtd_scene_result_document_format(EmrtdMrzFormat format) {
    switch(format) {
    case EmrtdMrzFormatTd1:
        return "TD1, 3 lines of 30";
    case EmrtdMrzFormatTd2:
        return "TD2, 2 lines of 36";
    case EmrtdMrzFormatTd3:
        return "TD3, 2 lines of 44";
    default:
        return "not recognised";
    }
}

void emrtd_scene_result_document_on_enter(void* context) {
    furi_assert(context);
    Emrtd* app = context;
    Widget* widget = app->widget;
    const EmrtdMrz* mrz = &app->result.mrz;

    FuriString* body = app->text_box_store;
    furi_string_reset(body);

    furi_string_cat_printf(body, "\e#Number\n%s\n", mrz->document_number);
    furi_string_cat_printf(
        body, "\e#Type\n%s\n", mrz->document_type[0] != '\0' ? mrz->document_type : "-");
    furi_string_cat_printf(
        body, "\e#Issuing state\n%s\n", mrz->issuing_state[0] != '\0' ? mrz->issuing_state : "-");

    char expiry[16];
    emrtd_mrz_format_date(mrz->date_of_expiry, true, expiry, sizeof(expiry));
    furi_string_cat_printf(body, "\e#Expires\n%s\n", expiry);

    furi_string_cat_printf(
        body, "\e#Layout\n%s\n", emrtd_scene_result_document_format(mrz->format));

    /*
     * The check digits are arithmetic over the printed zone, not a signature;
     * they catch a misread line, nothing more. Saying which is which matters,
     * because a document whose hashes verify and whose check digits fail was
     * simply read wrong.
     */
    furi_string_cat_printf(
        body,
        "\e#Check digits\n%s\n",
        mrz->check_digits_valid ? "All correct" : "At least one is wrong");

    if(app->result.has_dg12 && app->result.dg12.any) {
        const EmrtdDg12* dg12 = &app->result.dg12;

        if(dg12->issuing_authority[0] != '\0') {
            furi_string_cat_printf(body, "\e#Issuing authority\n%s\n", dg12->issuing_authority);
        }
        if(dg12->date_of_issue[0] != '\0') {
            furi_string_cat_printf(body, "\e#Date of issue\n%s\n", dg12->date_of_issue);
        }
        if(dg12->endorsements[0] != '\0') {
            furi_string_cat_printf(body, "\e#Endorsements\n%s\n", dg12->endorsements);
        }
    }

    widget_add_string_element(widget, 64, 1, AlignCenter, AlignTop, FontPrimary, "Document");
    widget_add_text_scroll_element(widget, 0, 13, 128, 51, furi_string_get_cstr(body));

    view_dispatcher_switch_to_view(app->view_dispatcher, EmrtdViewWidget);
}

bool emrtd_scene_result_document_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void emrtd_scene_result_document_on_exit(void* context) {
    furi_assert(context);
    Emrtd* app = context;

    widget_reset(app->widget);
    furi_string_reset(app->text_box_store);
}
