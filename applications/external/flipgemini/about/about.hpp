#pragma once
#include "easy_flipper/easy_flipper.h"

class FlipGeminiAbout {
private:
    Widget* widget;
    ViewDispatcher** viewDispatcherRef;

    static constexpr const uint32_t FlipGeminiViewSubmenu = 1; // View ID for submenu
    static constexpr const uint32_t FlipGeminiViewAbout = 2; // View ID for about

    static uint32_t callbackToSubmenu(void* context);

public:
    FlipGeminiAbout(ViewDispatcher** viewDispatcher);
    ~FlipGeminiAbout();
};
