#pragma once

#include "lib/String.hpp"
#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/text_input.h>

#include "UiView.hpp"

class TextInputUiView : public UiView {
private:
    TextInput* textInput;
    char* textBuffer;
    size_t bufferSize;
    function<void(char*)> inputHandler;

    static void inputCallback(void* context) {
        TextInputUiView* instance = (TextInputUiView*)context;
        if(instance->inputHandler != NULL) {
            instance->inputHandler(instance->textBuffer);
        }
    }

public:
    TextInputUiView(const char* header, size_t minLength, size_t maxLength) {
        textInput = text_input_alloc();
        text_input_set_header_text(textInput, header);
        text_input_set_minimum_length(textInput, minLength);
        textBuffer = new char[maxLength];
        bufferSize = maxLength;
    }

    void SetDefaultText(String* text) {
        strcpy(textBuffer, text->cstr());
    }

    void SetResultHandler(function<void(char*)> handler) {
        inputHandler = handler;
        text_input_set_result_callback(textInput, inputCallback, this, textBuffer, bufferSize, false);
    }

    View* GetNativeView() {
        return text_input_get_view(textInput);
    }

    ~TextInputUiView() {
        OnDestory();

        text_input_free(textInput);
        delete[] textBuffer;
    }
};
