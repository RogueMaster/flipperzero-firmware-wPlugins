#include "widget.h"
#include "widget_elements/widget_element_i.h"
#include <furi.h>
#include <m-array.h>

ARRAY_DEF(ElementArray, WidgetElement*, M_PTR_OPLIST);

struct Widget {
    View* view;
    void* context;
};

typedef struct {
    ElementArray_t element;
} GuiWidgetModel;

static void widget_add_element(Widget* widget, WidgetElement* element) {
    furi_assert(widget);
    furi_assert(element);

    with_view_model(
        widget->view,
        GuiWidgetModel * model,
        {
            element->parent = widget;
            ElementArray_push_back(model->element, element);
        },
        true);
}

WidgetElement* widget_add_string_multiline_element_utf8(
    Widget* widget,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical,
    const char* text) {
    furi_assert(widget);
    WidgetElement* string_multiline_element =
        widget_element_string_multiline_create(x, y, horizontal, vertical, FontPrimary, text);
    widget_add_element(widget, string_multiline_element);
    return string_multiline_element;
}

WidgetElement* widget_add_string_element_utf8(
    Widget* widget,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical,
    const char* text) {
    furi_assert(widget);
    WidgetElement* string_element = widget_element_string_create(x, y, horizontal, vertical, text);
    widget_add_element(widget, string_element);
    return string_element;
}

WidgetElement* widget_add_text_box_element_utf8(
    Widget* widget,
    uint8_t x,
    uint8_t y,
    uint8_t width,
    uint8_t height,
    Align horizontal,
    Align vertical,
    const char* text,
    bool strip_to_dots) {
    furi_assert(widget);
    WidgetElement* text_box_element = widget_element_text_box_create(
        x, y, width, height, horizontal, vertical, text, strip_to_dots);
    widget_add_element(widget, text_box_element);
    return text_box_element;
}

WidgetElement* widget_add_text_scroll_element_utf8(
    Widget* widget,
    uint8_t x,
    uint8_t y,
    uint8_t width,
    uint8_t height,
    const char* text) {
    furi_assert(widget);
    WidgetElement* text_scroll_element =
        widget_element_text_scroll_create(x, y, width, height, text);
    widget_add_element(widget, text_scroll_element);
    return text_scroll_element;
}

WidgetElement* widget_add_button_element_utf8(
    Widget* widget,
    GuiButtonType button_type,
    const char* text,
    ButtonCallback callback,
    void* context) {
    furi_assert(widget);
    WidgetElement* button_element =
        widget_element_button_create(button_type, text, callback, context);
    widget_add_element(widget, button_element);
    return button_element;
}
