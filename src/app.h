#ifndef APP_H
#define APP_H

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>

#include "clock_view.h"
#include "settings.h"

typedef enum {
    AppViewClock = 0,
    AppViewSettings = 1,
} AppView;

typedef enum {
    AppEventTick = 0,
} AppEvent;

typedef struct SettingsView SettingsView;

typedef struct App {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    ClockView* clock_view;
    SettingsView* settings_view;
    FuriTimer* tick_timer;
    Storage* storage;
    AppSettings settings;
} App;

int32_t app_run(void* p);

#endif /* APP_H */
