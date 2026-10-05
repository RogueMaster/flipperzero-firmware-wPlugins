/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * What this is, what it implements, what it cannot do, and the one line that
 * matters before it is pointed at anybody's document.
 */
#include "../emrtd_i.h"

void emrtd_scene_about_on_enter(void* context) {
    furi_assert(context);
    Emrtd* app = context;
    Widget* widget = app->widget;

    FuriString* body = app->text_box_store;
    furi_string_reset(body);

    furi_string_cat_printf(body, "\e#eMRTD Reader %s\n", EMRTD_VERSION);
    furi_string_cat_str(
        body,
        "\n\e#Use it on your own\ndocument, or with the\nholder's consent.\n"
        "The details it stores open\nan identity document.\n");
    furi_string_cat_str(
        body,
        "\n\e#What it does\nReads the chip in a\npassport, an identity card\n"
        "or a residence permit, opens\nit with PACE or BAC, and\nwrites every data group it\n"
        "can reach to the SD card.\n");
    furi_string_cat_str(
        body,
        "\n\e#Standards\nICAO Doc 9303 parts 3, 10\nand 11\n"
        "BSI TR-03110 part 3\nISO/IEC 7816-4\nISO/IEC 14443-4\n");
    furi_string_cat_str(
        body,
        "\n\e#What it cannot do\nPACE over elliptic curves\nonly: the firmware's mbed TLS\n"
        "offers no modular exponen-\ntiation this application may\nlink, so the MODP groups and\n"
        "RSA are out of reach.\nThe EF.SOD signature is not\nverified on the device.\n"
        "The groups behind Extended\nAccess Control need a state\nissued certificate.\n");
    furi_string_cat_str(body, "\n\e#Licence\nMIT.\nCopyright (c) 2026\nFilip Sedivy.\n");

    widget_add_string_element(widget, 64, 1, AlignCenter, AlignTop, FontPrimary, "About");
    widget_add_text_scroll_element(widget, 0, 13, 128, 51, furi_string_get_cstr(body));

    view_dispatcher_switch_to_view(app->view_dispatcher, EmrtdViewWidget);
}

bool emrtd_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void emrtd_scene_about_on_exit(void* context) {
    furi_assert(context);
    Emrtd* app = context;

    widget_reset(app->widget);
    furi_string_reset(app->text_box_store);
}
