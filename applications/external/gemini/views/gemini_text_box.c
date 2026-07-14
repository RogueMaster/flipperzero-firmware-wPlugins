#include "gemini_text_box.h"
#include <gui/modules/text_box.h>
#include <gui/view_stack.h>

struct GeminiTextBox {
    TextBox* text_box;
    View* nav_view;
    ViewStack* view_stack;
    void* context;
    GeminiTextBoxNavCallback nav_callback;
};

static bool gemini_text_box_input_callback(InputEvent* event, void* context) {
    GeminiTextBox* gemini_text_box = context;
    UNUSED(gemini_text_box);
    UNUSED(event);

    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        if(gemini_text_box->nav_callback) {
            return gemini_text_box->nav_callback(gemini_text_box->context);
        }
    }

    return false;
}

GeminiTextBox* gemini_text_box_alloc() {
    GeminiTextBox* gemini_text_box = malloc(sizeof(GeminiTextBox));
    gemini_text_box->view_stack = view_stack_alloc();
    gemini_text_box->text_box = text_box_alloc();
    view_stack_add_view(gemini_text_box->view_stack, text_box_get_view(gemini_text_box->text_box));
    gemini_text_box->nav_view = view_alloc();
    view_set_context(gemini_text_box->nav_view, gemini_text_box);
    view_set_input_callback(gemini_text_box->nav_view, gemini_text_box_input_callback);
    view_stack_add_view(gemini_text_box->view_stack, gemini_text_box->nav_view);
    return gemini_text_box;
}

void gemini_text_box_free(GeminiTextBox* text_box) {
    furi_check(text_box);
    text_box_free(text_box->text_box);
    view_stack_free(text_box->view_stack);
    free(text_box);
}

void gemini_text_box_set_nav_callback(
    GeminiTextBox* text_box,
    GeminiTextBoxNavCallback callback,
    void* context) {
    text_box->nav_callback = callback;
    text_box->context = context;
}

void gemini_text_box_set_text(GeminiTextBox* text_box, const char* text) {
    text_box_set_text(text_box->text_box, text);
}

void gemini_text_box_set_focus_end(GeminiTextBox* text_box) {
    text_box_set_focus(text_box->text_box, TextBoxFocusEnd);
}

View* gemini_text_box_get_view(GeminiTextBox* text_box) {
    return view_stack_get_view(text_box->view_stack);
}
