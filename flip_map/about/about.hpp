#pragma once
#include "easy_flipper/easy_flipper.h"

class FlipMapAbout
{
private:
    Widget *widget;
    ViewDispatcher **viewDispatcherRef;

    static constexpr const uint32_t FlipMapViewSubmenu = 1; // View ID for submenu
    static constexpr const uint32_t FlipMapViewAbout = 2;   // View ID for about

    static uint32_t callbackToSubmenu(void *context);

public:
    FlipMapAbout(ViewDispatcher **viewDispatcher);
    ~FlipMapAbout();
};
