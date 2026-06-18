#pragma once

#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>

#include <notification/notification_messages.h>

#include "dallas_test_worker.h"
#include "dallas_test_view.h"
#include "scenes/dallas_tester_scene.h"

typedef enum {
    DallasTesterViewSubmenu,
    DallasTesterViewTest,
    DallasTesterViewWidget,
} DallasTesterView;

typedef struct {
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Gui* gui;
    NotificationApp* notifications;

    Submenu* submenu;
    Widget* widget;
    DallasTestView* test_view;

    DallasTestWorker* worker;
    bool last_present; // tracks key presence so the LED blinks once on detection
} DallasTester;
