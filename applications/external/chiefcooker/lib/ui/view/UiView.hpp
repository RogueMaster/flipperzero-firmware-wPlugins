#pragma once

#include <functional>

#include <gui/gui.h>
#include <gui/view.h>

using namespace std;

class UiView {
private:
    function<void()> onDestroyHandler = NULL;
    function<void()> onReturnToView = NULL;
    function<bool()> goBackHandler = NULL;
    bool isOnTop = false;

public:
    virtual View* GetNativeView() = 0;
    virtual ~UiView() {
    }

    void SetOnDestroyHandler(function<void()> handler) {
        onDestroyHandler = handler;
    }

    void SetOnReturnToViewHandler(function<void()> handler) {
        onReturnToView = handler;
    }

    void SetGoBackHandler(function<bool()> handler) {
        goBackHandler = handler;
    }

    void OnReturn() {
        if(onReturnToView != NULL) {
            onReturnToView();
        }
    }

    bool IsOnTop() {
        return isOnTop;
    }

    void SetOnTop(bool value) {
        this->isOnTop = value;
    }

    bool GoBack() {
        if(goBackHandler != NULL) {
            return goBackHandler();
        }
        return true;
    }

protected:
    // Must be called from parent class destructor's!
    void OnDestory() {
        if(onDestroyHandler != NULL) {
            onDestroyHandler();
        }
    }
};

struct UiVIewPointerViewModel {
    UiView* uiVIew;
};
