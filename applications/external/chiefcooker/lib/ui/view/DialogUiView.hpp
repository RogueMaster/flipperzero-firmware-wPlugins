#pragma once

#include <gui/modules/dialog_ex.h>
#include "lib/ui/UiManager.hpp"

#include "UiView.hpp"

class DialogUiView : public UiView {
private:
    DialogEx* dialog;
    function<void(DialogExResult)> resultHandler;

    static void resultCallback(DialogExResult result, void* context) {
        DialogUiView* dialog = (DialogUiView*)context;
        if(dialog->resultHandler != NULL) {
            dialog->resultHandler(result);
        }
        UiManager::GetInstance()->PopView(false);
    }

public:
    DialogUiView(const char* header, const char* text) {
        dialog = dialog_ex_alloc();
        dialog_ex_set_header(dialog, header, 128 / 2, 64 / 4, AlignCenter, AlignCenter);
        dialog_ex_set_text(dialog, text, 128 / 2, 64 / 2, AlignCenter, AlignCenter);
    }

    void AddLeftButton(const char* label) {
        dialog_ex_set_left_button_text(dialog, label);
    }

    void AddRightButton(const char* label) {
        dialog_ex_set_right_button_text(dialog, label);
    }

    void AddCenterButton(const char* label) {
        dialog_ex_set_center_button_text(dialog, label);
    }

    void SetResultHandler(function<void(DialogExResult)> handler) {
        resultHandler = handler;
        dialog_ex_set_context(dialog, this);
        dialog_ex_set_result_callback(dialog, resultCallback);
    }

    View* GetNativeView() {
        return dialog_ex_get_view(dialog);
    }

    ~DialogUiView() {
        OnDestory();
        dialog_ex_free(dialog);
    }
};
