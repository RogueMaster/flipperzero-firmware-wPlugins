/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * Where to buy the author a coffee: the address, and a code a phone can take
 * it from without anybody typing it.
 */
#include "../emrtd_i.h"

/*
 * The code is the README's Support link, drawn by assets/make_donate_qr.py:
 * version 3, 29 modules at two pixels each, 58 of the screen's 64 rows. Three
 * pixels in from the top, the bottom and the right edge is all the quiet zone
 * that leaves - a module and a half against the four a printed code gets - so
 * the text keeps its distance on the fourth side instead.
 */
#define EMRTD_SCENE_DONATE_QR_X 67
#define EMRTD_SCENE_DONATE_QR_Y 3

/*
 * buymeacoffee.com/filipsedivy draws 126 px wide in FontSecondary, the whole
 * width of the screen, so beside the code it is broken into three lines. The
 * widest, "buymeacoffee", draws 58 px from x = 1, which leaves eight clear
 * columns before the code; the heading's "Buy me a" draws 46. No line comes
 * near the 127 px elements_multiline_text_aligned allows before it hyphenates.
 *
 * The heading's first row lines up with the code's top edge and the last
 * baseline with its bottom one, so the text block is as tall as the code.
 */
#define EMRTD_SCENE_DONATE_TEXT_X      1
#define EMRTD_SCENE_DONATE_ADDRESS_END 60

void emrtd_scene_donate_on_enter(void* context) {
    furi_assert(context);
    Emrtd* app = context;
    Widget* widget = app->widget;

    widget_add_string_multiline_element(
        widget,
        EMRTD_SCENE_DONATE_TEXT_X,
        EMRTD_SCENE_DONATE_QR_Y,
        AlignLeft,
        AlignTop,
        FontPrimary,
        "Buy me a\ncoffee");
    widget_add_string_multiline_element(
        widget,
        EMRTD_SCENE_DONATE_TEXT_X,
        EMRTD_SCENE_DONATE_ADDRESS_END,
        AlignLeft,
        AlignBottom,
        FontSecondary,
        "buymeacoffee\n.com/\nfilipsedivy");
    widget_add_icon_element(
        widget, EMRTD_SCENE_DONATE_QR_X, EMRTD_SCENE_DONATE_QR_Y, &I_EmrtdDonateQr_58x58);

    /* A phone camera takes a while to open and to focus. If the backlight
     * times out in the meantime the camera is left an unlit LCD, which in a
     * dim room it cannot find the code on. */
    notification_message(app->notifications, &sequence_display_backlight_enforce_on);

    view_dispatcher_switch_to_view(app->view_dispatcher, EmrtdViewWidget);
}

bool emrtd_scene_donate_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void emrtd_scene_donate_on_exit(void* context) {
    furi_assert(context);
    Emrtd* app = context;

    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
    widget_reset(app->widget);
}
