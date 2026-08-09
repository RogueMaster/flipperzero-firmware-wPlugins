#ifndef APP_H
#define APP_H

#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_holder.h>
#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>
#include <input/input.h>

#include "dcf77_gpio.h"
#include "settings.h"

typedef enum {
    AppScreenClock = 0,
    AppScreenSettings = 1,
    AppScreenSync = 2,
} AppScreen;

typedef enum {
    AppSettingsRowOffset = 0,
    AppSettingsRowAuto = 1,
    AppSettingsRowInvert = 2,
    AppSettingsRowSync = 3,
    AppSettingsRowCount = 4,
} AppSettingsRow;

typedef enum {
    AppEventTypeInput,
    AppEventTypeExit,
} AppEventType;

typedef struct {
    AppEventType type;
    InputEvent input;
} AppEvent;

typedef struct {
    AppScreen screen;
    AppSettingsRow settings_row;
    char beats_text[5];
    char local_time_text[16];
    char offset_text[12];
    char sync_status[24];
    char sync_detail[24];
} AppViewModel;

typedef struct App {
    Gui* gui;
    View* view;
    ViewHolder* view_holder;
    FuriMessageQueue* event_queue;
    Storage* storage;
    AppSettings settings;
    AppScreen screen;
    AppSettingsRow settings_row;
    char beats_text[5];
    char local_time_text[16];
    char offset_text[12];
    char sync_status[24];
    char sync_detail[24];
    Dcf77BitBuffer dcf77_buf;
    bool gpio_level;
    bool gpio_have_level;
    uint32_t gpio_edge_tick;
    uint32_t gpio_last_tick;
    bool auto_sync_attempted;
    volatile bool running;
} App;

int32_t app_run(void* p);

#endif /* APP_H */
