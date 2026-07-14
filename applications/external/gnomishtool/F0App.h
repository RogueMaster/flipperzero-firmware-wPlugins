#pragma once
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_power.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/view.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <notification/notification_app.h>
#include <storage/storage.h>

#include "Application.h"

struct F0App {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    FuriMutex* mutex;
    FuriTimer* timer;
    NotificationApp* Notificator;
    bool TimerEventFlag;
    float SystemScreenBrightness;
    float SystemLedBrightness;
};
typedef struct F0App F0App;

void UpdateView();
void SetLED(int r, int g, int b, float br);
void ResetLED();
void SetScreenBacklightBrightness(int brightness);
bool PinRead(GpioPin pin);
