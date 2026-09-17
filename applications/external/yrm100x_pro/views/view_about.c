#include "view_about.h"

uint32_t uhf_reader_navigation_about_callback(void* context) {
    UNUSED(context);
    return UHFReaderViewSubmenu;
}

void view_about_alloc(UHFReaderApp* App) {
    App->WidgetAbout = widget_alloc();

    widget_add_string_element(
        App->WidgetAbout, 64, 3, AlignCenter, AlignTop, FontPrimary, "YRM100X_PRO");

    widget_add_string_multiline_element(
        App->WidgetAbout,
        4,
        17,
        AlignLeft,
        AlignTop,
        FontSecondary,
        "Version 1.7\n"
        "YRM100X UHF RFID toolkit\n"
        "Maintainer:\n"
        "@AlexeySmirnov74");

    view_set_previous_callback(
        widget_get_view(App->WidgetAbout), uhf_reader_navigation_about_callback);

    view_dispatcher_add_view(
        App->ViewDispatcher, UHFReaderViewAbout, widget_get_view(App->WidgetAbout));
}

void view_about_free(UHFReaderApp* App) {
    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewAbout);
    widget_free(App->WidgetAbout);
    App->WidgetAbout = NULL;
}
