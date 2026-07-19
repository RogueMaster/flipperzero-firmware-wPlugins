#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "glitch_trigger_icons.h" // generated from icons/ by fbt

#include "helpers/glitch_config.h"
#include "helpers/glitch_engine.h"
#include "views/trigger_view.h"
#include "views/sweep_view.h"
#include "views/wiring_view.h"
#include "scenes/glitch_scene.h"

#define GLITCH_VERSION "1.0"

typedef enum {
    GlitchViewSubmenu,
    GlitchViewVarList,
    GlitchViewWidget,
    GlitchViewTrigger,
    GlitchViewSweep,
    GlitchViewWiring,
} GlitchViewId;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    /* shared modules */
    Submenu* submenu;
    VariableItemList* var_item_list;
    Widget* widget;

    /* custom views */
    TriggerView* trigger_view;
    SweepView* sweep_view;
    WiringView* wiring_view;

    /* the instrument */
    GlitchEngine* engine;
    GlitchParams params;

    /* settings */
    bool sound;
    bool vibro;
    bool led;
} GlitchApp;

/* feedback, all gated by settings (defined in glitch_trigger.c) */
void glitch_notify_fire(GlitchApp* app);
void glitch_notify_arm(GlitchApp* app);
void glitch_notify_disarm(GlitchApp* app);
void glitch_notify_hit(GlitchApp* app);
