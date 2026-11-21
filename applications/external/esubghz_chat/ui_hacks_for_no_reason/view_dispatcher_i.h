/* NOT a firmware header - app-local copy of a firmware private header. It must
 * stay byte-identical to the firmware's. See the sync notes at the top of
 * view_port_i.h before changing anything in this directory.
 */
/**
 * @file view_dispatcher_i.h
 * GUI: ViewDispatcher API
 */

#pragma once

#include <m-dict.h>
#include <stddef.h>

#include <gui/view_dispatcher.h>
#include "view_i.h"
#include "gui_i.h"

DICT_DEF2(ViewDict, uint32_t, M_DEFAULT_OPLIST, View*, M_PTR_OPLIST) // NOLINT

struct ViewDispatcher {
    bool is_event_loop_owned;
    FuriEventLoop* event_loop;
    FuriMessageQueue* input_queue;
    FuriMessageQueue* event_queue;

    Gui* gui;
    ViewPort* view_port;
    ViewDict_t views;

    View* current_view;

    struct Loading* loading; //<! Built-in loading view, allocated on first use

    View* ongoing_input_view;
    uint8_t ongoing_input;

    ViewDispatcherCustomEventCallback custom_event_callback;
    ViewDispatcherNavigationEventCallback navigation_event_callback;
    ViewDispatcherTickEventCallback tick_event_callback;
    uint32_t tick_period;
    void* event_context;

    FuriMessageQueue* ascii_queue;
};

_Static_assert(offsetof(struct ViewDispatcher, gui) == 16, "RM ViewDispatcher.gui layout changed");
_Static_assert(
    offsetof(struct ViewDispatcher, view_port) == 20,
    "RM ViewDispatcher.view_port layout changed");
_Static_assert(
    offsetof(struct ViewDispatcher, current_view) == 48,
    "RM ViewDispatcher.current_view layout changed");
