#pragma once
#include "easy_flipper/easy_flipper.h"

class FlipTelegramAbout {
private:
    Widget* widget;
    ViewDispatcher** viewDispatcherRef;

    static constexpr const uint32_t FlipTelegramViewSubmenu = 1; // View ID for submenu
    static constexpr const uint32_t FlipTelegramViewAbout = 2; // View ID for about

    static uint32_t callbackToSubmenu(void* context);

public:
    FlipTelegramAbout(ViewDispatcher** viewDispatcher);
    ~FlipTelegramAbout();
};
