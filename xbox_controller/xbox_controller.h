#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include <gui/modules/submenu.h>
#include <gui/modules/dialog_ex.h>

#include "views/xbox_controller_view.h"
#include "views/media_controller_view.h"
#include "xc_icons.h"

#include <dolphin/dolphin.h>

// this should be used as global state
// we can store different things here
typedef struct {
    Gui* gui;
    NotificationApp* notifications;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    DialogEx* dialog;
    XboxControllerView* xbox_controller_view;
    MediaControllerView* media_controller_view;
    uint32_t view_id;
} XboxController;

typedef enum {
    UsbHidViewSubmenu,
    UsbHidViewXboxController,
    UsbHidViewMediaController,
    UsbHidViewExitConfirm,
} UsbHidView;

extern const NotificationSequence sequence_blink_purple_50;

void send_xbox_ir(uint32_t command, NotificationApp* notifications, bool repeat);
