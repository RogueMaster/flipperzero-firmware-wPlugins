#pragma once
#include "easy_flipper/easy_flipper.h"

class FreeRoamAbout
{
private:
    Widget *widget = nullptr;
    ViewDispatcher **viewDispatcherRef = nullptr;
    void *appContext = nullptr;

    static constexpr const uint32_t FreeRoamViewSubmenu = 1; // View ID for submenu
    static constexpr const uint32_t FreeRoamViewAbout = 2;   // View ID for about

    // Static callback wrappers
    static uint32_t callbackToSubmenu(void *context);

public:
    FreeRoamAbout();
    ~FreeRoamAbout();

    bool init(ViewDispatcher **viewDispatcher, void *appContext);
    void free();
};
