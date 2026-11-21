/* NOT a firmware header. This is an app-local copy of the firmware's private
 * applications/services/gui/view_port_i.h, kept because struct ViewPort is
 * opaque in the public FAP API while this app needs to read
 * view_port->draw_callback and ->input_callback in order to hook them.
 *
 * This struct MUST stay byte-identical to the firmware's. The FAP API version
 * check does not protect it: these structs are not part of the API symbol
 * table, so a firmware layout change still compiles cleanly here and only
 * shows up at runtime, as a callback pointer read from the wrong offset. That
 * is not theoretical - a missing `bool was_drawn` put draw_callback at offset
 * 12 instead of 16 and MemManage-faulted the app at launch.
 *
 * Re-diff every file in this directory against the firmware source on each API
 * bump. Never drop or reorder a field because it looks unused here - the
 * offsets are the whole point.
 */
/**
 * @file view_port_i.h
 * GUI: internal ViewPort API
 */

#pragma once

#include "gui_i.h"
#include <stddef.h>
#include <gui/view_port.h>

struct ViewPort {
    Gui* gui;
    FuriMutex* mutex;
    bool is_enabled;
    // Set by view_port_draw(), cleared by view_port_update(): while something else covers this
    // view port, its update requests would repaint the view port on top of it instead
    bool was_drawn;
    ViewPortOrientation orientation;

    uint8_t width;
    uint8_t height;

    ViewPortDrawCallback draw_callback;
    void* draw_callback_context;

    ViewPortInputCallback input_callback;
    void* input_callback_context;

    ViewPortAsciiCallback ascii_callback;
    void* ascii_callback_context;
};

_Static_assert(
    offsetof(struct ViewPort, draw_callback) == 16,
    "RM ViewPort.draw_callback layout changed");
_Static_assert(
    offsetof(struct ViewPort, input_callback) == 24,
    "RM ViewPort.input_callback layout changed");
_Static_assert(sizeof(struct ViewPort) == 40, "RM ViewPort size changed");
