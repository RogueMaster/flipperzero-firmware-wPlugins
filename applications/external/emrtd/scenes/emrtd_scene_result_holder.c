/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The person the document belongs to, from DG1 and, where the chip carries
 * it, from DG11.
 */
#include "../emrtd_i.h"

#include "../protocol/emrtd_mrz.h"

/** ICAO 9303-4, 4.2.2.3: M, F, or the filler when it is not stated. */
static const char* emrtd_scene_result_holder_sex(const char* sex) {
    switch(sex[0]) {
    case 'M':
        return "Male";
    case 'F':
        return "Female";
    case '<':
    case '\0':
        return "Not stated";
    default:
        return sex;
    }
}

void emrtd_scene_result_holder_on_enter(void* context) {
    furi_assert(context);
    Emrtd* app = context;
    Widget* widget = app->widget;
    const EmrtdMrz* mrz = &app->result.mrz;

    FuriString* body = app->text_box_store;
    furi_string_reset(body);

    char name[80];
    emrtd_mrz_full_name(mrz, name, sizeof(name));
    furi_string_cat_printf(body, "\e#Name\n%s\n", name);

    char birth[16];
    emrtd_mrz_format_date(mrz->date_of_birth, false, birth, sizeof(birth));
    furi_string_cat_printf(body, "\e#Date of birth\n%s\n", birth);

    furi_string_cat_printf(body, "\e#Sex\n%s\n", emrtd_scene_result_holder_sex(mrz->sex));
    furi_string_cat_printf(
        body, "\e#Nationality\n%s\n", mrz->nationality[0] != '\0' ? mrz->nationality : "-");

    /* DG11 is optional and what it holds differs from state to state, so
     * every field is printed only when the chip carried it. */
    if(app->result.has_dg11 && app->result.dg11.any) {
        const EmrtdDg11* dg11 = &app->result.dg11;

        if(dg11->full_name[0] != '\0') {
            furi_string_cat_printf(body, "\e#Full name\n%s\n", dg11->full_name);
        }
        if(dg11->personal_number[0] != '\0') {
            furi_string_cat_printf(body, "\e#Personal number\n%s\n", dg11->personal_number);
        }
        if(dg11->place_of_birth[0] != '\0') {
            furi_string_cat_printf(body, "\e#Place of birth\n%s\n", dg11->place_of_birth);
        }
        if(dg11->address[0] != '\0') {
            furi_string_cat_printf(body, "\e#Address\n%s\n", dg11->address);
        }
        if(dg11->telephone[0] != '\0') {
            furi_string_cat_printf(body, "\e#Telephone\n%s\n", dg11->telephone);
        }
        if(dg11->profession[0] != '\0') {
            furi_string_cat_printf(body, "\e#Profession\n%s\n", dg11->profession);
        }
    }

    widget_add_string_element(widget, 64, 1, AlignCenter, AlignTop, FontPrimary, "Holder");
    widget_add_text_scroll_element(widget, 0, 13, 128, 51, furi_string_get_cstr(body));

    view_dispatcher_switch_to_view(app->view_dispatcher, EmrtdViewWidget);
}

bool emrtd_scene_result_holder_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void emrtd_scene_result_holder_on_exit(void* context) {
    furi_assert(context);
    Emrtd* app = context;

    widget_reset(app->widget);
    furi_string_reset(app->text_box_store);
}
