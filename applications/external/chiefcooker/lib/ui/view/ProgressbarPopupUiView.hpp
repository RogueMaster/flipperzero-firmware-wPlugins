#pragma once

#include "gui/elements.h"
#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/popup.h>

#include "UiView.hpp"

#undef LOG_TAG
#define LOG_TAG "UI_VARITEMLST"

using namespace std;

class ProgressbarPopupUiView : public UiView {
private:
    View* view;
    const char* header;
    const char* progressText;
    float progressValue = 0.0f;

public:
    ProgressbarPopupUiView(const char* header) {
        this->header = header;

        view = view_alloc();
        view_set_context(view, this);
        view_set_draw_callback(view, drawCallback);
        view_allocate_model(view, ViewModelTypeLockFree, sizeof(ProgressbarPopupUiView*));
        with_view_model_cpp(view, UiVIewPointerViewModel*, model, model->uiVIew = this;, false);
    }

    void draw(Canvas* canvas) {
        canvas_clear(canvas);
        canvas_set_color(canvas, ColorBlack);

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignCenter, header);

        canvas_set_font(canvas, FontSecondary);
        elements_progress_bar_with_text(canvas, 4, 32, 120, progressValue, progressText);
    }

    void SetProgress(const char* progressText, float progressValue) {
        this->progressText = progressText;
        this->progressValue = progressValue;

        Refresh();
    }

    void Refresh() {
        view_commit_model(view, true);
    }

    View* GetNativeView() {
        return view;
    }

    ~ProgressbarPopupUiView() {
        if(view != NULL) {
            OnDestory();
            view_free_model(view);
            view_free(view);
            view = NULL;
        }
    }

private:
    static void drawCallback(Canvas* canvas, void* model) {
        ProgressbarPopupUiView* uiView = (ProgressbarPopupUiView*)((UiVIewPointerViewModel*)model)->uiVIew;
        uiView->draw(canvas);
    }
};
