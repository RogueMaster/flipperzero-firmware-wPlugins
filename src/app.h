#ifndef APP_H
#define APP_H

#include <gui/gui.h>
#include <gui/view_port.h>
#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>
#include <input/input.h>

#include "settings.h"

typedef enum {
    AppScreenClock = 0,
    AppScreenSettings = 1,
} AppScreen;

typedef enum {
    AppEventTypeInput,
    AppEventTypeTick,
} AppEventType;

typedef struct {
    AppEventType type;
    InputEvent input;
} AppEvent;

typedef struct App {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    FuriTimer* tick_timer;
    Storage* storage;
    AppSettings settings;
    AppScreen screen;
    char beats_text[5];
    char local_time_text[16];
    char offset_text[12];
    bool running;
} App;

int32_t app_run(void* p);

#endif /* APP_H */
