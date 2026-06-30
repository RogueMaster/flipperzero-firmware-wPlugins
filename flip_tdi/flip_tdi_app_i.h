#pragma once

#include "helpers/flip_tdi_types.h"
#include "helpers/flip_tdi_event.h"

#include "scenes/flip_tdi_scene.h"
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <notification/notification_messages.h>
#include "views/flip_tdi_view_main.h"
#include <flip_tdi_icons.h>

#include "helpers/ftdi_usb.h"


typedef struct FlipTDIApp FlipTDIApp;

struct FlipTDIApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;
    Submenu* submenu;
    Widget* widget;
    FlipTDIViewMainType* flip_tdi_view_main_instance;

    FtdiUsb* ftdi_usb;
};

void flip_tdi_start(FlipTDIApp* app);
void flip_tdi_stop(FlipTDIApp* app);