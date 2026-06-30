#pragma once
#include "easy_flipper/easy_flipper.h"

class FlipSocialAbout
{
private:
    Widget *widget;
    ViewDispatcher **viewDispatcherRef;

    static constexpr const uint32_t FlipSocialViewSubmenu = 1; // View ID for submenu
    static constexpr const uint32_t FlipSocialViewAbout = 2;   // View ID for about

    static uint32_t callbackToSubmenu(void *context);

public:
    FlipSocialAbout(ViewDispatcher **viewDispatcher);
    ~FlipSocialAbout();
};
