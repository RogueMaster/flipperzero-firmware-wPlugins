#pragma once
#include "easy_flipper/easy_flipper.h"

class GhoulsAbout
{
private:
    Widget *widget = nullptr;
    ViewDispatcher **viewDispatcherRef = nullptr;
    void *appContext = nullptr;

    static constexpr const uint32_t GhoulsViewSubmenu = 1; // View ID for submenu
    static constexpr const uint32_t GhoulsViewAbout = 2;   // View ID for about

    // Static callback wrappers
    static uint32_t callbackToSubmenu(void *context);

public:
    GhoulsAbout();
    ~GhoulsAbout();

    bool init(ViewDispatcher **viewDispatcher, void *appContext);
    void free();
};
