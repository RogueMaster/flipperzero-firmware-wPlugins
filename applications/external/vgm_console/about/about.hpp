#pragma once
#include "furi.h"
#include <gui/modules/widget.h>
#include <gui/view_dispatcher.h>

class VideoGameModuleConsoleAbout
{
private:
    Widget *widget;
    ViewDispatcher *viewDispatcherRef;

    static constexpr const uint32_t VideoGameModuleConsoleViewSubmenu = 1; // View ID for submenu
    static constexpr const uint32_t VideoGameModuleConsoleViewAbout = 2;   // View ID for about

    static uint32_t callbackToSubmenu(void *context);

public:
    VideoGameModuleConsoleAbout(ViewDispatcher *viewDispatcher);
    ~VideoGameModuleConsoleAbout();
};
