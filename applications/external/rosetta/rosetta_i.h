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

#include "rosetta_icons.h" // generated from icons/ by fbt

#include "helpers/protocols.h"
#include "helpers/nfc_reader.h"
#include "helpers/onewire_reader.h"
#include "helpers/rf_scope.h"
#include "views/lesson_view.h"
#include "views/capture_view.h"
#include "views/scope_view.h"
#include "scenes/rosetta_scene.h"

#define ROSETTA_VERSION "1.0"

typedef enum {
    RosettaViewSubmenu,
    RosettaViewSettings,
    RosettaViewWidget,
    RosettaViewLesson,
    RosettaViewCapture,
    RosettaViewScope,
} RosettaViewId;

typedef enum {
    RosettaCustomEventCaptureReady = 100, // a live reader produced a result
    RosettaCustomEventCaptureRescan, // user asked to capture again
} RosettaCustomEvent;

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
    LessonView* lesson_view;
    CaptureView* capture_view;
    ScopeView* scope_view;

    /* live readers (one active at a time) */
    NfcReader* nfc;
    OneWireReader* onewire;
    RfScope* rf;

    /* navigation state */
    RosettaProtocol protocol; // protocol selected in the main menu

    /* settings */
    bool sound;
    bool vibro;
    bool led;
    uint8_t rf_freq_index; // index into the Sub-GHz frequency preset table
} RosettaApp;

/* feedback (defined in rosetta.c), all gated by settings */
void rosetta_notify_capture(RosettaApp* app, bool good);
void rosetta_notify_blip(RosettaApp* app);
