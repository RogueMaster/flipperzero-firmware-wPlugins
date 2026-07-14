#pragma once
#include "easy_flipper/easy_flipper.h"

class FlipWorldAbout {
private:
    Widget* widget = nullptr;
    ViewDispatcher** viewDispatcherRef = nullptr;
    void* appContext = nullptr;

    static constexpr const uint32_t FlipWorldViewSubmenu = 1; // View ID for submenu
    static constexpr const uint32_t FlipWorldViewAbout = 2; // View ID for about

    static uint32_t callbackToSubmenu(void* context);

public:
    FlipWorldAbout();
    ~FlipWorldAbout();

    void free();
    bool init(ViewDispatcher** viewDispatcher, void* appContext);
};
