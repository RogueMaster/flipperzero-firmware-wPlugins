#pragma once

#include <gui/view.h>

typedef struct GeminiTextBox GeminiTextBox;

typedef bool (*GeminiTextBoxNavCallback)(void* context);

GeminiTextBox* gemini_text_box_alloc();
void gemini_text_box_free(GeminiTextBox* text_box);
void gemini_text_box_set_nav_callback(
    GeminiTextBox* text_box,
    GeminiTextBoxNavCallback callback,
    void* context);
void gemini_text_box_set_text(GeminiTextBox* text_box, const char* text);
void gemini_text_box_set_focus_end(GeminiTextBox* text_box);
View* gemini_text_box_get_view(GeminiTextBox* text_box);
