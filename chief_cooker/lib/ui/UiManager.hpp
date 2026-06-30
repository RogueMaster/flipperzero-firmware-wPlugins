#pragma once

#include <forward_list>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/loading.h>

#include "view/UiView.hpp"

#undef LOG_TAG
#define LOG_TAG "UI_MGR"

using namespace std;

static void* __ui_manager_instance = NULL;

class UiManager {
private:
    Gui* gui = NULL;
    ViewDispatcher* viewDispatcher = NULL;
    forward_list<UiView*> viewStack;
    uint8_t viewStackSize = 0;

    uint32_t loadingId = 9999;
    Loading* loading = NULL;

    UiManager() {
    }

    static uint32_t backCallback(void*) {
        UiManager* uiManager = GetInstance();
        UiView* currentView = uiManager->viewStack.front();
        if(currentView->GoBack()) {
            uiManager->PopView(false);
        }
        return uiManager->currentViewId();
    }

    uint32_t currentViewId() {
        if(viewStack.empty()) {
            return VIEW_NONE;
        }
        return viewStackSize;
    }

    void freeLoading() {
        if(loading != NULL) {
            view_dispatcher_remove_view(viewDispatcher, loadingId);
            loading_free(loading);
            loading = NULL;
        }
    }

    void showView(uint32_t viewId) {
        freeLoading();
        view_dispatcher_switch_to_view(viewDispatcher, viewId);
    }

public:
    void ShowLoading() {
        if(loading == NULL) {
            loading = loading_alloc();
            View* loadingView = loading_get_view(loading);
            view_dispatcher_add_view(viewDispatcher, loadingId, loadingView);
            view_dispatcher_switch_to_view(viewDispatcher, loadingId);
        }
    }

    static UiManager* GetInstance() {
        if(__ui_manager_instance == NULL) {
            __ui_manager_instance = new UiManager();
        }
        return (UiManager*)__ui_manager_instance;
    }

    void InitGui() {
        gui = (Gui*)furi_record_open(RECORD_GUI);
        viewDispatcher = view_dispatcher_alloc();
        view_dispatcher_attach_to_gui(viewDispatcher, gui, ViewDispatcherTypeFullscreen);
    }

    void PushView(UiView* view) {
        if(!viewStack.empty()) {
            viewStack.front()->SetOnTop(false);
        }

        viewStackSize++;
        viewStack.push_front(view);
        view->SetOnTop(true);

        view_set_previous_callback(view->GetNativeView(), backCallback);
        view_dispatcher_add_view(viewDispatcher, currentViewId(), view->GetNativeView());
        showView(currentViewId());
    }

    void PopView(bool preserveView) {
        UiView* currentView = viewStack.front();
        currentView->SetOnTop(false);
        view_dispatcher_remove_view(viewDispatcher, currentViewId());
        viewStack.pop_front();
        viewStackSize--;

        if(!viewStack.empty()) {
            UiView* viewReturningTo = viewStack.front();
            viewReturningTo->SetOnTop(true);
            viewReturningTo->OnReturn();
        }

        showView(currentViewId());

        if(!preserveView) {
            delete currentView;
        }
    }

    void RunEventLoop() {
        while(!viewStack.empty()) {
            view_dispatcher_run(viewDispatcher);
        }
    }

    ~UiManager() {
        while(!viewStack.empty()) {
            PopView(false);
        }

        freeLoading();

        if(viewDispatcher != NULL) {
            view_dispatcher_free(viewDispatcher);
            viewDispatcher = NULL;
        }

        if(gui != NULL) {
            furi_record_close(RECORD_GUI);
            gui = NULL;
        }
    }
};
