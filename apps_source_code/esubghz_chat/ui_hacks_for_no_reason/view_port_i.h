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
 * Last synced: Unleashed dev 000b238e and Official 1.5.1, verified field by
 * field against each firmware.elf's DWARF (2026-11). Unleashed carries a
 * `bool was_drawn` that the Official firmware does not have, which moves every
 * field after it, so the layout is split on FW_ORIGIN below. Forks are NOT
 * covered: a Momentum or RogueMaster build must be re-verified before being
 * added to the split, because guessing here crashes the app at launch.
 *
 * Last synced (older note): Unleashed dev, FAP API 88.25, SDK build 1521.
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
#include <gui/view_port.h>
#include <stddef.h>

struct ViewPort {
    Gui* gui;
    FuriMutex* mutex;
    bool is_enabled;
#ifndef FW_ORIGIN_Official
    /* Set by view_port_draw(), cleared by view_port_update(). Unleashed only;
     * the Official firmware has no such field. */
    bool was_drawn;
#endif
    ViewPortOrientation orientation;

    uint8_t width;
    uint8_t height;

    ViewPortDrawCallback draw_callback;
    void* draw_callback_context;

    ViewPortInputCallback input_callback;
    void* input_callback_context;
};

/* draw_callback and input_callback are the only fields this app reads out of a
 * firmware-owned ViewPort. Fail the build instead of the device if the copy
 * above is ever edited into a different layout. */
#ifdef FW_ORIGIN_Official
_Static_assert(
    offsetof(struct ViewPort, draw_callback) == 12,
    "struct ViewPort layout drifted: re-sync against the Official firmware");
_Static_assert(
    offsetof(struct ViewPort, input_callback) == 20,
    "struct ViewPort layout drifted: re-sync against the Official firmware");
_Static_assert(
    sizeof(struct ViewPort) == 28,
    "struct ViewPort layout drifted: re-sync against the Official firmware");
#else
_Static_assert(
    offsetof(struct ViewPort, draw_callback) == 16,
    "struct ViewPort layout drifted: re-sync against the Unleashed firmware");
_Static_assert(
    offsetof(struct ViewPort, input_callback) == 24,
    "struct ViewPort layout drifted: re-sync against the Unleashed firmware");
_Static_assert(
    sizeof(struct ViewPort) == 32,
    "struct ViewPort layout drifted: re-sync against the Unleashed firmware");
#endif
