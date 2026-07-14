#pragma once
#include <gui/modules/widget.h>

#ifdef __cplusplus
extern "C" {
#endif

WidgetElement* widget_add_string_multiline_element_utf8(
    Widget* widget,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical,
    const char* text);

WidgetElement* widget_add_string_element_utf8(
    Widget* widget,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical,
    const char* text);

WidgetElement* widget_add_text_box_element_utf8(
    Widget* widget,
    uint8_t x,
    uint8_t y,
    uint8_t width,
    uint8_t height,
    Align horizontal,
    Align vertical,
    const char* text,
    bool strip_to_dots);

WidgetElement* widget_add_text_scroll_element_utf8(
    Widget* widget,
    uint8_t x,
    uint8_t y,
    uint8_t width,
    uint8_t height,
    const char* text);

WidgetElement* widget_add_button_element_utf8(
    Widget* widget,
    GuiButtonType button_type,
    const char* text,
    ButtonCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
